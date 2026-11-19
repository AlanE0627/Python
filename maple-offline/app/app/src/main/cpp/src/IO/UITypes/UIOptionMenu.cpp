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
#include "UIOptionMenu.h"

#include <nlnx/nx.hpp>

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Graphics/Geometry.h"
#include "../Components/MapleButton.h"
#include "../KeyAction.h"

namespace ms {
namespace {
// SysOpt rows (backgrnd 299x366): BGM and SOUND value strips.
constexpr int16_t BGM_Y = 70;
constexpr int16_t SFX_Y = 100;
constexpr int16_t HP_Y = 188;
constexpr int16_t MP_Y = 220;
constexpr int16_t BAR_X0 = 66;
constexpr int16_t BAR_X1 = 238;
constexpr int16_t MUTE_X0 = 244;
constexpr int16_t MUTE_X1 = 290;
}  // namespace

UIOptionMenu::UIOptionMenu(bool game_option) :
    UIDragElement<PosOPTIONMENU>(),
    game_option_(game_option),
    dragging_bgm_(false),
    dragging_sfx_(false) {
    nl::node win = nl::nx::ui_img("UIWindow.img")[game_option_ ? "GameOpt" : "SysOpt"];
    backgrnd_ = win["backgrnd"];

    bgm_ = bgm_old_ = Setting<BGMVolume>::get().load();
    sfx_ = sfx_old_ = Setting<SFXVolume>::get().load();
    hp_alert_ = hp_alert_old_ = Setting<AutoHpPercent>::get().load();
    mp_alert_ = mp_alert_old_ = Setting<AutoMpPercent>::get().load();
    bgm_mute_ = bgm_ == 0;
    sfx_mute_ = sfx_ == 0;

    if (bgm_mute_) bgm_ = 50;
    if (sfx_mute_) sfx_ = 50;

    Point<int16_t> dim = backgrnd_.get_dimensions();
    nl::node basic = nl::nx::ui_img("Basic.img");

    buttons_[OK] = std::make_unique<MapleButton>(basic["BtOK2"], Point<int16_t>(dim.x() - 104, dim.y() - 23));
    buttons_[CANCEL] = std::make_unique<MapleButton>(basic["BtCancel2"], Point<int16_t>(dim.x() - 54, dim.y() - 23));

    if (game_option_) {
        // Single-player: there is nobody to send invites; everything is "allowed".
        buttons_[CANCEL]->set_active(false);
        buttons_[OK]->set_position(Point<int16_t>(dim.x() / 2 - 23, dim.y() - 23));
    }

    dimension_ = dim;
    drag_area_ = Point<int16_t>(dim.x(), 20);
}

void UIOptionMenu::draw_bar(int16_t y, uint8_t volume, bool muted) const {
    int16_t width = BAR_X1 - BAR_X0;
    int16_t filled = muted ? 0 : static_cast<int16_t>(width * volume / 100);

    ColorBox(width, 6, Color::Name::DUSTYGRAY, 0.6f).draw(DrawArgument(position_ + Point<int16_t>(BAR_X0, y + 8)));
    ColorBox(filled, 6, Color::Name::MEDIUMBLUE, 0.9f).draw(DrawArgument(position_ + Point<int16_t>(BAR_X0, y + 8)));
    ColorBox(4, 14, Color::Name::BLACK, 0.8f)
        .draw(DrawArgument(position_ + Point<int16_t>(BAR_X0 + filled - 2, y + 4)));

    if (muted) {
        ColorBox(MUTE_X1 - MUTE_X0, 18, Color::Name::RED, 0.25f)
            .draw(DrawArgument(position_ + Point<int16_t>(MUTE_X0, y + 2)));
    }
}

void UIOptionMenu::draw(float inter) const {
    backgrnd_.draw(position_);

    if (!game_option_) {
        draw_bar(BGM_Y, bgm_, bgm_mute_);
        draw_bar(SFX_Y, sfx_, sfx_mute_);
        draw_bar(HP_Y, hp_alert_, false);
        draw_bar(MP_Y, mp_alert_, false);
    }

    UIElement::draw_buttons(inter);
}

void UIOptionMenu::apply(bool save) {
    uint8_t bgm = bgm_mute_ ? 0 : bgm_;
    uint8_t sfx = sfx_mute_ ? 0 : sfx_;

    Music::set_bgmvolume(bgm);
    Sound::set_sfxvolume(sfx);

    if (save) {
        Setting<BGMVolume>::get().save(bgm);
        Setting<SFXVolume>::get().save(sfx);
        Setting<AutoHpPercent>::get().save(hp_alert_);
        Setting<AutoMpPercent>::get().save(mp_alert_);
        hp_alert_old_ = hp_alert_;
        mp_alert_old_ = mp_alert_;
        bgm_old_ = bgm;
        sfx_old_ = sfx;

        // Settings are otherwise only written at exit, which Android may skip.
        Configuration::get().save();
    }
}

Cursor::State UIOptionMenu::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    if (!game_option_) {
        Point<int16_t> local = cursorpos - position_;
        auto in_row = [&](int16_t y) { return local.y() >= y && local.y() < y + 26; };
        auto to_volume = [&](int16_t x) {
            int16_t v = static_cast<int16_t>(100 * (x - BAR_X0) / (BAR_X1 - BAR_X0));
            return static_cast<uint8_t>(std::max<int16_t>(0, std::min<int16_t>(100, v)));
        };

        if (!clicked) {
            dragging_bgm_ = dragging_sfx_ = dragging_hp_ = dragging_mp_ = false;
        }

        bool on_bar = local.x() >= BAR_X0 - 4 && local.x() <= BAR_X1 + 4;
        bool on_mute = local.x() >= MUTE_X0 && local.x() <= MUTE_X1;

        if (clicked && in_row(BGM_Y) && on_mute) {
            bgm_mute_ = !bgm_mute_;
            apply(false);
            return Cursor::State::CLICKING;
        }

        if (clicked && in_row(SFX_Y) && on_mute) {
            sfx_mute_ = !sfx_mute_;
            apply(false);
            return Cursor::State::CLICKING;
        }

        if ((clicked && in_row(HP_Y) && on_bar) || dragging_hp_) {
            dragging_hp_ = clicked;
            hp_alert_ = to_volume(local.x());
            return Cursor::State::CLICKING;
        }

        if ((clicked && in_row(MP_Y) && on_bar) || dragging_mp_) {
            dragging_mp_ = clicked;
            mp_alert_ = to_volume(local.x());
            return Cursor::State::CLICKING;
        }

        if ((clicked && in_row(BGM_Y) && on_bar) || dragging_bgm_) {
            dragging_bgm_ = clicked;
            bgm_ = to_volume(local.x());
            bgm_mute_ = false;
            apply(false);
            return Cursor::State::CLICKING;
        }

        if ((clicked && in_row(SFX_Y) && on_bar) || dragging_sfx_) {
            dragging_sfx_ = clicked;
            sfx_ = to_volume(local.x());
            sfx_mute_ = false;
            apply(false);
            return Cursor::State::CLICKING;
        }
    }

    return UIDragElement::send_cursor(clicked, cursorpos);
}

void UIOptionMenu::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (escape) {
            button_pressed(CANCEL);
        } else if (keycode == KeyAction::Id::RETURN) {
            button_pressed(OK);
        }
    }
}

UIElement::Type UIOptionMenu::get_type() const {
    return TYPE;
}

Button::State UIOptionMenu::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case OK:
            if (!game_option_) {
                apply(true);
            }
            deactivate();
            break;
        case CANCEL:
            if (!game_option_) {
                Music::set_bgmvolume(bgm_old_);
                Sound::set_sfxvolume(sfx_old_);
            }
            deactivate();
            break;
        default: break;
    }

    return Button::State::NORMAL;
}
}  // namespace ms
