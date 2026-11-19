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
#include "UIStatsInfo.h"

#include <nlnx/nx.hpp>

#include "../../Character/ExpTable.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/PlayerPackets.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "UINotice.h"

namespace ms {
namespace {
auto fn_spend_ap = []<typename... T>(T && ...args) {
    SpendApPacket(std::forward<T>(args)...).dispatch();
};

// v83 UIWindow.img/Stat/backgrnd (175x347): value column starts at x 57.
constexpr int16_t VALUE_X = 60;
constexpr int16_t APUP_X = 152;
constexpr int16_t DETAIL_X = 176;
}  // namespace

UIStatsInfo::UIStatsInfo(const CharStats &st) :
    UIDragElement<PosSTATS>(Point<int16_t>(175, 20)),
    stats_(st) {
    nl::node Stat = nl::nx::ui_img("UIWindow.img")["Stat"];

    backgrnd_ = Stat["backgrnd"];
    backgrnd2_ = Stat["backgrnd2"];

    buttons_[BT_CLOSE] = std::make_unique<MapleButton>(nl::nx::ui_img("Basic.img")["BtClose"],
                                                       Point<int16_t>(157, 6));

    const std::pair<Buttons, int16_t> apups[] = { { BT_HP, 117 }, { BT_MP, 135 },
                                                  { BT_STR, 245 }, { BT_DEX, 263 },
                                                  { BT_INT, 281 }, { BT_LUK, 299 } };

    for (const auto &[bt, y] : apups) {
        buttons_[bt] = std::make_unique<MapleButton>(Stat["BtApUp"], Point<int16_t>(APUP_X, y + 2));
    }

    buttons_[BT_AUTO] = std::make_unique<MapleButton>(Stat["BtAuto"], Point<int16_t>(97, 197));
    buttons_[BT_DETAIL] = std::make_unique<MapleButton>(Stat["BtDetail"], Point<int16_t>(120, 324));

    const int16_t rows[StatLabel::NUM_LABELS] = {
        33, 50, 81, 99, 117, 135, 153, 171,  // NAME..FAME
        0,                                    // AP (centred box)
        245, 263, 281, 299,                   // STR..LUK
        8, 26, 44, 62, 80, 98, 116, 134, 152  // detail panel
    };

    for (size_t i = 0; i < StatLabel::NUM_LABELS; i++) {
        bool detail = i >= StatLabel::ATTACK;

        stat_labels_[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
        stat_offsets_[i] = detail ? Point<int16_t>(DETAIL_X + 80, rows[i] - 2)
                                  : Point<int16_t>(VALUE_X, rows[i] - 2);
    }

    stat_labels_[AP] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::EMPEROR);
    stat_offsets_[AP] = Point<int16_t>(75, 214);

    show_detail_ = false;
    has_ap_ = true;

    update_all_stats();
    update_stat(MapleStat::Id::JOB);
    update_stat(MapleStat::Id::FAME);
    update_ap();

    dimension_ = Point<int16_t>(175, 347);
    drag_area_ = Point<int16_t>(175, 20);
}

void UIStatsInfo::draw(float alpha) const {
    backgrnd_.draw(position_);

    if (show_detail_) {
        backgrnd2_.draw(position_ + Point<int16_t>(DETAIL_X, 0));
    }

    size_t last = show_detail_ ? StatLabel::NUM_LABELS : StatLabel::ATTACK;

    for (size_t i = 0; i < last; i++) {
        stat_labels_[i].draw(position_ + stat_offsets_[i]);
    }

    UIElement::draw_buttons(alpha);
}

void UIStatsInfo::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed && escape) {
        deactivate();
    }
}

bool UIStatsInfo::is_in_range(Point<int16_t> cursorpos) const {
    Point<int16_t> size = show_detail_ ? Point<int16_t>(DETAIL_X + 177, 347) : dimension_;

    return Rectangle<int16_t>(position_, position_ + size).contains(cursorpos);
}

UIElement::Type UIStatsInfo::get_type() const {
    return TYPE;
}

void UIStatsInfo::update_all_stats() {
    update_simple(AP, MapleStat::Id::AP);

    if (has_ap_ ^ (stats_.get_stat(MapleStat::Id::AP) > 0)) {
        update_ap();
    }

    stat_labels_[NAME].change_text(stats_.get_name());
    stat_labels_[GUILD].change_text("-");
    update_simple(LEVEL, MapleStat::Id::LEVEL);
    stat_labels_[HP].change_text(std::to_string(stats_.get_stat(MapleStat::Id::HP)) + " / "
                                 + std::to_string(stats_.get_total(EquipStat::Id::HP)));
    stat_labels_[MP].change_text(std::to_string(stats_.get_stat(MapleStat::Id::MP)) + " / "
                                 + std::to_string(stats_.get_total(EquipStat::Id::MP)));

    int16_t level = stats_.get_stat(MapleStat::Id::LEVEL);
    int64_t exp = stats_.get_exp();

    if (level < ExpTable::LEVELCAP) {
        double pct = 100.0 * static_cast<double>(exp) / ExpTable::values[level];
        std::string p = std::to_string(pct);
        stat_labels_[EXP].change_text(std::to_string(exp) + " (" + p.substr(0, p.find('.') + 3) + "%)");
    } else {
        stat_labels_[EXP].change_text("-");
    }

    update_basevstotal(STR, MapleStat::Id::STR, EquipStat::Id::STR);
    update_basevstotal(DEX, MapleStat::Id::DEX, EquipStat::Id::DEX);
    update_basevstotal(INT, MapleStat::Id::INT, EquipStat::Id::INT);
    update_basevstotal(LUK, MapleStat::Id::LUK, EquipStat::Id::LUK);

    stat_labels_[ATTACK].change_text(std::to_string(stats_.get_mindamage()) + " ~ "
                                     + std::to_string(stats_.get_maxdamage()));
    stat_labels_[ATTACK].change_color(stats_.is_damage_buffed() ? Color::Name::RED
                                                                : Color::Name::EMPEROR);

    update_buffed(WDEF, EquipStat::Id::WDEF);
    update_buffed(MAGIC, EquipStat::Id::MAGIC);
    update_buffed(MDEF, EquipStat::Id::MDEF);
    update_buffed(ACC, EquipStat::Id::ACC);
    update_buffed(AVOID, EquipStat::Id::AVOID);
    update_buffed(HANDS, EquipStat::Id::HANDS);

    stat_labels_[SPEED].change_text(std::to_string(stats_.get_total(EquipStat::Id::SPEED)) + "%");
    stat_labels_[JUMP].change_text(std::to_string(stats_.get_total(EquipStat::Id::JUMP)) + "%");
}

void UIStatsInfo::update_stat(MapleStat::Id stat) {
    switch (stat) {
        case MapleStat::Id::JOB: stat_labels_[JOB].change_text(stats_.get_jobname()); break;
        case MapleStat::Id::FAME:
            stat_labels_[FAME].change_text(
                std::to_string(static_cast<int16_t>(stats_.get_stat(MapleStat::Id::FAME))));
            break;
        default: break;
    }
}

Button::State UIStatsInfo::button_pressed(uint16_t id) {
    const Player &player = Stage::get().get_player();

    switch (id) {
        case BT_CLOSE: deactivate(); break;
        case BT_HP: send_apup(MapleStat::Id::HP); break;
        case BT_MP: send_apup(MapleStat::Id::MP); break;
        case BT_STR: send_apup(MapleStat::Id::STR); break;
        case BT_DEX: send_apup(MapleStat::Id::DEX); break;
        case BT_INT: send_apup(MapleStat::Id::INT); break;
        case BT_LUK: send_apup(MapleStat::Id::LUK); break;
        case BT_AUTO: {
            uint16_t nowap = stats_.get_stat(MapleStat::Id::AP);
            EquipStat::Id primary = player.get_stats().get_job().get_primary(player.get_weapontype());

            MapleStat::Id target = MapleStat::Id::STR;

            switch (primary) {
                case EquipStat::Id::DEX: target = MapleStat::Id::DEX; break;
                case EquipStat::Id::INT: target = MapleStat::Id::INT; break;
                case EquipStat::Id::LUK: target = MapleStat::Id::LUK; break;
                default: break;
            }

            std::string name = target == MapleStat::Id::STR   ? "STR"
                               : target == MapleStat::Id::DEX ? "DEX"
                               : target == MapleStat::Id::INT ? "INT"
                                                              : "LUK";
            std::string message = "Your AP will be distributed as follows:\\n" + name + " : +"
                                  + std::to_string(nowap);

            std::function<void(bool)> yesnohandler = [&, target, nowap](bool yes) {
                if (yes) {
                    for (uint16_t i = 0; i < nowap; i++) {
                        send_apup(target);
                    }
                }
            };

            UI::get().emplace<UIYesNo>(message, yesnohandler, Text::Alignment::LEFT);
        } break;
        case BT_DETAIL: set_detail(!show_detail_); break;
        default: break;
    }

    return Button::State::NORMAL;
}

void UIStatsInfo::send_apup(MapleStat::Id stat) const {
    fn_spend_ap(stat);
    UI::get().disable();
}

void UIStatsInfo::set_detail(bool enabled) {
    show_detail_ = enabled;
}

void UIStatsInfo::update_ap() {
    bool nowap = stats_.get_stat(MapleStat::Id::AP) > 0;
    Button::State newstate = nowap ? Button::State::NORMAL : Button::State::DISABLED;

    for (int i = BT_HP; i <= BT_AUTO; i++) {
        buttons_[i]->set_state(newstate);
    }

    has_ap_ = nowap;
}

void UIStatsInfo::update_simple(StatLabel label, MapleStat::Id stat) {
    stat_labels_[label].change_text(std::to_string(stats_.get_stat(stat)));
}
void UIStatsInfo::update_basevstotal(StatLabel label,
                                     MapleStat::Id bstat,
                                     EquipStat::Id tstat) {
    int32_t base = stats_.get_stat(bstat);
    int32_t total = stats_.get_total(tstat);
    int32_t delta = total - base;

    std::string stattext = std::to_string(total);

    if (delta) {
        stattext += " (" + std::to_string(base);

        if (delta > 0) {
            stattext += "+" + std::to_string(delta);
        } else if (delta < 0) {
            stattext += "-" + std::to_string(-delta);
        }

        stattext += ")";
    }

    stat_labels_[label].change_text(stattext);
}

void UIStatsInfo::update_buffed(StatLabel label, EquipStat::Id stat) {
    int32_t total = stats_.get_total(stat);
    int32_t delta = stats_.get_buffdelta(stat);

    std::string stattext = std::to_string(total);

    if (delta) {
        stattext += " (" + std::to_string(total - delta);

        if (delta > 0) {
            stattext += "+" + std::to_string(delta);

            stat_labels_[label].change_color(Color::Name::RED);
        } else if (delta < 0) {
            stattext += "-" + std::to_string(-delta);

            stat_labels_[label].change_color(Color::Name::BLUE);
        }

        stattext += ")";
    }

    stat_labels_[label].change_text(stattext);
}
}  // namespace ms