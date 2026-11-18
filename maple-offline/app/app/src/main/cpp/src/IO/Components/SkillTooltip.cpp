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
#include "SkillTooltip.h"

#include <nlnx/nx.hpp>

#include "../Data/SkillData.h"
#include "../Util/Lang.h"

namespace ms {
SkillTooltip::SkillTooltip() {
    nl::node Frame = nl::nx::ui["UIToolTip.img"]["Item"]["Frame2"];

    frame_ = Frame;
    cover_ = Frame["cover"];

    skill_id_ = 0;
}

void SkillTooltip::set_skill(int32_t id,
                             int32_t level,
                             int32_t mlevel,
                             int64_t expiration) {
    if (skill_id_ == id) {
        return;
    }

    skill_id_ = id;

    if (skill_id_ == 0) {
        return;
    }

    const SkillData &data = SkillData::get(id);

    int32_t masterlevel;

    if (mlevel > 0) {
        masterlevel = mlevel;
    } else {
        masterlevel = data.get_masterlevel();
    }

    std::string descstr = data.get_desc();

    // maple-offline: drop the master level tag written in the data (English
    // or Korean wording) and put our own in front
    for (const char *tag : {"[Master Level", "[마스터 레벨"}) {
        size_t start = descstr.find(tag);

        if (start != std::string::npos) {
            size_t end = descstr.find(']', start);

            if (end != std::string::npos) {
                size_t cut = end + 1;

                if (descstr.compare(cut, 2, "\\n") == 0) {
                    cut += 2;
                }

                descstr.erase(start, cut - start);
            }
        }
    }

    if (masterlevel > 0) {
        descstr.insert(0, std::string("[") + Lang::MASTER_LEVEL + " : "
                              + std::to_string(masterlevel) + "]\\n");
    }

    const std::string exptag = "#cAvailable until";

    if (expiration > 0) {
        // TODO: Blank
    } else {
        size_t expstart = descstr.find(exptag);
        size_t expend = descstr.find('#', expstart + 1);

        if (expstart < expend && expend != std::string::npos) {
            size_t expsize = expend - expstart + 1;
            descstr.erase(expstart, expsize);
        }
    }

    std::string levelstr;
    bool current = level > 0;
    bool next = level < masterlevel;

    if (current) {
        levelstr += std::string("[") + Lang::CURRENT_LEVEL + " : " + std::to_string(level) + "]\\n"
                    + data.get_level_desc(level);
    }

    if (current && next) {
        levelstr += "\\n";
    }

    if (next) {
        levelstr += std::string("[") + Lang::NEXT_LEVEL + " : " + std::to_string(level + 1) + "]\\n"
                    + data.get_level_desc(level + 1);
    }

    icon_ = data.get_icon(SkillData::Icon::NORMAL);
    name_ = Text(Text::Font::A12B,
                 Text::Alignment::LEFT,
                 Color::Name::WHITE,
                 data.get_name(),
                 320);
    desc_ = Text(Text::Font::A12M,
                 Text::Alignment::LEFT,
                 Color::Name::WHITE,
                 descstr,
                 210);
    level_desc_ = Text(Text::Font::A12M,
                       Text::Alignment::LEFT,
                       Color::Name::WHITE,
                       levelstr,
                       290);

    int16_t desc_height = desc_.height() + 11;

    icon_offset_ = name_.height();
    level_offset_ = std::max<int16_t>(desc_height, 85);
    height_ = icon_offset_ + level_offset_ + level_desc_.height();

    int16_t icon_width = (icon_.get_dimensions().x() * 2) + 4;
    width_ = 292;

    line_ = ColorLine(width_ + 16, Color::Name::WHITE, 1.0f);
    box_ = ColorBox(icon_width, icon_width, Color::Name::WHITE, 0.65f);
}

void SkillTooltip::draw(Point<int16_t> pos) const {
    if (skill_id_ == 0) {
        return;
    }

    int16_t max_width = Constants::Constants::get().get_viewwidth();
    int16_t max_height = Constants::Constants::get().get_viewheight();
    int16_t cur_width = pos.x() + width_ + 45;
    int16_t cur_height = pos.y() + height_ + 35;

    int16_t adj_x = cur_width - max_width;
    int16_t adj_y = cur_height - max_height;

    if (adj_x > 0) {
        pos.shift_x(adj_x * -1);
    }

    if (adj_y > 0) {
        pos.shift_y(adj_y * -1);
    }

    // maple-offline: v83 tooltip box (the later frame graphics are absent)
    ColorBox(width_ + 34, height_ + 30, Color::Name::LIGHTGREY, 0.6f)
        .draw(DrawArgument(pos + Point<int16_t>(9, -5)));
    ColorBox(width_ + 32, height_ + 28, Color::Name::BLACK, 0.82f)
        .draw(DrawArgument(pos + Point<int16_t>(10, -4)));
    name_.draw(pos + Point<int16_t>(33, 3));

    pos.shift_y(icon_offset_);

    box_.draw(DrawArgument(pos + Point<int16_t>(26, 21)));
    icon_.draw(DrawArgument(pos + Point<int16_t>(28, 87), 2.0f, 2.0f));
    desc_.draw(pos + Point<int16_t>(102, 15));

    pos.shift_y(level_offset_);

    line_.draw(pos + Point<int16_t>(22, 10));
    level_desc_.draw(pos + Point<int16_t>(25, 11));
}
}  // namespace ms