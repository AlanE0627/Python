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

#include <vector>

#include "../Graphics/Text.h"
#include "../Graphics/Texture.h"
#include "Tooltip.h"

namespace ms {
// maple-offline: v83-style equipment tooltip — a dark translucent box with
// the icon, name and text lines (requirements, jobs, category, stats,
// upgrades). The later-version tooltip graphics (UIToolTip.img) do not
// exist in v83 data.
class EquipTooltip : public Tooltip {
public:
    EquipTooltip();

    void set_equip(Tooltip::Parent parent, int16_t invpos);

    void draw(Point<int16_t> position) const override;

    // kept for callers; v83 has no comparison tooltip
    void draw_preview(Point<int16_t> position) const {}

private:
    static constexpr int16_t WIDTH_ = 230;

    int16_t inv_pos_;
    Tooltip::Parent parent_;
    Texture icon_;
    Text name_;
    std::vector<Text> lines_;
    Text desc_;
    int16_t height_;
};
}  // namespace ms
