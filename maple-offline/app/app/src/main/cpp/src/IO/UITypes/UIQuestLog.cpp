//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "UIQuestLog.h"

#include <nlnx/nx.hpp>

#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/NpcInteractionPackets.h"
#include "../../Util/MapleText.h"
#include "../../Graphics/Geometry.h"
#include <algorithm>

#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

namespace ms {
UIQuestLog::UIQuestLog(const QuestLog &ql) :
    UIDragElement<PosQUEST>(Point<int16_t>(245, 20)),
    questlog_(ql),
    tab_(TAB1),
    offset_(0),
    selected_(-1),
    has_reward_(false),
    desc_scroll_(0) {
    nl::node quest = nl::nx::ui["UIWindow.img"]["Quest"];
    nl::node backgrnd = quest["backgrnd"];

    sprites_.emplace_back(backgrnd);

    nl::node taben = quest["Tab"]["enabled"];
    nl::node tabdis = quest["Tab"]["disabled"];

    for (uint16_t i = 0; i < 3; i++) {
        std::string k = std::to_string(i);
        Texture en = taben[k];
        int16_t center = 42 + i * 80;

        buttons_[TAB0 + i] = std::make_unique<TwoSpriteButton>(
            tabdis[k], taben[k],
            Point<int16_t>(center - en.width() / 2, 28));
    }

    buttons_[CLOSE] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtClose"],
                                                    Point<int16_t>(226, 6));

    // give-up sits on the detail panel's bottom bar
    buttons_[GIVEUP] = std::make_unique<MapleButton>(
        quest["BtGiveup"], Point<int16_t>(LIST_W_ + DETAIL_W_ - 70, 373));
    buttons_[GIVEUP]->set_active(false);

    icon_ = quest["icon1"];
    detail_bg_ = quest["backgrnd2"];
    detail_bg_reward_ = quest["backgrnd4"];
    reward_label_ = quest["reward"];

    empty_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::DUSTYGRAY, "No quests.");
    detail_name_ = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", 160);
    detail_npc_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "", 100);
    detail_level_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "", 100);
    detail_desc_ = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::DARKGREY, "", 272);

    slider_ = Slider(Slider::Type::LINE_CYAN,
                     Range<int16_t>(LIST_Y_, LIST_Y_ + ROWS_ * ROW_HEIGHT_),
                     226,
                     ROWS_,
                     1,
                     [&](bool upwards) {
                         int16_t shift = upwards ? -1 : 1;
                         int16_t next = offset_ + shift;

                         if (next >= 0 && next + ROWS_ <= static_cast<int16_t>(rows_.size())) {
                             offset_ = next;
                         }
                     });

    change_tab(TAB1);

    dimension_ = Texture(backgrnd).get_dimensions();
    drag_area_ = Point<int16_t>(dimension_.x(), 20);
}

bool UIQuestLog::can_start(int16_t qid) const {
    if (questlog_.is_started(qid) || questlog_.is_completed(qid)) {
        return false;
    }

    nl::node check = nl::nx::quest["Check.img"][std::to_string(qid)]["0"];

    // only quests handed out by an NPC show up as available
    if (static_cast<int32_t>(check["npc"]) == 0) {
        return false;
    }

    // dated event quests (all from 2008-2010) are long expired and the
    // server refuses them, so they are never offered
    if (check["end"].data_type() != nl::node::type::none) {
        return false;
    }

    const Player &player = Stage::get().get_player();
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

        if (state == 2 && !questlog_.is_completed(pre)) {
            return false;
        }

        if (state == 1 && !questlog_.is_started(pre)) {
            return false;
        }

        if (state == 0 && (questlog_.is_started(pre) || questlog_.is_completed(pre))) {
            return false;
        }
    }

    return true;
}

void UIQuestLog::load_list() {
    rows_.clear();
    qids_.clear();
    offset_ = 0;

    auto add = [&](int16_t qid) {
        std::string name = nl::nx::quest["QuestInfo.img"][std::to_string(qid)]["name"];

        if (name.empty()) {
            return;
        }

        qids_.push_back(qid);
        rows_.emplace_back(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK,
                           MapleText::strip(name), 190);
    };

    if (tab_ == TAB0) {
        for (nl::node info : nl::nx::quest["QuestInfo.img"]) {
            int32_t qid = std::atoi(info.name().c_str());

            if (qid > 0 && qid < 32768 && can_start(static_cast<int16_t>(qid))) {
                add(static_cast<int16_t>(qid));
            }
        }
    } else if (tab_ == TAB1) {
        for (const auto &[qid, data] : questlog_.get_started()) {
            add(qid);
        }
    } else if (tab_ == TAB2) {
        for (const auto &[qid, time] : questlog_.get_completed()) {
            add(qid);
        }
    }

    slider_.setrows(ROWS_, static_cast<int16_t>(rows_.size()));

    if (selected_ >= 0
        && std::find(qids_.begin(), qids_.end(), selected_) == qids_.end()) {
        select(-1);
    } else {
        select(selected_);
    }
}

void UIQuestLog::refresh() {
    int16_t keep = offset_;
    load_list();

    if (keep + ROWS_ <= static_cast<int16_t>(rows_.size())) {
        offset_ = keep;
    }
}

void UIQuestLog::select(int16_t qid) {
    selected_ = qid;
    buttons_[GIVEUP]->set_active(qid >= 0 && tab_ == TAB1);

    if (qid < 0) {
        dimension_ = Point<int16_t>(LIST_W_, dimension_.y());
        return;
    }

    dimension_ = Point<int16_t>(LIST_W_ + DETAIL_W_, dimension_.y());

    std::string sid = std::to_string(qid);
    nl::node info = nl::nx::quest["QuestInfo.img"][sid];
    nl::node check = nl::nx::quest["Check.img"][sid];
    nl::node act = nl::nx::quest["Act.img"][sid]["1"];

    detail_name_.change_text(MapleText::strip(info["name"]));

    int32_t npc = check["0"]["npc"];

    if (tab_ != TAB0 && static_cast<int32_t>(check["1"]["npc"]) != 0) {
        npc = check["1"]["npc"];
    }

    std::string npcname = npc ? static_cast<std::string>(
            nl::nx::string["Npc.img"][std::to_string(npc)]["name"]) : "";
    detail_npc_.change_text(npcname);

    int32_t lvmin = check["0"]["lvmin"];
    detail_level_.change_text(lvmin > 0 ? "Lv. " + std::to_string(lvmin) : "");

    // description for the current state: 0 before start, 1 in progress,
    // 2 after completion (falls back to the earlier text if missing)
    std::string key = tab_ == TAB0 ? "0" : (tab_ == TAB1 ? "1" : "2");
    std::string desc = info[key];

    if (desc.empty()) {
        desc = static_cast<std::string>(info["0"]);
    }

    detail_desc_.change_text(MapleText::format(desc));

    desc_scroll_ = 0;
    reward_lines_.clear();

    auto add_reward = [&](const std::string &line) {
        reward_lines_.emplace_back(Text::Font::A11M, Text::Alignment::LEFT,
                                   Color::Name::DARKGREY, line, 250);
    };

    int32_t exp = act["exp"];
    int32_t meso = act["money"];
    int32_t fame = act["pop"];

    if (exp > 0) {
        add_reward("EXP +" + std::to_string(exp));
    }

    if (meso > 0) {
        add_reward("Mesos +" + std::to_string(meso));
    }

    if (fame > 0) {
        add_reward("Fame +" + std::to_string(fame));
    }

    for (nl::node item : act["item"]) {
        int32_t count = item["count"];
        int32_t id = item["id"];

        if (count <= 0 || id == 0) {
            continue;
        }

        std::string name = MapleText::strip("#t" + std::to_string(id) + "#");

        if (name.empty()) {
            continue;
        }

        // items with a probability are a random pick, so mark them
        bool random = item["prop"].data_type() != nl::node::type::none
                      && static_cast<int32_t>(item["prop"]) > 0;
        add_reward(name + (count > 1 ? " x" + std::to_string(count) : "")
                   + (random ? " (random)" : ""));
    }

    has_reward_ = !reward_lines_.empty();
}

void UIQuestLog::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    if (rows_.empty()) {
        empty_.draw(position_ + Point<int16_t>(122, LIST_Y_ + 10));
    }

    for (int16_t i = 0; i < ROWS_; i++) {
        int16_t idx = offset_ + i;

        if (idx >= static_cast<int16_t>(rows_.size())) {
            break;
        }

        Point<int16_t> row = position_ + Point<int16_t>(12, LIST_Y_ + i * ROW_HEIGHT_);

        if (qids_[idx] == selected_) {
            ColorBox(214, ROW_HEIGHT_, Color::Name::MEDIUMBLUE, 0.25f)
                .draw(DrawArgument(row + Point<int16_t>(-4, -1)));
        }

        icon_.draw(row + Point<int16_t>(0, 1));
        rows_[idx].draw(row + Point<int16_t>(18, 0));
    }

    if (rows_.size() > static_cast<size_t>(ROWS_)) {
        slider_.draw(position_);
    }

    if (selected_ >= 0) {
        Point<int16_t> dp = position_ + Point<int16_t>(LIST_W_, 0);

        (has_reward_ ? detail_bg_reward_ : detail_bg_).draw(DrawArgument(dp));

        detail_name_.draw(dp + Point<int16_t>(26, 38));
        detail_npc_.draw(dp + Point<int16_t>(244, 40));
        detail_level_.draw(dp + Point<int16_t>(244, 62));
        // description clipped to its box; the mouse wheel scrolls it
        int16_t top = dp.y() + DESC_TOP_;
        int16_t bottom = dp.y() + (has_reward_ ? DESC_BOTTOM_REWARD_ : DESC_BOTTOM_);
        detail_desc_.draw(DrawArgument(dp + Point<int16_t>(18, DESC_TOP_ - desc_scroll_)),
                          Range<int16_t>(top, bottom));

        if (has_reward_) {
            reward_label_.draw(DrawArgument(dp + Point<int16_t>(22, 252)));

            for (size_t i = 0; i < reward_lines_.size() && i < 5; i++) {
                reward_lines_[i].draw(dp + Point<int16_t>(30, 274 + static_cast<int16_t>(i) * 17));
            }
        }
    }

    UIElement::draw_buttons(alpha);
}

void UIQuestLog::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (escape) {
            if (selected_ >= 0) {
                select(-1);
            } else {
                deactivate();
            }
        } else if (keycode == KeyAction::Id::TAB) {
            change_tab(tab_ < TAB2 ? tab_ + 1 : TAB0);
        }
    }
}

bool UIQuestLog::is_in_range(Point<int16_t> cursorpos) const {
    Rectangle<int16_t> bounds(position_, position_ + dimension_);

    return bounds.contains(cursorpos);
}

void UIQuestLog::send_scroll(double yoffset) {
    if (selected_ < 0) {
        return;
    }

    Point<int16_t> rel = last_cursor_ - position_;

    if (rel.x() < LIST_W_ || rel.x() >= LIST_W_ + DETAIL_W_) {
        return;
    }

    int16_t visible = (has_reward_ ? DESC_BOTTOM_REWARD_ : DESC_BOTTOM_) - DESC_TOP_;
    int16_t max_scroll = std::max<int16_t>(0, detail_desc_.height() - visible + 6);
    int16_t next = desc_scroll_ - static_cast<int16_t>(yoffset * 18);

    desc_scroll_ = std::clamp<int16_t>(next, 0, max_scroll);
}

Cursor::State UIQuestLog::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    last_cursor_ = cursorpos;

    if (rows_.size() > static_cast<size_t>(ROWS_)) {
        if (Cursor::State state = slider_.send_cursor(cursorpos - position_, clicking)) {
            return state;
        }
    }

    Point<int16_t> rel = cursorpos - position_;

    if (clicking && rel.x() >= 8 && rel.x() < 222 && rel.y() >= LIST_Y_
        && rel.y() < LIST_Y_ + ROWS_ * ROW_HEIGHT_) {
        int16_t idx = offset_ + (rel.y() - LIST_Y_) / ROW_HEIGHT_;

        if (idx < static_cast<int16_t>(qids_.size())) {
            select(qids_[idx] == selected_ ? -1 : qids_[idx]);
            return Cursor::State::CLICKING;
        }
    }

    // touch / controller cursor: a click in the description box pages it
    // (upper half back, lower half forward)
    if (clicking && selected_ >= 0 && rel.x() >= LIST_W_ + 10
        && rel.x() < LIST_W_ + DETAIL_W_ - 10) {
        int16_t bottom = has_reward_ ? DESC_BOTTOM_REWARD_ : DESC_BOTTOM_;

        if (rel.y() >= DESC_TOP_ && rel.y() < bottom) {
            int16_t page = (bottom - DESC_TOP_) / 2;
            send_scroll(rel.y() < (DESC_TOP_ + bottom) / 2 ? page / 18.0 : -page / 18.0);

            return Cursor::State::CLICKING;
        }
    }

    return UIDragElement::send_cursor(clicking, cursorpos);
}

UIElement::Type UIQuestLog::get_type() const {
    return TYPE;
}

Button::State UIQuestLog::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case TAB0:
        case TAB1:
        case TAB2: change_tab(buttonid); return Button::State::IDENTITY;
        case CLOSE: deactivate(); break;
        case GIVEUP:
            if (selected_ >= 0 && tab_ == TAB1) {
                ForfeitQuestPacket(selected_).dispatch();
            }

            break;
        default: break;
    }

    return Button::State::NORMAL;
}

void UIQuestLog::change_tab(uint16_t tabid) {
    for (uint16_t i = TAB0; i <= TAB2; i++) {
        buttons_[i]->set_state(i == tabid ? Button::State::PRESSED : Button::State::NORMAL);
    }

    tab_ = tabid;
    selected_ = -1;
    load_list();
}
}  // namespace ms
