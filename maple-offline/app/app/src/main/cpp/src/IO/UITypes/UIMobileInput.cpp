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

#include "../Data/ItemData.h"
#include "StringHandling.h"
#include "UIMobileInput.h"
#include "UI.h"
#include "TouchButton.h"
#include "UIStateGame.h"

extern "C" int maple_gamepad_active;

namespace ms {
    // maple-offline: wide-screen layout above the status bar (logical
    // 1300x600; the status bar starts at y ~529). Stick on the left thumb;
    // attack / jump / pick-up and four skill keys on the right thumb. The
    // skill buttons send Ins / Home / PgUp / Del, the classic v83 skill keys,
    // so skills are bound to them in the key config window; a controller's
    // L1 / R1 / L2 / R2 send the same keys.
    UIMobileInput::UIMobileInput() :
    joystick_(Point<int16_t>(140, 430), 90) {
        const int16_t s = 80;
        touch_buttons_[MobileButtons::ButtonNormal0] =
                std::make_unique<TouchButton>(Point<int16_t>(930, 440), s, GLFMKeyCodeInsert, "Ins");
        touch_buttons_[MobileButtons::ButtonNormal1] =
                std::make_unique<TouchButton>(Point<int16_t>(1020, 440), s, GLFMKeyCodeHome, "Home");
        touch_buttons_[MobileButtons::ButtonJump] =
                std::make_unique<TouchButton>(Point<int16_t>(1110, 440), s, GLFMKeyCodeAltLeft, "점프");
        touch_buttons_[MobileButtons::ButtonNormal2] =
                std::make_unique<TouchButton>(Point<int16_t>(1200, 440), s, GLFMKeyCodeControlLeft, "공격");
        touch_buttons_[MobileButtons::ButtonNormal3] =
                std::make_unique<TouchButton>(Point<int16_t>(1020, 350), s, GLFMKeyCodePageUp, "PgUp");
        touch_buttons_[MobileButtons::ButtonNormal4] =
                std::make_unique<TouchButton>(Point<int16_t>(1110, 350), s, GLFMKeyCodeDelete, "Del");
        touch_buttons_[MobileButtons::ButtonNormal5] =
                std::make_unique<TouchButton>(Point<int16_t>(1200, 350), s, GLFMKeyCodeZ, "줍기");
    }

    void UIMobileInput::draw() const {
        // maple-offline: hide the touch overlay while a controller is in use.
        if (maple_gamepad_active) return;
        if (UI::get().get_state_type() == typeid(UIStateGame)) {
            joystick_.draw();
            for (const auto &iter: touch_buttons_) {
                if (const TouchButton *button = iter.second.get()) {
                    button->draw();
                }
            }
        }
    }

    void UIMobileInput::update() {
        if (maple_gamepad_active) return;
        if (UI::get().get_state_type() == typeid(UIStateGame)) {
            const std::unordered_map<int16_t, TouchInfo> &touch_phase_map = UI::get().get_touch_phase();
            for (const auto &iter: touch_buttons_) {
                if (TouchButton *button = iter.second.get()) {
                    for (const auto &touch_phase: touch_phase_map) {
                        if (button->set_state(touch_phase.second))
                            button->bind_touch_id(touch_phase.first);
                    }
                }
            }

            for (const auto &touch_phase: touch_phase_map) {
                if (joystick_.set_state(touch_phase.second)) {
                    joystick_.bind_touch_id(touch_phase.first);
                }
            }

            for (const auto &iter: touch_buttons_) {
                if (TouchButton *button = iter.second.get()) {
                    button->update();
                }
            }

            joystick_.update();
        }
    }

    std::map<uint16_t, std::unique_ptr<TouchButton>>& UIMobileInput::getTouchButtons() {
        return touch_buttons_;
    }

    VirtualJoyStick& UIMobileInput::getVirtualJoyStick() {
        return joystick_;
    }
}  // namespace ms