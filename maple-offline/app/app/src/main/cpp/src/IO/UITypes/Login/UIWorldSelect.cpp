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
#include "UIWorldSelect.h"
#include <cstdio>

#include "../../LoginScene.h"
#include "../../../Graphics/Geometry.h"

#include <nlnx/nx.hpp>

#include "../../Audio/Audio.h"
#include "../../Net/Packets/LoginPackets.h"
#include "../../Util/Randomizer.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "../UI.h"
#include "UILoginNotice.h"
#include "UILoginWait.h"
#include "UIRegion.h"

namespace ms {
namespace {
auto fn_charlist_req = [](auto&&... args) {
    CharlistRequestPacket(std::forward<decltype(args)>(args)...).dispatch();
};

auto fn_server_status_req = [](auto&&... args) {
    ServerStatusRequestPacket(std::forward<decltype(args)>(args)...).dispatch();
};
}  // namespace

UIWorldSelect::UIWorldSelect() :
    UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600)) {
    world_count_ = 0;
    recommended_world_count_ = 0;
    recommended_worldid_ = 0;
    world_selected_ = false;
    use_recommended_ = false;
    show_recommended_ = false;
    draw_chatballoon_ = true;

    std::string version_text = Configuration::get().get_version();
    version_ = Text(Text::Font::A11M,
                    Text::Alignment::LEFT,
                    Color::Name::LEMONGRASS,
                    "Ver. " + version_text);

    recommended_message_ = Text(Text::Font::A11M,
                                Text::Alignment::CENTER,
                                Color::Name::JAMBALAYA,
                                "",
                                100,
                                true,
                                5);

    // maple-offline: v83 world select. Scene (city, signboards) comes from MapLogin.img.
    LoginScene::get().set_step(LoginScene::WORLD_SELECT);
    position_ = LoginScene::get().frame_origin();
    channel_src_pos_ = Point<int16_t>(430, 318);

    worldid_ = Setting<DefaultWorld>::get().load();
    channelid_ = Setting<DefaultChannel>::get().load();
    uint8_t regionid = Setting<DefaultRegion>::get().load();

    nl::node login = nl::nx::ui_img("Login.img");
    world_select_ = login["WorldSelect"];
    world_src_ = world_select_["BtWorld"];
    channel_src_ = world_select_["channel"];
    nl::node common = login["Common"];

    // KMS 1.2.95 world select (Login.img/Title/signboard marks the KMS set):
    // world list board at the top, channel board 371x222 with 5 columns.
    kms_ = static_cast<bool>(login["Title"]["signboard"]);
    if (kms_) {
        panel_tl_ = Point<int16_t>(218, 198);
        // channel cells of the board grid: first cell x 19, y 89, 66x28 pitch
        ch_off_ = Point<int16_t>(22, 92);
        ch_step_ = Point<int16_t>(66, 28);
        ch_cols_ = 5;
        wname_off_ = Point<int16_t>(8, 8);
        go_off_ = Point<int16_t>(229, 15);
        ch_box_ = Point<int16_t>(60, 21);
        channel_src_pos_ = panel_tl_;   // chBackgrn origin is (0,0)
    } else {
        panel_tl_ = channel_src_pos_ - Point<int16_t>(224, 116);
    }

    set_region(regionid);

    sprites_.emplace_back(common["frame"], Point<int16_t>(400, 300));
    sprites_.emplace_back(common["step"]["1"], Point<int16_t>(40, 0));

    buttons_[Buttons::BT_VIEW_ALL] =
        std::make_unique<MapleButton>(world_select_["BtViewAll"], Point<int16_t>(0, 53));
    buttons_[Buttons::BT_VIEW_RECOMMENDED] =
        std::make_unique<MapleButton>(world_select_["BtViewChoice"], Point<int16_t>(0, 53));
    buttons_[Buttons::BT_VIEW_RECOMMENDED_SELECT] =
        std::make_unique<MapleButton>(world_select_["alert"]["BtChoice"], Point<int16_t>(349, 327));
    buttons_[Buttons::BT_VIEW_RECOMMENDED_CANCEL] =
        std::make_unique<MapleButton>(world_select_["alert"]["BtClose"], Point<int16_t>(407, 327));
    buttons_[Buttons::BT_VIEW_RECOMMENDED_PREV] =
        std::make_unique<MapleButton>(world_select_["alert"]["BtArrowL"], Point<int16_t>(338, 244));
    buttons_[Buttons::BT_VIEW_RECOMMENDED_NEXT] =
        std::make_unique<MapleButton>(world_select_["alert"]["BtArrowR"], Point<int16_t>(439, 244));
    buttons_[Buttons::BT_VIEW_ALL]->set_active(false);
    buttons_[Buttons::BT_VIEW_RECOMMENDED]->set_active(false);
    buttons_[Buttons::BT_VIEW_RECOMMENDED_SELECT]->set_active(false);
    buttons_[Buttons::BT_VIEW_RECOMMENDED_CANCEL]->set_active(false);
    buttons_[Buttons::BT_VIEW_RECOMMENDED_PREV]->set_active(false);
    buttons_[Buttons::BT_VIEW_RECOMMENDED_NEXT]->set_active(false);
    recommended_textures_.emplace_back(world_select_["alert"]["backgrd"]);

    // Region switching does not exist in v83; keep the slot inactive.
    buttons_[Buttons::BT_CHANGE_REGION] =
        std::make_unique<MapleButton>(world_select_["BtRegion"], Point<int16_t>(3, 127));
    buttons_[Buttons::BT_CHANGE_REGION]->set_active(false);
    buttons_[Buttons::BT_QUIT_GAME] =
        std::make_unique<MapleButton>(common["BtExit"], Point<int16_t>(0, 515));

    // Channel panel (v83: chBackgrn centred, origin 224,116, on channel_src_pos_).
    const Point<int16_t> panel_tl = panel_tl_;
    for (size_t i = 0; i < Buttons::BT_ENTER_WORLD - Buttons::BT_CHANNEL0; i++) {
        buttons_[Buttons::BT_CHANNEL0 + i] = std::make_unique<MapleButton>(
            channel_src_[std::to_string(i)], panel_tl + channel_cell(i));
        buttons_[Buttons::BT_CHANNEL0 + i]->set_active(false);
    }

    channels_background_ = world_select_["chBackgrn"];

    buttons_[Buttons::BT_ENTER_WORLD] = std::make_unique<MapleButton>(
        world_select_["BtGoworld"], panel_tl + go_off_);
    buttons_[Buttons::BT_ENTER_WORLD]->set_active(false);

    chatballoon_.change_text(
        "Please select the World you would like to play in.");

    if (Configuration::get().get_auto_login()) {
        auto world = Configuration::get().get_auto_world();
        auto channel = Configuration::get().get_auto_channel();

        Configuration::get().set_worldid(world);
        Configuration::get().set_channelid(channel);

        UI::get().emplace<UILoginWait>();
        auto loginwait = UI::get().get_element<UILoginWait>();

        if (loginwait && loginwait->get().is_active()) {
            fn_charlist_req(world, channel);
        }
    }
}

void UIWorldSelect::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    worlds_background_.draw(position_ + world_src_pos_);

    if (show_recommended_) {
        recommended_textures_[0].draw(position_ + Point<int16_t>(302, 152));
        recommended_world_textures_[recommended_worldid_].draw(
            position_ + Point<int16_t>(336, 187));
        recommended_message_.draw(position_ + Point<int16_t>(401, 259));
    }

    if (world_selected_) {
        channels_background_.draw(position_ + channel_src_pos_);
        world_textures_[worldid_].draw(position_ + panel_tl_ + wname_off_);
    }

    UIElement::draw_buttons(alpha);

    if (world_selected_) {
        // maple-offline: v83 has no "selected" channel art; mark it with a light box.
        ColorBox(ch_box_.x(), ch_box_.y(), Color::Name::WHITE, 0.35f)
            .draw(DrawArgument(position_ + panel_tl_ + channel_cell(channelid_)));
    }

    version_.draw(position_ + Point<int16_t>(707, 1));

    if (draw_chatballoon_ && !kms_) {
        chatballoon_.draw(position_ + Point<int16_t>(500, 80));
    }
}

Cursor::State UIWorldSelect::send_cursor(bool clicked,
                                         Point<int16_t> cursorpos) {
    // maple-offline: v83 chBackgrn is centred on channel_src_pos_ (origin 224,116).
    const Point<int16_t> panel_tl = position_ + channel_src_pos_ - channels_background_.get_origin();
    Rectangle<int16_t> channels_bounds = Rectangle<int16_t>(
        panel_tl, panel_tl + channels_background_.get_dimensions());

    Rectangle<int16_t> worlds_bounds = Rectangle<int16_t>(
        position_ + world_src_pos_,
        position_ + world_src_pos_ + worlds_background_.get_dimensions());

    if (world_selected_ && !channels_bounds.contains(cursorpos)
        && !worlds_bounds.contains(cursorpos)) {
        if (clicked) {
            world_selected_ = false;
            clear_selected_world();
        }
    }

    Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;
#ifdef MS_DESKTOP_DEBUG
    if (clicked) {
        auto b = buttons_[Buttons::BT_ENTER_WORLD]->bounds(position_);
        std::fprintf(stderr, "[dbg] click %d,%d enter lt %d,%d rb %d,%d active %d sel %d\n", cursorpos.x(), cursorpos.y(),
            b.left(), b.top(), b.right(), b.bottom(), buttons_[Buttons::BT_ENTER_WORLD]->is_active(), world_selected_);
    }
#endif

    for (auto &[btnid, button] : buttons_) {
        if (button->is_active()
            && button->bounds(position_).contains(cursorpos)) {
            if (button->get_state() == Button::State::NORMAL) {
                Sound(Sound::Name::BUTTON_OVER).play();

                button->set_state(Button::State::MOUSEOVER);
                ret = Cursor::State::CAN_CLICK;
            } else if (button->get_state() == Button::State::PRESSED) {
                if (clicked) {
                    Sound(Sound::Name::BUTTON_CLICK).play();

                    button->set_state(button_pressed(btnid));

                    ret = Cursor::State::IDLE;
                } else {
                    ret = Cursor::State::CAN_CLICK;
                }
            } else if (button->get_state() == Button::State::MOUSEOVER) {
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

void UIWorldSelect::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (world_selected_) {
            World selectedWorld = worlds_[worldid_];

            uint8_t selected_channel = channelid_;
            uint8_t channel_total = selectedWorld.channelcount;

            uint8_t COLUMNS = 5;
            uint8_t columns = std::min(channel_total, COLUMNS);

            uint8_t rows = std::floor((channel_total - 1) / COLUMNS) + 1;

            div_t div = std::div(selected_channel, columns);
            auto current_col = div.rem;
            // auto current_row = div.quot;

            if (keycode == KeyAction::Id::UP) {
                auto next_channel =
                    (selected_channel - COLUMNS < 0
                         ? (selected_channel - COLUMNS) + rows * COLUMNS
                         : selected_channel - COLUMNS);

                if (next_channel == channelid_) {
                    return;
                }

                if (next_channel > channel_total) {
                    button_pressed(next_channel - COLUMNS
                                   + Buttons::BT_CHANNEL0);
                } else {
                    button_pressed(next_channel + Buttons::BT_CHANNEL0);
                }
            } else if (keycode == KeyAction::Id::DOWN) {
                auto next_channel = (selected_channel + COLUMNS >= channel_total
                                         ? current_col
                                         : selected_channel + COLUMNS);

                if (next_channel == channelid_) {
                    return;
                }

                if (next_channel > channel_total) {
                    button_pressed(next_channel + COLUMNS
                                   + Buttons::BT_CHANNEL0);
                } else {
                    button_pressed(next_channel + Buttons::BT_CHANNEL0);
                }
            } else if (keycode == KeyAction::Id::LEFT
                       || keycode == KeyAction::Id::TAB) {
                if (selected_channel != 0) {
                    selected_channel--;
                } else {
                    selected_channel = channel_total - 1;
                }

                button_pressed(selected_channel + Buttons::BT_CHANNEL0);
            } else if (keycode == KeyAction::Id::RIGHT) {
                if (selected_channel != channel_total - 1) {
                    selected_channel++;
                } else {
                    selected_channel = 0;
                }

                button_pressed(selected_channel + Buttons::BT_CHANNEL0);
            } else if (escape) {
                world_selected_ = false;

                clear_selected_world();
            } else if (keycode == KeyAction::Id::RETURN) {
                button_pressed(Buttons::BT_ENTER_WORLD);
            }
        } else if (show_recommended_) {
            if (escape || keycode == KeyAction::Id::RETURN) {
                toggle_recommended(false);
            }
        } else {
            auto selected_world = worldid_;
//            auto world_count = world_count_ - 1;

            if (keycode == KeyAction::Id::LEFT
                || keycode == KeyAction::Id::RIGHT
                || keycode == KeyAction::Id::UP
                || keycode == KeyAction::Id::DOWN
                || keycode == KeyAction::Id::TAB) {
                bool world_found = false;
                bool forward = keycode == KeyAction::Id::LEFT
                               || keycode == KeyAction::Id::UP;

                while (!world_found) {
                    selected_world = get_next_world(selected_world, forward);

                    for (const auto &world : worlds_) {
                        if (world.wid == selected_world) {
                            world_found = true;
                            break;
                        }
                    }
                }

                buttons_[Buttons::BT_WORLD0 + worldid_]->set_state(
                    Button::State::NORMAL);

                worldid_ = static_cast<uint8_t>(selected_world);

                buttons_[Buttons::BT_WORLD0 + worldid_]->set_state(
                    Button::State::PRESSED);
            } else if (escape) {
                auto quitconfirm = UI::get().get_element<UIQuitConfirm>();

                if (quitconfirm && quitconfirm->get().is_active()) {
                    return UI::get().send_key(keycode, pressed);
                }

                button_pressed(Buttons::BT_QUIT_GAME);
            } else if (keycode == KeyAction::Id::RETURN) {
                auto quitconfirm = UI::get().get_element<UIQuitConfirm>();

                if (quitconfirm && quitconfirm->get().is_active()) {
                    return UI::get().send_key(keycode, pressed);
                }

                bool found = false;

                for (size_t i = Buttons::BT_WORLD0; i < Buttons::BT_CHANNEL0;
                     i++) {
                    auto state = buttons_[Buttons::BT_WORLD0 + i]->get_state();

                    if (state == Button::State::PRESSED) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    button_pressed(selected_world + Buttons::BT_WORLD0);
                } else {
                    buttons_[Buttons::BT_WORLD0 + selected_world]->set_state(
                        Button::State::PRESSED);
                }
            }
        }
    }
}

UIElement::Type UIWorldSelect::get_type() const {
    return TYPE;
}

void UIWorldSelect::draw_world() {
    if (world_count_ <= 0) {
        return;  // TODO: Send the user back to the login screen? Otherwise, I
                 // think the screen will be blank with no worlds, or throw a
                 // UILoginNotice up with failed to communite to server?
    }

    for (const auto &world : worlds_) {
        if (world.channelcount < 2) {
            return;  // I remove the world if there is only one channel because
                     // the graphic for the channel selection is defaulted to at
                     // least 2
        }

        buttons_[Buttons::BT_WORLD0 + world.wid]->set_active(true);

        if (channelid_ >= world.channelcount) {
            channelid_ = 0;
        }
    }
}

void UIWorldSelect::add_world(World world) {
    worlds_.emplace_back(std::move(world));
    world_count_++;
}

void UIWorldSelect::add_recommended_world(RecommendedWorld world) {
    if (use_recommended_) {
        recommended_worlds_.emplace_back(std::move(world));
        recommended_world_count_++;

        buttons_[Buttons::BT_VIEW_RECOMMENDED]->set_state(
            Button::State::NORMAL);
    }
}

void UIWorldSelect::change_world(const World &selectedWorld) {
    buttons_[Buttons::BT_WORLD0 + selectedWorld.wid]->set_state(
        Button::State::PRESSED);

    for (size_t i = 0; i < selectedWorld.channelcount; ++i) {
        buttons_[Buttons::BT_CHANNEL0 + i]->set_active(true);

        if (i == channelid_) {
            buttons_[Buttons::BT_CHANNEL0 + i]->set_state(
                Button::State::PRESSED);
        }
    }

    buttons_[Buttons::BT_ENTER_WORLD]->set_active(true);
}

void UIWorldSelect::remove_selected() {
    deactivate();

    Sound(Sound::Name::SCROLL_UP).play();

    world_selected_ = false;

    clear_selected_world();

    draw_chatballoon_ = false;
}

void UIWorldSelect::set_region(uint8_t /*regionid*/) {
    // maple-offline: v83 world buttons are hanging ribbons BtWorld/<worldid>.
    world_map_[Buttons::BT_WORLD0] = Worlds::SCANIA;
    world_map_[Buttons::BT_WORLD1] = Worlds::BERA;
    world_map_[Buttons::BT_WORLD2] = Worlds::AURORA;
    world_map_[Buttons::BT_WORLD3] = Worlds::ELYSIUM1;
    world_map_[Buttons::BT_WORLD4] = Worlds::REBOOT1;

    worlds_background_ = nl::node();
    world_src_pos_ = Point<int16_t>(0, 0);

    // KMS 1.2.95 world art is numbered by KMS worlds (0 = Extreme); the
    // single offline world uses the Khaini art, as on the KMS world screen.
    bool kms = static_cast<bool>(nl::nx::ui_img("Login.img")["Title"]["signboard"]);

    for (size_t i = Buttons::BT_WORLD0; i <= Buttons::BT_WORLD4; i++) {
        std::string world = std::to_string(world_map_[i]);
        if (kms) {
            static const char *const kms_art[] = { "3", "1", "2", "4", "5" };
            world = kms_art[i - Buttons::BT_WORLD0];
        }
        world_textures_.emplace_back(world_select_["world"][world]);
        recommended_world_textures_.emplace_back(world_select_["world"][world]);
        Point<int16_t> wpos(static_cast<int16_t>(200 + 30 * i), 120);
        if (static_cast<bool>(nl::nx::ui_img("Login.img")["Title"]["signboard"])) {
            // KMS: grid on the world board, 6 per row
            // maple-offline: world buttons sit on the ivory of the top board,
            // clear of its brown frame (was 125,48: on the frame)
            wpos = Point<int16_t>(static_cast<int16_t>(147 + 97 * (i % 6)),
                                  static_cast<int16_t>(52 + 26 * (i / 6)));
        }
        buttons_[Buttons::BT_WORLD0 + i] = std::make_unique<MapleButton>(world_src_[world], wpos);
        buttons_[Buttons::BT_WORLD0 + i]->set_active(false);
    }
}

Point<int16_t> UIWorldSelect::channel_cell(size_t i) const {
    int16_t col = static_cast<int16_t>(i % ch_cols_), row = static_cast<int16_t>(i / ch_cols_);
    return ch_off_ + Point<int16_t>(static_cast<int16_t>(col * ch_step_.x()),
                                    static_cast<int16_t>(row * ch_step_.y()));
}

uint16_t UIWorldSelect::get_worldbyid(uint16_t worldid) {
    return world_map_.find(worldid)->second;
}

Button::State UIWorldSelect::button_pressed(uint16_t id) {
    if (id == Buttons::BT_ENTER_WORLD) {
        enter_world();

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_QUIT_GAME) {
        UI::get().emplace<UIQuitConfirm>();

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_VIEW_RECOMMENDED) {
        world_selected_ = false;
        clear_selected_world();
        toggle_recommended(true);

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_VIEW_ALL) {
        toggle_recommended(false);

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_VIEW_RECOMMENDED_SELECT) {
        buttons_[Buttons::BT_WORLD0 + worldid_]->set_state(
            Button::State::NORMAL);

        worldid_ = recommended_worldid_;

        buttons_[Buttons::BT_WORLD0 + worldid_]->set_state(
            Button::State::PRESSED);

        toggle_recommended(false);

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_VIEW_RECOMMENDED_CANCEL) {
        toggle_recommended(false);

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_VIEW_RECOMMENDED_PREV) {
        if (recommended_worldid_ > 0) {
            recommended_worldid_--;
        } else {
            recommended_worldid_ = recommended_world_count_ - 1;
        }

        recommended_message_.change_text(
            recommended_worlds_[recommended_worldid_].message);

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_VIEW_RECOMMENDED_NEXT) {
        if (recommended_worldid_ < recommended_world_count_ - 1) {
            recommended_worldid_++;
        } else {
            recommended_worldid_ = 0;
        }

        recommended_message_.change_text(
            recommended_worlds_[recommended_worldid_].message);

        return Button::State::NORMAL;
    }

    if (id == Buttons::BT_CHANGE_REGION) {
        UI::get().emplace<UIRegion>();

        deactivate();

        return Button::State::NORMAL;
    }

    if (id >= Buttons::BT_WORLD0 && id < Buttons::BT_CHANNEL0) {
        toggle_recommended(false);

        buttons_[Buttons::BT_WORLD0 + worldid_]->set_state(
            Button::State::NORMAL);

        worldid_ = id - Buttons::BT_WORLD0;

        fn_server_status_req(worldid_);

        world_selected_ = true;
        clear_selected_world();
        change_world(worlds_[worldid_]);

        return Button::State::PRESSED;
    }

    if (id >= Buttons::BT_CHANNEL0 && id < Buttons::BT_ENTER_WORLD) {
        auto selectedch = static_cast<uint8_t>(id - Buttons::BT_CHANNEL0);

        if (selectedch != channelid_) {
            buttons_[Buttons::BT_CHANNEL0 + channelid_]->set_state(
                Button::State::NORMAL);
            channelid_ = static_cast<uint8_t>(id - Buttons::BT_CHANNEL0);
            buttons_[Buttons::BT_CHANNEL0 + channelid_]->set_state(
                Button::State::PRESSED);
            Sound(Sound::Name::WORLD_SELECT).play();
        } else {
            enter_world();
        }

        return Button::State::PRESSED;
    }

    return Button::State::NORMAL;
}

uint8_t UIWorldSelect::get_channel_count(uint8_t wid) const {
    for (const auto &world : worlds_) {
        if (world.wid == wid) {
            return world.channelcount;
        }
    }

    return 0;
}

void UIWorldSelect::enter_world() {
    Configuration::get().set_worldid(worldid_);
    Configuration::get().set_channelid(channelid_);

    UI::get().emplace<UILoginWait>();
    auto loginwait = UI::get().get_element<UILoginWait>();

    if (loginwait && loginwait->get().is_active()) {
        fn_charlist_req(worldid_, channelid_);
    }
}

void UIWorldSelect::toggle_recommended(bool active) {
    if (recommended_world_count_ > 0) {
        recommended_worldid_ = 0;
        show_recommended_ = active;

        buttons_[Buttons::BT_VIEW_ALL]->set_active(active);
        buttons_[Buttons::BT_VIEW_RECOMMENDED]->set_active(!active);
        buttons_[Buttons::BT_VIEW_RECOMMENDED_SELECT]->set_active(active);
        buttons_[Buttons::BT_VIEW_RECOMMENDED_CANCEL]->set_active(active);
        buttons_[Buttons::BT_VIEW_RECOMMENDED_PREV]->set_active(active);
        buttons_[Buttons::BT_VIEW_RECOMMENDED_NEXT]->set_active(active);

        if (recommended_world_count_ <= 1) {
            buttons_[Buttons::BT_VIEW_RECOMMENDED_PREV]->set_state(
                Button::State::DISABLED);
            buttons_[Buttons::BT_VIEW_RECOMMENDED_NEXT]->set_state(
                Button::State::DISABLED);
        } else {
            buttons_[Buttons::BT_VIEW_RECOMMENDED_PREV]->set_state(
                Button::State::NORMAL);
            buttons_[Buttons::BT_VIEW_RECOMMENDED_NEXT]->set_state(
                Button::State::NORMAL);
        }

        if (!active) {
            recommended_message_.change_text("");
        } else {
            recommended_message_.change_text(
                recommended_worlds_[recommended_worldid_].message);
        }
    }
}

void UIWorldSelect::clear_selected_world() {
    channelid_ = 0;

    for (size_t i = Buttons::BT_CHANNEL0; i < Buttons::BT_ENTER_WORLD; i++) {
        buttons_[i]->set_state(Button::State::NORMAL);
    }

    buttons_[Buttons::BT_CHANNEL0]->set_state(Button::State::PRESSED);

    for (size_t i = 0; i < Buttons::BT_ENTER_WORLD - Buttons::BT_CHANNEL0;
         i++) {
        buttons_[Buttons::BT_CHANNEL0 + i]->set_active(false);
    }

    buttons_[Buttons::BT_ENTER_WORLD]->set_active(false);
}

uint16_t UIWorldSelect::get_next_world(uint16_t id, bool upward) {
    uint16_t next_world;

    if (world_map_[Buttons::BT_WORLD0] == Worlds::SCANIA) {
        switch (id) {
            case Buttons::BT_WORLD0:
                next_world = (upward) ? Worlds::REBOOT1 : Worlds::BERA;
                break;
            case Buttons::BT_WORLD1:
                next_world = (upward) ? Worlds::SCANIA : Worlds::AURORA;
                break;
            case Buttons::BT_WORLD2:
                next_world = (upward) ? Worlds::BERA : Worlds::ELYSIUM1;
                break;
            case Buttons::BT_WORLD3:
                next_world = (upward) ? Worlds::AURORA : Worlds::REBOOT1;
                break;
            case Buttons::BT_WORLD4:
                next_world = (upward) ? Worlds::ELYSIUM1 : Worlds::SCANIA;
                break;
            default: break;
        }
    } else {
        switch (id) {
            case Buttons::BT_WORLD0:
                next_world = (upward) ? Worlds::REBOOT1 : Worlds::REBOOT1;
                break;
            case Buttons::BT_WORLD4:
                next_world = (upward) ? Worlds::SCANIA : Worlds::SCANIA;
                break;
            default: break;
        }
    }

    auto world = world_map_.begin();

    while (world != world_map_.end()) {
        if (world->second == next_world) {
            return world->first;
        }

        world++;
    }

    return Worlds::SCANIA;
}
}  // namespace ms