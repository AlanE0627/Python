#include <cstdio>
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
#include "UIRaceSelect.h"

#include <cctype>
#include <nlnx/nx.hpp>

#include "../Audio/Audio.h"
#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../Configuration.h"
#include "../UI.h"
#include "UIAranCreation.h"
#include "UICharSelect.h"
#include "UICygnusCreation.h"
#include "UIExplorerCreation.h"
#include "UILoginNotice.h"
#include "../../LoginScene.h"

namespace ms {
namespace {
// Frame-relative layout (800x600). The three group pictures stand in a row
// in the sky, the description board sits under them, and the Select button
// fills the empty slot drawn inside the description board.
const Point<int16_t> RACE_POS[3] = { Point<int16_t>(68, 92),
                                     Point<int16_t>(298, 94),
                                     Point<int16_t>(524, 91) };
const Point<int16_t> TITLE_POS = Point<int16_t>(400, 56);
const Point<int16_t> DESC_POS = Point<int16_t>(130, 382);
const Point<int16_t> SELECT_OFF = Point<int16_t>(449, 124);
}  // namespace

UIRaceSelect::UIRaceSelect() :
    UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600)) {
    LoginScene::get().set_step(LoginScene::RACE_SELECT);
    position_ = LoginScene::get().frame_origin();

    nl::node Login = nl::nx::ui["Login.img"];
    nl::node Common = Login["Common"];
    nl::node RaceSelect = Login["RaceSelect"];

    sprites_.emplace_back(Common["frame"], Point<int16_t>(400, 300));
    sprites_.emplace_back(Common["step"]["3"], Point<int16_t>(40, 0));

    title_ = RaceSelect["textGL"]["0"];

    const char *const keys[NUM_RACES] = { "normal", "knight", "aran" };
    const char *const buttons[NUM_RACES] = { "BtNormal", "BtKnight", "BtAran" };

    for (uint8_t i = 0; i < NUM_RACES; i++) {
        nl::node race = RaceSelect[keys[i]];

        race_pos_[i] = RACE_POS[i];
        on_animation_[i] = race["OnAnimation"];
        description_[i] = race["text"]["0"];

        buttons_[Buttons::RACE0 + i] =
            std::make_unique<MapleButton>(race[buttons[i]], race_pos_[i]);
    }

    buttons_[Buttons::BACK] =
        std::make_unique<MapleButton>(Common["BtStart"], Point<int16_t>(0, 515));
    buttons_[Buttons::SELECT] = std::make_unique<MapleButton>(
        RaceSelect["BtSelect"],
        DESC_POS + SELECT_OFF);

    selected_race_ = EXPLORER;
    select_race(EXPLORER);

    dimension_ = Point<int16_t>(800, 600);
}

void UIRaceSelect::draw(float inter) const {
    UIElement::draw_sprites(inter);

    title_.draw(position_ + TITLE_POS);

    for (uint8_t i = 0; i < NUM_RACES; i++) {
        if (i == selected_race_) {
            on_animation_[i].draw(position_ + race_pos_[i], inter);
        }
    }

    description_[selected_race_].draw(position_ + DESC_POS);

    UIElement::draw_buttons(inter);
}

void UIRaceSelect::update() {
    UIElement::update();

    on_animation_[selected_race_].update();
}

Cursor::State UIRaceSelect::send_cursor(bool clicked,
                                        Point<int16_t> cursorpos) {
    Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

    for (auto &[btnid, button] : buttons_) {
        bool is_race = btnid >= Buttons::RACE0;

        if (button->is_active()
            && button->bounds(position_).contains(cursorpos)) {
            if (button->get_state() == Button::State::NORMAL) {
                Sound(Sound::Name::BUTTON_OVER).play();

                button->set_state(Button::State::MOUSEOVER);
                ret = Cursor::State::CAN_CLICK;
            } else if (button->get_state() == Button::State::MOUSEOVER
                       || (is_race && button->get_state() == Button::State::PRESSED)) {
                if (clicked) {
                    Sound(Sound::Name::BUTTON_CLICK).play();

                    button->set_state(button_pressed(btnid));

                    ret = Cursor::State::IDLE;
                } else {
                    ret = Cursor::State::CAN_CLICK;
                }
            }
        } else if (button->get_state() == Button::State::MOUSEOVER) {
            button->set_state(Button::State::NORMAL);
        }
    }

    return ret;
}

void UIRaceSelect::send_key(int32_t keycode, bool pressed, bool escape) {
    if (!pressed) {
        return;
    }

    if (escape) {
        show_charselect();
    } else if (keycode == KeyAction::Id::LEFT) {
        select_race((selected_race_ + NUM_RACES - 1) % NUM_RACES);
    } else if (keycode == KeyAction::Id::RIGHT) {
        select_race((selected_race_ + 1) % NUM_RACES);
    } else if (keycode == KeyAction::Id::RETURN) {
        start_creation();
    }
}

UIElement::Type UIRaceSelect::get_type() const {
    return TYPE;
}

bool UIRaceSelect::check_name(const std::string &name) const {
    nl::node ForbiddenName = nl::nx::etc["ForbiddenName.img"];
    std::string lName = to_lower(name);

    for (const auto &forbiddenName : ForbiddenName) {
        std::string fName = to_lower(forbiddenName);

        if (!fName.empty() && lName.find(fName) != std::string::npos) {
            return false;
        }
    }

    return true;
}

void UIRaceSelect::send_naming_result(bool nameused) {
    if (selected_race_ == EXPLORER) {
        if (auto creation = UI::get().get_element<UIExplorerCreation>()) {
            creation->get().send_naming_result(nameused);
        }
    } else if (selected_race_ == CYGNUSKNIGHTS) {
        if (auto creation = UI::get().get_element<UICygnusCreation>()) {
            creation->get().send_naming_result(nameused);
        }
    } else if (selected_race_ == ARAN) {
        if (auto creation = UI::get().get_element<UIAranCreation>()) {
            creation->get().send_naming_result(nameused);
        }
    }
}

Button::State UIRaceSelect::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::BACK: show_charselect(); break;
        case Buttons::SELECT: start_creation(); break;
        default:
            if (buttonid >= Buttons::RACE0) {
                uint8_t race = buttonid - Buttons::RACE0;

                if (race == selected_race_) {
                    start_creation();
                } else {
                    select_race(race);
                }

                // The selected group is shown by its OnAnimation instead.
                return Button::State::PRESSED;
            }
            break;
    }

    return Button::State::NORMAL;
}

void UIRaceSelect::select_race(uint8_t race) {
    if (race != selected_race_) {
        Sound(Sound::Name::RACE_SELECT).play();
    }

    selected_race_ = race;
    on_animation_[race].reset();

    // v83: the grey picture of the selected group is hidden ("pressed" is
    // an empty 1x1 frame) and its coloured OnAnimation drawn in its place.
    for (uint8_t i = 0; i < NUM_RACES; i++) {
        buttons_[Buttons::RACE0 + i]->set_state(
            i == race ? Button::State::PRESSED : Button::State::NORMAL);
    }
}

void UIRaceSelect::start_creation() {
    Sound(Sound::Name::SCROLL_UP).play();

    deactivate();

    switch (selected_race_) {
        case EXPLORER: UI::get().emplace<UIExplorerCreation>(); break;
        case CYGNUSKNIGHTS: UI::get().emplace<UICygnusCreation>(); break;
        case ARAN: UI::get().emplace<UIAranCreation>(); break;
        default: break;
    }
}

void UIRaceSelect::show_charselect() {
    Sound(Sound::Name::SCROLL_UP).play();

    deactivate();

    if (auto charselect = UI::get().get_element<UICharSelect>()) {
        charselect->get().makeactive();
        LoginScene::get().set_step(LoginScene::CHAR_SELECT);
    }
}

std::string UIRaceSelect::to_lower(std::string value) const {
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    return value;
}
}  // namespace ms
