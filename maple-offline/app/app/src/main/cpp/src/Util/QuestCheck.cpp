#include "QuestCheck.h"

#include <chrono>
#include <climits>
#include <cstdio>
#include <map>
#include <string>
#include <nlnx/nx.hpp>

#include "../Gameplay/Stage.h"

namespace ms::QuestCheck {
namespace {
bool has(nl::node n) {
    return n.data_type() != nl::node::type::none || n.size() > 0;
}
}  // namespace

bool can_start(int16_t qid) {
    const Player &player = Stage::get().get_player();
    const QuestLog &log = const_cast<Player &>(player).get_quests();

    if (log.is_started(qid) || log.is_completed(qid)) {
        return false;
    }

    nl::node check = nl::nx::quest["Check.img"][std::to_string(qid)]["0"];

    // only quests handed out by an NPC count
    if (static_cast<int32_t>(check["npc"]) == 0) {
        return false;
    }

    // dated event quests (2008-2010) are long expired; the server
    // refuses them
    if (has(check["end"])) {
        return false;
    }

    int32_t level = player.get_level();
    int32_t lvmin = check["lvmin"];
    int32_t lvmax = check["lvmax"];

    if (level < lvmin || (lvmax > 0 && level > lvmax)) {
        return false;
    }

    nl::node jobs = check["job"];

    if (jobs.size() > 0) {
        int32_t job = player.get_stats().get_job().get_id();
        bool ok = false;

        for (nl::node j : jobs) {
            if (static_cast<int32_t>(j) == job) {
                ok = true;
                break;
            }
        }

        if (!ok) {
            return false;
        }
    }

    for (nl::node q : check["quest"]) {
        int16_t pre = static_cast<int16_t>(static_cast<int32_t>(q["id"]));
        int32_t state = q["state"];

        if (state == 2 && !log.is_completed(pre)) {
            return false;
        }

        if (state == 1 && !log.is_started(pre)) {
            return false;
        }

        if (state == 0 && (log.is_started(pre) || log.is_completed(pre))) {
            return false;
        }
    }

    return true;
}

bool can_complete(int16_t qid) {
    Player &player = Stage::get().get_player();
    const QuestLog &log = player.get_quests();

    auto iter = log.get_started().find(qid);

    if (iter == log.get_started().end()) {
        return false;
    }

    nl::node check = nl::nx::quest["Check.img"][std::to_string(qid)]["1"];

    if (static_cast<int32_t>(check["npc"]) == 0) {
        return false;
    }

    for (nl::node item : check["item"]) {
        int32_t id = item["id"];
        int32_t count = item["count"];

        if (count > 0 && player.get_inventory().get_total_item_count(id) < count) {
            return false;
        }
    }

    // kill counts: the progress string holds three digits per mob entry
    const std::string &data = iter->second;
    size_t index = 0;

    for (nl::node mob : check["mob"]) {
        int32_t need = mob["count"];
        int32_t have = 0;

        if (data.size() >= (index + 1) * 3) {
            have = std::atoi(data.substr(index * 3, 3).c_str());
        }

        if (have < need) {
            return false;
        }

        index++;
    }

    return true;
}

int8_t npc_icon(int32_t npcid) {
    static std::map<int32_t, int8_t> icons;
    static uint32_t built_revision = UINT32_MAX;
    static int32_t built_level = -1;
    static int32_t built_job = -1;
    static auto built_at = std::chrono::steady_clock::time_point();

    Player &player = Stage::get().get_player();
    uint32_t revision = player.get_quests().revision();
    int32_t level = player.get_level();
    int32_t job = player.get_stats().get_job().get_id();
    // item pickups can make a turn-in ready without a quest record change,
    // so the map is also rebuilt every few seconds
    auto now = std::chrono::steady_clock::now();
    bool stale = now - built_at > std::chrono::seconds(3);

    if (revision != built_revision || level != built_level || job != built_job || stale) {
        icons.clear();

        for (nl::node info : nl::nx::quest["QuestInfo.img"]) {
            int32_t id = std::atoi(info.name().c_str());

            if (id <= 0 || id >= 32768) {
                continue;
            }

            auto qid = static_cast<int16_t>(id);
            std::string sid = info.name();

            if (player.get_quests().is_started(qid)) {
                if (can_complete(qid)) {
                    int32_t npc = nl::nx::quest["Check.img"][sid]["1"]["npc"];
                    icons[npc] = 2;
                }
            } else if (can_start(qid)) {
                int32_t npc = nl::nx::quest["Check.img"][sid]["0"]["npc"];

                if (icons.find(npc) == icons.end()) {
                    icons[npc] = 0;
                }
            }
        }

        built_revision = revision;
        built_level = level;
        built_job = job;
        built_at = now;
#ifdef MS_DESKTOP_DEBUG
        static size_t last_count = SIZE_MAX;
        if (icons.size() != last_count) {
            last_count = icons.size();
            std::string list;
            for (auto &[n, t] : icons) list += " " + std::to_string(n) + ":" + std::to_string(t);
            printf("[dbg] quest icons%s\n", list.c_str());
            fflush(stdout);
        }
#endif
    }

    auto it = icons.find(npcid);

    return it == icons.end() ? -1 : it->second;
}
}  // namespace ms::QuestCheck
