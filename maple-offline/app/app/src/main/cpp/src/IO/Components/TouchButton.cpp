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
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>

#include "StringHandling.h"
#include "TouchButton.h"
#include "UI.h"
#include "Stage.h"

namespace ms {
    TouchButton::TouchButton(Point<int16_t> position, ActionType action_type,
                             const std::string &text) :
            position_(position),
            background_(100, 100, Color::Name::BLACK, 0.535f),
            bind_key_(GLFMKeyCodeUnknown),
            action_type_(action_type),
            text_(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::YELLOW) {
        text_.change_text(text);
        bind_touch_id_ = -1;
    }

    TouchButton::TouchButton(Point<int16_t> position, ActionType action_type, GLFMKeyCode bind_key,
                             const std::string &text) :
            position_(position),
            background_(100, 100, Color::Name::BLACK, 0.535f),
            bind_key_(bind_key),
            action_type_(action_type),
            text_(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::YELLOW) {
        text_.change_text(text);
        bind_touch_id_ = -1;
    }

    TouchButton::TouchButton(Point<int16_t> position, int16_t size, GLFMKeyCode bind_key,
                             const std::string &text) :
            position_(position),
            background_(size, size, Color::Name::BLACK, 0.4f),
            bind_key_(bind_key),
            action_type_(ActionType::Default),
            text_(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE),
            size_(size) {
        text_.change_text(text);
        bind_touch_id_ = -1;
    }

    void TouchButton::draw() const {
        background_.draw(position_);
        text_.draw(position_ + Point<int16_t>(size_ / 2, size_ / 2 - 8));
    }

    void TouchButton::update() {
        const std::unordered_map<int16_t, TouchInfo> &touch_phase_map = UI::get().get_touch_phase();
        auto it = touch_phase_map.find(bind_touch_id_);
        if (it != touch_phase_map.end()) {
            GLFMTouchPhase current_phase = it->second.phase;
            if (current_phase == GLFMTouchPhaseBegan ||
                current_phase == GLFMTouchPhaseMoved) {
                if (action_type_ == ActionType::Jump) {
                    Stage::get().get_player().send_action(KeyAction::Id::JUMP, true);
                } else if (action_type_ == ActionType::Potion) {

                } else if (action_type_ == ActionType::Skill) {

                } else if (!held_) {
                    held_ = true;
                    UI::get().send_key(bind_key_, true);
                }
            } else if (current_phase == GLFMTouchPhaseEnded
                       || current_phase == GLFMTouchPhaseCancelled) {
                UI::get().remove_touch_phase(bind_touch_id_);
                bind_touch_id_ = -1;
                if (action_type_ == ActionType::Jump) {
                    Stage::get().get_player().send_action(KeyAction::Id::JUMP, false);
                } else if (action_type_ == ActionType::Potion) {

                } else if (action_type_ == ActionType::Skill) {

                } else {
                    held_ = false;
                    UI::get().send_key(bind_key_, false);
                }
            }
        }
    }

    bool TouchButton::set_state(TouchInfo touchInfo) {
        if (touchInfo.relative_pos.x() > position_.x() &&
            touchInfo.relative_pos.x() < (position_.x() + size_) &&
            touchInfo.relative_pos.y() > position_.y() &&
            touchInfo.relative_pos.y() < (position_.y() + size_)) {
            return true;
        }
        return false;
    }

    void TouchButton::bind_touch_id(int16_t touch_id) {
        bind_touch_id_ = touch_id;
    }

    int16_t TouchButton::get_bind_touch_id() {
        return bind_touch_id_;
    }

}  // namespace ms