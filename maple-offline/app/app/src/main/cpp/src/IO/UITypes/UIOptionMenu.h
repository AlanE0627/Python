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

#include "../UIDragElement.h"

namespace ms {
// maple-offline: v83 option windows. game_option = UIWindow.img/GameOpt
// (social permissions, informational in single-player); otherwise
// UIWindow.img/SysOpt with working BGM / sound volume and mute.
class UIOptionMenu : public UIDragElement<PosOPTIONMENU> {
public:
    static constexpr Type TYPE = UIElement::Type::OPTION_MENU;
    static constexpr bool FOCUSED = true;
    static constexpr bool TOGGLED = false;

    explicit UIOptionMenu(bool game_option = false);

    void draw(float inter) const override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    enum Buttons : uint16_t { OK, CANCEL };

    void apply(bool save);
    void draw_bar(int16_t y, uint8_t volume, bool muted) const;

    bool game_option_;
    Texture backgrnd_;
    uint8_t bgm_, sfx_, bgm_old_, sfx_old_;
    bool bgm_mute_, sfx_mute_;
    bool dragging_bgm_, dragging_sfx_;
    // maple-offline: HP / MP ALERT rows drive the built-in auto potion
    uint8_t hp_alert_ = 0, mp_alert_ = 0, hp_alert_old_ = 0, mp_alert_old_ = 0;
    bool dragging_hp_ = false, dragging_mp_ = false;
    std::vector<Text> labels_;
};
}  // namespace ms
