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
#pragma once

#include <array>

#include "../Components/Textfield.h"
#include "../Template/BoolPair.h"
#include "../UIElement.h"

namespace ms {
// maple-offline: v83 race select (Login.img/RaceSelect) — Explorers,
// Cygnus Knights and Aran, shown over the open sky stop of MapLogin.
class UIRaceSelect : public UIElement {
public:
    static constexpr Type TYPE = UIElement::Type::RACE_SELECT;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = false;

    UIRaceSelect();

    void draw(float inter) const override;

    void update() override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

    bool check_name(const std::string &name) const;

    void send_naming_result(bool nameused);

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    void select_race(uint8_t race);

    void start_creation();

    void show_charselect();

    std::string to_lower(std::string value) const;

    // Same order as the v83 buttons, left to right.
    enum Race : uint8_t { EXPLORER, CYGNUSKNIGHTS, ARAN, NUM_RACES };

    enum Buttons : uint16_t { BACK, SELECT, RACE0, RACE1, RACE2,
                              KMS_DUAL, KMS_EVAN, CONFIRM_OK, CONFIRM_CANCEL };

    // KMS 1.2.95 layout (Login.img/RaceSelect has BtDual): five cards, the
    // races this v83 server cannot create (Dual Blade, Evan) shown disabled,
    // and a confirm box before creation.
    bool kms_ = false;
    bool confirm_open_ = false;
    Texture confirm_bg_;
    std::array<Texture, 5> confirm_race_;
    void open_confirm(bool open);

    std::array<Animation, NUM_RACES> on_animation_;
    std::array<Texture, NUM_RACES> description_;
    std::array<Point<int16_t>, NUM_RACES> race_pos_;
    Texture title_;
    uint8_t selected_race_;
};
}  // namespace ms
