#include "QuestTalk.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlnx/nx.hpp>

#include "../IO/UI.h"
#include "../IO/UITypes/UINpcTalk.h"
#include "../Net/Packets/NpcInteractionPackets.h"
#include "../Util/QuestCheck.h"
#include "Stage.h"

namespace ms::QuestTalk {
namespace {
enum Style : int8_t { OK, NEXT, NEXT_PREV, ACCEPT };

constexpr int32_t TALK_OPTION = 99;

// Reuses the open window: replacing it would destroy the element whose
// button handler is calling us.
std::optional<std::reference_wrapper<UINpcTalk>> window() {
    if (auto talk = UI::get().get_element<UINpcTalk>()) {
        talk->get().makeactive();
        return talk;
    }

    UI::get().emplace<UINpcTalk>();

    return UI::get().get_element<UINpcTalk>();
}

void show(int32_t npcid, const std::string &text, Style style,
          UINpcTalk::LocalResponder responder) {
    if (auto talk = window()) {
        switch (style) {
            case OK: talk->get().change_text(npcid, 0, 0, 0, 0, text); break;
            case NEXT: talk->get().change_text(npcid, 0, 0, 1, 0, text); break;
            case NEXT_PREV: talk->get().change_text(npcid, 0, 1, 1, 0, text); break;
            case ACCEPT: talk->get().change_text(npcid, 12, 0, 0, 0, text); break;
        }

        talk->get().set_local(std::move(responder));
    }
}

void show_menu(int32_t npcid, const std::string &text,
               UINpcTalk::LocalResponder responder) {
    if (auto talk = window()) {
        talk->get().change_text(npcid, 4, 0, 0, 0, text);
        talk->get().set_local(std::move(responder));
    }
}

std::vector<std::string> pages(nl::node node) {
    std::vector<std::string> out;

    for (int i = 0;; i++) {
        nl::node page = node[std::to_string(i)];

        if (page.data_type() != nl::node::type::string) {
            break;
        }

        out.push_back(page);
    }

    return out;
}

// Walks a list of pages with next/prev; the last page uses `last` and
// calls `done(action)` with its answer.
void run_pages(int32_t npcid, std::shared_ptr<std::vector<std::string>> list,
               size_t index, Style last, std::function<void(int8_t)> done) {
    if (list->empty()) {
        done(1);
        return;
    }

    bool is_last = index + 1 >= list->size();
    Style style = is_last ? last : (index == 0 ? NEXT : NEXT_PREV);

    show(npcid, (*list)[index], style,
         [=](int8_t action, int32_t) {
             if (is_last) {
                 done(action);
             } else if (action == 1) {
                 run_pages(npcid, list, index + 1, last, done);
             } else if (action == 0 && index > 0) {
                 run_pages(npcid, list, index - 1, last, done);
             }
         });
}

void start_quest(int32_t npcid, int16_t qid) {
    std::string sid = std::to_string(qid);
#ifdef MS_DESKTOP_DEBUG
    fprintf(stderr, "[dbg] start_quest %d npc %d\n", qid, npcid);
#endif
    nl::node check = nl::nx::quest["Check.img"][sid]["0"];

    if (check["startscript"].data_type() == nl::node::type::string) {
        // the server script runs the whole conversation
        QuestActionPacket(4, qid, npcid).dispatch();
        return;
    }

    nl::node say = nl::nx::quest["Say.img"][sid]["0"];
    auto list = std::make_shared<std::vector<std::string>>(pages(say));

    if (list->empty()) {
        QuestActionPacket(1, qid, npcid).dispatch();
        return;
    }

    run_pages(npcid, list, 0, ACCEPT, [=](int8_t action) {
        if (action == 1) {
            QuestActionPacket(1, qid, npcid).dispatch();

            auto yes = std::make_shared<std::vector<std::string>>(pages(say["yes"]));

            if (!yes->empty()) {
                run_pages(npcid, yes, 0, OK, [](int8_t) {});
            }
        } else if (action == 0) {
            auto no = std::make_shared<std::vector<std::string>>(pages(say["no"]));

            if (!no->empty()) {
                run_pages(npcid, no, 0, OK, [](int8_t) {});
            }
        }
    });
}

void complete_quest(int32_t npcid, int16_t qid) {
    std::string sid = std::to_string(qid);
    nl::node check = nl::nx::quest["Check.img"][sid]["1"];

    if (check["endscript"].data_type() == nl::node::type::string) {
        QuestActionPacket(5, qid, npcid).dispatch();
        return;
    }

    nl::node say = nl::nx::quest["Say.img"][sid]["1"];
    auto list = std::make_shared<std::vector<std::string>>(pages(say));

    if (list->empty()) {
        QuestActionPacket(2, qid, npcid).dispatch();
        return;
    }

    run_pages(npcid, list, 0, OK, [=](int8_t action) {
        if (action == 1) {
            QuestActionPacket(2, qid, npcid).dispatch();
        }
    });
}
}  // namespace

bool open(int32_t npcid, int32_t oid, bool scripted) {
    std::vector<int16_t> available;
    std::vector<int16_t> completable;
    Player &player = Stage::get().get_player();

    for (nl::node info : nl::nx::quest["QuestInfo.img"]) {
        int32_t id = std::atoi(info.name().c_str());

        if (id <= 0 || id >= 32768) {
            continue;
        }

        auto qid = static_cast<int16_t>(id);
        nl::node check = nl::nx::quest["Check.img"][info.name()];

        if (player.get_quests().is_started(qid)) {
            if (static_cast<int32_t>(check["1"]["npc"]) == npcid
                && QuestCheck::can_complete(qid)) {
                completable.push_back(qid);
            }
        } else if (static_cast<int32_t>(check["0"]["npc"]) == npcid
                   && QuestCheck::can_start(qid)) {
            available.push_back(qid);
        }
    }

    if (available.empty() && completable.empty()) {
        return false;
    }

    std::string text = "What do you want to do?\r\n";
    std::vector<std::pair<int16_t, bool>> options;  // qid, complete?

    auto name_of = [](int16_t qid) {
        return static_cast<std::string>(
                nl::nx::quest["QuestInfo.img"][std::to_string(qid)]["name"]);
    };

    if (!completable.empty()) {
        text += "#r[Quest can be completed]#k\r\n";

        for (int16_t qid : completable) {
            text += "#L" + std::to_string(options.size()) + "##b" + name_of(qid) + "#k#l\r\n";
            options.emplace_back(qid, true);
        }
    }

    if (!available.empty()) {
        text += "#r[Quest available]#k\r\n";

        for (int16_t qid : available) {
            text += "#L" + std::to_string(options.size()) + "##b" + name_of(qid) + "#k#l\r\n";
            options.emplace_back(qid, false);
        }
    }

    if (scripted) {
        text += "#L" + std::to_string(TALK_OPTION) + "#Talk#l";
    }

    show_menu(npcid, text, [=](int8_t action, int32_t selection) {
        if (selection == TALK_OPTION) {
            TalkToNPCPacket(oid).dispatch();
            return;
        }

        if (selection < 0 || selection >= static_cast<int32_t>(options.size())) {
            return;
        }

        auto [qid, complete] = options[selection];

        if (complete) {
            complete_quest(npcid, qid);
        } else {
            start_quest(npcid, qid);
        }
    });

    return true;
}
}  // namespace ms::QuestTalk
