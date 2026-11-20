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

#include "../../Character/CharStats.h"
#include "../../Graphics/Text.h"
#include "../Components/Charset.h"
#include "../UIElement.h"

namespace ms {
// maple-offline: v83 status bar (UI.wz/StatusBar.img). The 800 px v83 bar is
// laid out on the wide screen with the chat/level/gauge part on the left and
// the button block anchored to the right edge.
class UIStatusBar : public UIElement {
public:
    static constexpr Type TYPE = UIElement::Type::STATUS_BAR;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = true;

    enum MenuType { MENU, SETTING, COMMUNITY, CHARACTER, EVENT };

    UIStatusBar(const CharStats &stats, uint8_t channel_count);

    void draw(float alpha) const override;

    void update() override;

    bool is_in_range(Point<int16_t> cursorpos) const override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

    void toggle_qs();

    void toggle_menu();

    void remove_menus();

    bool is_menu_active();

    // maple-offline: the MENU popup or the SHORTCUT (quick slot) panel is
    // open; both sit under the touch buttons, which hide meanwhile
    bool is_popup_open() const { return menu_active_ || quickslot_active_; }

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    float exp_percent() const;

    void draw_gauge(int16_t x0, int16_t x1, float percent) const;

    void draw_numbers(const std::string &text, Point<int16_t> center) const;

    enum Buttons : uint16_t {
        BT_SHOP,
        BT_MENU,
        BT_SHORTCUT,
        BT_TRADE,
        BT_KEYSET,
        BT_STAT,
        BT_EQUIP,
        BT_INVEN,
        BT_SKILL,
        BT_QUICKSLOT,
        BT_QUICKSLOT_DOWN,
        // v83 game menu popup (UIWindow.img/GameMenu)
        BT_GM_CHANNEL,
        BT_GM_GAMEOPT,
        BT_GM_SYSOPT,
        BT_GM_SKIN,
        BT_GM_QUIT
    };

    const CharStats &stats_;
    uint8_t channel_count_;
    int16_t VWIDTH_;
    int16_t VHEIGHT_;
    Point<int16_t> bar_pos_;
    Texture backgrnd_;
    Texture backgrnd2_;
    Texture gauge_bar_;
    Texture gauge_gray_;
    Texture graduation_;
    Texture quickslot_panel_;
    Charset numbers_;
    Text level_text_;
    Text job_text_;
    Text name_text_;
    bool quickslot_active_;
    bool menu_active_ = false;
    Texture menu_backgrnd_;
    Point<int16_t> menu_pos_;
};
}  // namespace ms
