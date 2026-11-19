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
#include <list>

#include "../../Character/Look/CharLook.h"
#include "../Components/Charset.h"
#include "../Components/NameTag.h"
#include "../UIElement.h"

namespace ms {
// The character selection screen
class UICharSelect : public UIElement {
public:
    static constexpr Type TYPE = UIElement::Type::CHAR_SELECT;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = false;

    UICharSelect(std::vector<CharEntry> characters,
                 int8_t characters_count,
                 int32_t slots,
                 int8_t require_pic);

    void draw(float inter) const override;

    void update() override;

    void doubleclick(Point<int16_t> cursorpos) override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

    void add_character(CharEntry &&character);

    void post_add_character();

    void remove_character(int32_t id);

    const CharEntry &get_character(int32_t id);

    bool update_character(int32_t id, const StatsEntry &stats);

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    void update_buttons();

    void update_selected_character();

    void select_last_slot();

    // maple-offline: v83 layout (characters stand on the bridge of MapLogin's
    // character-select stop; Start/Create/Delete hang on the signboard).
    Point<int16_t> slot_feet(size_t slot) const;

    std::string get_infolabel(size_t index, const StatsEntry &stats) const;

    void request_pic();

    void check_pic(const std::string &entered_pic) const;

    static constexpr uint8_t PAGESIZE_ = 3;

    enum Buttons : uint16_t {
        CHARACTER_SELECT,
        CHARACTER_NEW,
        CHARACTER_DELETE,
        PAGE_LEFT,
        PAGE_RIGHT,
        BACK,
        CHARACTER_SLOT0
    };

    std::vector<CharEntry> characters_;
    int8_t characters_count_;
    int32_t slots_;
    int8_t require_pic_;
    Text version_;
    uint8_t selected_character_;
    uint8_t selected_page;
    uint8_t page_count_;
    Texture world_plaque_;
    Text world_label_;
    Text channel_label_;
    Point<int16_t> scroll_pos_;
    Animation scroll_open_;
    Texture scroll_full_;
    bool scroll_done_;
    Texture charinfo_;
    nl::node nametag_;
    std::vector<CharLook> char_looks_;
    std::vector<NameTag> nametags_;
    Animation empty_slot_glow_;
    Texture empty_slot_;
    Animation selected_effect_;
    Animation selected_beam_;

    enum InfoLabel : uint8_t { JOB, LEVEL, FAME, STR, INT, DEX, LUK, RANK, NUM_LABELS };

    std::array<Text, UICharSelect::InfoLabel::NUM_LABELS> info_labels_;
};
}  // namespace ms