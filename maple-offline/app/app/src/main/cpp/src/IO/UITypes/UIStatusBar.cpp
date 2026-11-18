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
#include "UIStatusBar.h"

#include <nlnx/nx.hpp>

#include "../../Character/ExpTable.h"
#include "../../Character/Job.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "UIChannel.h"
#include "UIEquipInventory.h"
#include "UIItemInventory.h"
#include "UIKeyConfig.h"
#include "UIOptionMenu.h"
#include "UIQuit.h"
#include "UISkillBook.h"
#include "UIStatsInfo.h"

namespace ms {
namespace {
auto fn_enter_cashshop = []<typename... T>(T && ...args) {
    EnterCashShopPacket(std::forward<T>(args)...).dispatch();
};

// Layout of StatusBar.img/gauge/bar (340x31): fill rows 15..28,
// HP x 2..107, MP x 111..216, EXP x 225..338.
constexpr int16_t GAUGE_X = 216;
constexpr int16_t GAUGE_Y = 35;
constexpr int16_t FILL_Y = 15;
constexpr int16_t FILL_H = 14;
constexpr int16_t RIGHT_BLOCK = 216;
}  // namespace

UIStatusBar::UIStatusBar(const CharStats &st, uint8_t channel_count) :
    UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 71)),
    stats_(st),
    channel_count_(channel_count),
    quickslot_active_(false) {
    VWIDTH_ = Constants::Constants::get().get_viewwidth();
    VHEIGHT_ = Constants::Constants::get().get_viewheight();

    nl::node bar = nl::nx::ui["StatusBar.img"];

    backgrnd_ = bar["base"]["backgrnd"];
    backgrnd2_ = bar["base"]["backgrnd2"];
    gauge_bar_ = bar["gauge"]["bar"];
    gauge_gray_ = bar["gauge"]["gray"];
    graduation_ = bar["gauge"]["graduation"];
    quickslot_panel_ = bar["base"]["quickSlot"];
    numbers_ = Charset(bar["number"], Charset::Alignment::LEFT);

    position_ = Point<int16_t>(0, VHEIGHT_ - 71);
    dimension_ = Point<int16_t>(VWIDTH_, 71);

    level_text_ = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE);
    job_text_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
    name_text_ = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE);

    // Big buttons in the top band, anchored to the right edge.
    int16_t rx = VWIDTH_ - RIGHT_BLOCK;
    const char *const big[4] = { "BtShop", "BtMenu", "BtShort", "BtNPT" };

    for (uint16_t i = 0; i < 4; i++) {
        buttons_[BT_SHOP + i] = std::make_unique<MapleButton>(
            bar[big[i]], Point<int16_t>(rx + i * 54, 1));
    }

    // Small quick keys in the bottom band.
    const char *const keys[6] = { "KeySet", "StatKey", "EquipKey", "InvenKey", "SkillKey", "QuickSlot" };

    for (uint16_t i = 0; i < 6; i++) {
        buttons_[BT_KEYSET + i] = std::make_unique<MapleButton>(
            bar[keys[i]], Point<int16_t>(rx + 12 + i * 32, 43));
    }

    buttons_[BT_QUICKSLOT_DOWN] = std::make_unique<MapleButton>(
        bar["QuickSlotD"], Point<int16_t>(rx + 12 + 5 * 32, 43));
    buttons_[BT_QUICKSLOT_DOWN]->set_active(false);

    // Game menu popup above the MENU button.
    nl::node gm = nl::nx::ui["UIWindow.img"]["GameMenu"];
    menu_backgrnd_ = gm["backgrnd"];
    menu_pos_ = Point<int16_t>(rx + 54 + 27 - 46, -142);

    const char *const gm_buttons[5] = { "BtChannel", "BtGameOpt", "BtSysOpt", "BtSkin", "BtQuit" };

    for (uint16_t i = 0; i < 5; i++) {
        buttons_[BT_GM_CHANNEL + i] = std::make_unique<MapleButton>(
            gm[gm_buttons[i]], menu_pos_ + Point<int16_t>(6, 23 + i * 21));
        buttons_[BT_GM_CHANNEL + i]->set_active(false);
    }

    // Skins are not part of v83 single-player.
    buttons_[BT_GM_SKIN]->set_state(Button::State::DISABLED);

    // Trading/NPT is not part of single-player.
    buttons_[BT_TRADE]->set_state(Button::State::DISABLED);
}

float UIStatusBar::exp_percent() const {
    int16_t level = stats_.get_stat(MapleStat::Id::LEVEL);

    if (level >= ExpTable::LEVELCAP) {
        return 0.0f;
    }

    return static_cast<float>(static_cast<double>(stats_.get_exp())
                              / ExpTable::values[level]);
}

void UIStatusBar::draw_gauge(int16_t x0, int16_t x1, float percent) const {
    percent = std::max(0.0f, std::min(1.0f, percent));

    int16_t width = x1 - x0 + 1;
    int16_t filled = static_cast<int16_t>(width * percent);

    if (filled < width) {
        gauge_gray_.draw(DrawArgument(
            position_ + Point<int16_t>(GAUGE_X + x0 + filled, GAUGE_Y + FILL_Y),
            Point<int16_t>(width - filled, FILL_H)));
    }
}

void UIStatusBar::draw_numbers(const std::string &text, Point<int16_t> center) const {
    // StatusBar.img/number names its symbols slash/percent/Lbracket/Rbracket;
    // Charset keys them by their first letter.
    std::string mapped;

    for (char c : text) {
        switch (c) {
            case '/': mapped += 's'; break;
            case '%': mapped += 'p'; break;
            case '[': mapped += 'L'; break;
            case ']': mapped += 'R'; break;
            default: mapped += c; break;
        }
    }

    int16_t width = 0;

    for (char c : mapped) {
        width += numbers_.getw(c);
    }

    numbers_.draw(mapped, DrawArgument(center - Point<int16_t>(width / 2, 3)));
}

void UIStatusBar::draw(float alpha) const {
    Point<int16_t> top = position_;

    backgrnd_.draw(top);
    backgrnd_.draw(top + Point<int16_t>(VWIDTH_ - 800, 0));
    backgrnd2_.draw(top);

    level_text_.draw(top + Point<int16_t>(32, 41));
    job_text_.draw(top + Point<int16_t>(142, 36));
    name_text_.draw(top + Point<int16_t>(142, 49));

    gauge_bar_.draw(top + Point<int16_t>(GAUGE_X, GAUGE_Y));

    int32_t hp = stats_.get_stat(MapleStat::Id::HP);
    int32_t mp = stats_.get_stat(MapleStat::Id::MP);
    int32_t maxhp = std::max<int32_t>(1, stats_.get_total(EquipStat::Id::HP));
    int32_t maxmp = std::max<int32_t>(1, stats_.get_total(EquipStat::Id::MP));
    float expp = exp_percent();

    draw_gauge(2, 107, static_cast<float>(hp) / maxhp);
    draw_gauge(111, 216, static_cast<float>(mp) / maxmp);
    draw_gauge(225, 338, expp);

    graduation_.draw(top + Point<int16_t>(GAUGE_X, GAUGE_Y));

    int16_t fy = GAUGE_Y + FILL_Y + FILL_H / 2;

    draw_numbers("[" + std::to_string(hp) + "/" + std::to_string(maxhp) + "]",
                 top + Point<int16_t>(GAUGE_X + 55, fy));
    draw_numbers("[" + std::to_string(mp) + "/" + std::to_string(maxmp) + "]",
                 top + Point<int16_t>(GAUGE_X + 164, fy));

    std::string pct = std::to_string(expp * 100.0f);
    pct = pct.substr(0, pct.find('.') + 3);
    draw_numbers(std::to_string(stats_.get_exp()) + "[" + pct + "%]",
                 top + Point<int16_t>(GAUGE_X + 282, fy));

    if (quickslot_active_) {
        quickslot_panel_.draw(top + Point<int16_t>(VWIDTH_ - 155, -82));
    }

    if (menu_active_) {
        menu_backgrnd_.draw(top + menu_pos_);
    }

    UIElement::draw_buttons(alpha);
}

void UIStatusBar::update() {
    UIElement::update();

    level_text_.change_text(std::to_string(stats_.get_stat(MapleStat::Id::LEVEL)));
    job_text_.change_text(stats_.get_jobname());
    name_text_.change_text(stats_.get_name());
}

bool UIStatusBar::is_in_range(Point<int16_t> cursorpos) const {
    Rectangle<int16_t> bounds(position_, position_ + dimension_);

    if (menu_active_) {
        Rectangle<int16_t> menu(position_ + menu_pos_,
                                position_ + menu_pos_ + menu_backgrnd_.get_dimensions());

        if (menu.contains(cursorpos)) {
            return true;
        }
    }

    if (quickslot_active_) {
        Rectangle<int16_t> qs(position_ + Point<int16_t>(VWIDTH_ - 155, -82),
                              position_ + Point<int16_t>(VWIDTH_ - 4, 0));

        if (qs.contains(cursorpos)) {
            return true;
        }
    }

    return bounds.contains(cursorpos);
}

void UIStatusBar::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed && escape) {
        if (menu_active_) {
            remove_menus();
        } else {
            UI::get().emplace<UIQuit>(stats_);
        }
    }
}

UIElement::Type UIStatusBar::get_type() const {
    return TYPE;
}

void UIStatusBar::toggle_qs() {
    quickslot_active_ = !quickslot_active_;
    buttons_[BT_QUICKSLOT]->set_active(!quickslot_active_);
    buttons_[BT_QUICKSLOT_DOWN]->set_active(quickslot_active_);
}

void UIStatusBar::toggle_menu() {
    menu_active_ = !menu_active_;

    for (uint16_t i = BT_GM_CHANNEL; i <= BT_GM_QUIT; i++) {
        buttons_[i]->set_active(menu_active_);
    }
}

void UIStatusBar::remove_menus() {
    if (menu_active_) {
        toggle_menu();
    }
}

bool UIStatusBar::is_menu_active() {
    return menu_active_;
}

Button::State UIStatusBar::button_pressed(uint16_t id) {
#ifdef MS_DESKTOP_DEBUG
    std::fprintf(stderr, "[dbg] statusbar button %d\n", id);
#endif
    Player &player = Stage::get().get_player();

    switch (id) {
        case BT_SHOP: fn_enter_cashshop(); break;
        case BT_MENU: toggle_menu(); break;
        case BT_SHORTCUT:
        case BT_KEYSET:
            UI::get().emplace<UIKeyConfig>(player.get_inventory(), player.get_skills());
            break;
        case BT_STAT: UI::get().emplace<UIStatsInfo>(player.get_stats()); break;
        case BT_EQUIP: UI::get().emplace<UIEquipInventory>(player.get_inventory()); break;
        case BT_INVEN: UI::get().emplace<UIItemInventory>(player.get_inventory()); break;
        case BT_SKILL:
            UI::get().emplace<UISkillBook>(player.get_stats(), player.get_skills());
            break;
        case BT_QUICKSLOT:
        case BT_QUICKSLOT_DOWN: toggle_qs(); break;
        case BT_GM_CHANNEL:
            remove_menus();
            UI::get().emplace<UIChannel>(player.get_world_id(), player.get_channel_id(),
                                         channel_count_);
            break;
        case BT_GM_GAMEOPT:
        case BT_GM_SYSOPT:
            remove_menus();
            UI::get().emplace<UIOptionMenu>();
            break;
        case BT_GM_QUIT:
            remove_menus();
            UI::get().emplace<UIQuit>(stats_);
            break;
        default: break;
    }

    return Button::State::NORMAL;
}
}  // namespace ms
