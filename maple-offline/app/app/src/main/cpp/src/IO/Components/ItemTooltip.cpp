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
#include "ItemTooltip.h"

#include <nlnx/nx.hpp>

#include "../Constants.h"
#include "../Data/ItemData.h"
#include "../Graphics/Geometry.h"
#include "../Util/Lang.h"

#include <algorithm>

namespace ms {
// TODO: Add blue dot next to name
ItemTooltip::ItemTooltip() {
    nl::node Item = nl::nx::ui_img("UIToolTip.img")["Item"];
    nl::node Frame = Item["Frame2"];
    nl::node ItemIcon = Item["ItemIcon"];

    frame_ = Frame;
    cover_ = Frame["cover"];
    base_ = ItemIcon["base"];
    item_cover_ = ItemIcon["cover"];
    type_[true] = ItemIcon["new"];
    type_[false] = ItemIcon["old"];

    item_id_ = 0;
}

bool ItemTooltip::set_item(int32_t iid) {
    if (item_id_ == iid) {
        return false;
    }

    item_id_ = iid;

    if (item_id_ == 0) {
        return false;
    }

    const ItemData &idata = ItemData::get(item_id_);

    item_icon_ = idata.get_icon(false);
    untradable_ = idata.is_untradable();
    unique_ = idata.is_unique();

    std::string quality = "";

    if (unique_ && untradable_) {
        quality = std::string(Lang::ONE_OF_A_KIND) + ", " + Lang::UNTRADABLE;
    } else if (unique_ && !untradable_) {
        quality = Lang::ONE_OF_A_KIND;
    } else if (!unique_ && untradable_) {
        quality = Lang::UNTRADABLE;
    } else {
        quality = "";
    }

    name_ = Text(Text::Font::A12B,
                 Text::Alignment::CENTER,
                 Color::Name::WHITE,
                 idata.get_name(),
                 240);
    desc_ = Text(Text::Font::A11M,
                 Text::Alignment::LEFT,
                 Color::Name::WHITE,
                 idata.get_desc(),
                 190);
    qual_ = Text(Text::Font::A12M,
                 Text::Alignment::CENTER,
                 Color::Name::ORANGE,
                 quality,
                 185);

    // maple-offline: v83 layout — name on top, icon left, text right
    fill_width_ = 260;
    int16_t body = std::max<int16_t>(40, desc_.height());
    fill_height_ = static_cast<int16_t>(10 + name_.height() + 6
                                        + ((untradable_ || unique_) ? qual_.height() + 2 : 0)
                                        + body + 10);
    desc_delta_ = 0;

    return true;
}

void ItemTooltip::draw(Point<int16_t> pos) const {
    if (item_id_ == 0) {
        return;
    }

    int16_t max_width = Constants::Constants::get().get_viewwidth();
    int16_t max_height = Constants::Constants::get().get_viewheight();

    if (pos.x() + fill_width_ > max_width) {
        pos.set_x(static_cast<int16_t>(max_width - fill_width_));
    }

    if (pos.y() + fill_height_ > max_height) {
        pos.set_y(static_cast<int16_t>(std::max(0, max_height - fill_height_)));
    }

    ColorBox(fill_width_ + 2, fill_height_ + 2, Color::Name::LIGHTGREY, 0.6f)
        .draw(DrawArgument(pos + Point<int16_t>(-1, -1)));
    ColorBox(fill_width_, fill_height_, Color::Name::BLACK, 0.82f).draw(DrawArgument(pos));

    int16_t y = 10;
    name_.draw(pos + Point<int16_t>(fill_width_ / 2, y));
    y += name_.height() + 6;

    if (untradable_ || unique_) {
        qual_.draw(pos + Point<int16_t>(fill_width_ / 2, y));
        y += qual_.height() + 2;
    }

    item_icon_.draw(DrawArgument(pos + Point<int16_t>(12, y + 32)));
    desc_.draw(pos + Point<int16_t>(60, y));
}
}  // namespace ms
