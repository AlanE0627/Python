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

#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

namespace ms {
UIQuestLog::UIQuestLog(const QuestLog &ql) :
    UIDragElement<PosQUEST>(Point<int16_t>(245, 20)),
    questlog_(ql),
    tab_(TAB1),
    offset_(0) {
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

    icon_ = quest["icon1"];

    empty_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::DUSTYGRAY, "No quests.");

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

void UIQuestLog::load_list() {
    rows_.clear();
    offset_ = 0;

    auto add = [&](int16_t qid) {
        std::string name = nl::nx::quest["QuestInfo.img"][std::to_string(qid)]["name"];

        if (name.empty()) {
            name = "Quest " + std::to_string(qid);
        }

        rows_.emplace_back(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, name, 190);
    };

    if (tab_ == TAB1) {
        for (const auto &[qid, data] : questlog_.get_started()) {
            add(qid);
        }
    } else if (tab_ == TAB2) {
        for (const auto &[qid, time] : questlog_.get_completed()) {
            add(qid);
        }
    }
    // TAB0 (available) needs the quest requirement checks; filled later.

    slider_.setrows(ROWS_, static_cast<int16_t>(rows_.size()));
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

        icon_.draw(row + Point<int16_t>(0, 1));
        rows_[idx].draw(row + Point<int16_t>(18, 0));
    }

    if (rows_.size() > static_cast<size_t>(ROWS_)) {
        slider_.draw(position_);
    }

    UIElement::draw_buttons(alpha);
}

void UIQuestLog::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (escape) {
            deactivate();
        } else if (keycode == KeyAction::Id::TAB) {
            change_tab(tab_ < TAB2 ? tab_ + 1 : TAB0);
        }
    }
}

Cursor::State UIQuestLog::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    if (rows_.size() > static_cast<size_t>(ROWS_)) {
        if (Cursor::State state = slider_.send_cursor(cursorpos - position_, clicking)) {
            return state;
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
        default: break;
    }

    return Button::State::NORMAL;
}

void UIQuestLog::change_tab(uint16_t tabid) {
    for (uint16_t i = TAB0; i <= TAB2; i++) {
        buttons_[i]->set_state(i == tabid ? Button::State::PRESSED : Button::State::NORMAL);
    }

    tab_ = tabid;
    load_list();
}
}  // namespace ms
