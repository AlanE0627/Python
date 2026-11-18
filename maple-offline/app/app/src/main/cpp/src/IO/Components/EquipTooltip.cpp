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
#include "EquipTooltip.h"

#include "../../Constants.h"
#include "../../Data/EquipData.h"
#include "../../Data/WeaponData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/Geometry.h"
#include "../../Util/Lang.h"

namespace ms {
EquipTooltip::EquipTooltip() : inv_pos_(0), parent_(Tooltip::Parent::NONE), height_(0) {}

void EquipTooltip::set_equip(Tooltip::Parent parent, int16_t ivp) {
    if (inv_pos_ == ivp && parent_ == parent) {
        return;
    }

    inv_pos_ = ivp;
    parent_ = parent;
    lines_.clear();

    if (ivp == 0) {
        return;
    }

    InventoryType::Id invtype;

    switch (parent) {
        case Tooltip::Parent::ITEM_INVENTORY:
        case Tooltip::Parent::SHOP: invtype = InventoryType::Id::EQUIP; break;
        case Tooltip::Parent::EQUIP_INVENTORY: invtype = InventoryType::Id::EQUIPPED; break;
        default: invtype = InventoryType::Id::NONE; break;
    }

    Player &player = Stage::get().get_player();
    auto oequip = player.get_inventory().get_equip(invtype, ivp);

    if (!oequip) {
        inv_pos_ = 0;
        return;
    }

    const Equip &equip = *oequip;
    const CharStats &stats = player.get_stats();
    int32_t item_id = equip.get_item_id();
    const EquipData &equipdata = EquipData::get(item_id);
    const ItemData &itemdata = equipdata.get_itemdata();

    icon_ = itemdata.get_icon(false);

    // name, coloured by upgrade quality as in v83
    Color::Name namecolor = Color::Name::WHITE;

    switch (equip.get_quality()) {
        case EquipQuality::Id::GREY: namecolor = Color::Name::LIGHTGREY; break;
        case EquipQuality::Id::ORANGE: namecolor = Color::Name::ORANGE; break;
        case EquipQuality::Id::BLUE: namecolor = Color::Name::MEDIUMBLUE; break;
        case EquipQuality::Id::VIOLET: namecolor = Color::Name::VIOLET; break;
        case EquipQuality::Id::GOLD: namecolor = Color::Name::YELLOW; break;
        default: break;
    }

    std::string namestr = itemdata.get_name();

    if (equip.get_level() > 0) {
        namestr += " (+" + std::to_string(equip.get_level()) + ")";
    }

    if (itemdata.get_gender() == 0) {
        namestr += std::string(" ") + Lang::MALE_ONLY;
    } else if (itemdata.get_gender() == 1) {
        namestr += std::string(" ") + Lang::FEMALE_ONLY;
    }

    name_ = Text(Text::Font::A12B, Text::Alignment::CENTER, namecolor, namestr, WIDTH_ - 16);

    auto add = [&](const std::string &text, Color::Name color) {
        lines_.emplace_back(Text::Font::A11M, Text::Alignment::LEFT, color, text, WIDTH_ - 20);
    };

    if (itemdata.is_untradable()) {
        add(Lang::UNTRADABLE, Color::Name::ORANGE);
    }

    if (itemdata.is_unique()) {
        add(Lang::ONE_OF_A_KIND, Color::Name::ORANGE);
    }

    // requirements (red when not met)
    struct Req {
        MapleStat::Id id;
        const char *label;
    };

    const Req reqs[] = {{MapleStat::Id::LEVEL, Lang::REQ_LEV},
                        {MapleStat::Id::STR, Lang::REQ_STR},
                        {MapleStat::Id::DEX, Lang::REQ_DEX},
                        {MapleStat::Id::INT, Lang::REQ_INT},
                        {MapleStat::Id::LUK, Lang::REQ_LUK}};

    for (const Req &r : reqs) {
        int16_t need = equipdata.get_reqstat(r.id);
        bool ok = stats.get_stat(r.id) >= need;

        add(std::string(r.label) + " : " + std::to_string(need),
            ok ? Color::Name::WHITE : Color::Name::RED);
    }

    // job line: usable jobs white, others grey (bit mask 1/2/4/8/16)
    int16_t reqjob = equipdata.get_reqstat(MapleStat::Id::JOB);
    int32_t mybranch = stats.get_stat(MapleStat::Id::JOB) / 100;

    if (mybranch >= 10) {
        // Cygnus (1x) and Aran (21) share the explorer branches
        mybranch = mybranch % 10 == 0 ? 0 : mybranch % 10;
        if (stats.get_stat(MapleStat::Id::JOB) / 100 == 21) {
            mybranch = 1;
        }
    }

    // only the jobs that can wear it are listed; red if ours is not one
    std::string jobs;
    bool can_job = false;

    for (int b = 0; b < 6; b++) {
        bool allowed = reqjob == 0 || (b > 0 && (reqjob & (1 << (b - 1))));

        if (!allowed) {
            continue;
        }

        if (b == mybranch || reqjob == 0) {
            can_job = true;
        }

        jobs += std::string(jobs.empty() ? "" : " ") + Lang::JOBS[b];
    }

    add(jobs, can_job ? Color::Name::WHITE : Color::Name::RED);

    add(std::string(Lang::CATEGORY) + " : " + equipdata.get_type(), Color::Name::WHITE);

    if (equipdata.is_weapon()) {
        const WeaponData &weapon = WeaponData::get(item_id);
        add(std::string(Lang::ATTACK_SPEED) + " : " + Lang::speed_name(weapon.get_speed()),
            Color::Name::WHITE);
    }

    for (size_t i = 0; i < EquipStat::Id::LENGTH; i++) {
        auto es = static_cast<EquipStat::Id>(i);
        uint16_t value = equip.get_stat(es);

        if (value > 0) {
            add(std::string(Lang::EQUIP_STATS[i]) + " : +" + std::to_string(value),
                Color::Name::WHITE);
        }
    }

    if (!itemdata.is_cashitem()) {
        add(std::string(Lang::UPGRADES) + " : " + std::to_string(equip.get_slots()),
            Color::Name::WHITE);
    }

    std::string desc = itemdata.get_desc();
    desc_ = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, desc, WIDTH_ - 20);

    height_ = 10 + name_.height() + 6 + 36 + 6;

    for (const Text &t : lines_) {
        height_ += t.height();
    }

    if (!desc.empty()) {
        height_ += desc_.height() + 6;
    }

    height_ += 8;
}

void EquipTooltip::draw(Point<int16_t> pos) const {
    if (inv_pos_ == 0) {
        return;
    }

    int16_t max_width = Constants::Constants::get().get_viewwidth();
    int16_t max_height = Constants::Constants::get().get_viewheight();

    if (pos.x() + WIDTH_ > max_width) {
        pos.set_x(static_cast<int16_t>(max_width - WIDTH_));
    }

    if (pos.y() + height_ > max_height) {
        pos.set_y(static_cast<int16_t>(std::max(0, max_height - height_)));
    }

    // v83 tooltip: dark translucent box with a thin light border
    ColorBox(WIDTH_ + 2, height_ + 2, Color::Name::LIGHTGREY, 0.6f)
        .draw(DrawArgument(pos + Point<int16_t>(-1, -1)));
    ColorBox(WIDTH_, height_, Color::Name::BLACK, 0.82f).draw(DrawArgument(pos));

    int16_t y = 10;
    name_.draw(pos + Point<int16_t>(WIDTH_ / 2, y));
    y += name_.height() + 6;

    icon_.draw(DrawArgument(pos + Point<int16_t>(12, y + 32)));
    y += 36 + 6;

    for (const Text &t : lines_) {
        t.draw(pos + Point<int16_t>(10, y));
        y += t.height();
    }

    if (!desc_.get_text().empty()) {
        y += 6;
        desc_.draw(pos + Point<int16_t>(10, y));
    }
}
}  // namespace ms
