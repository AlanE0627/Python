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
#include "MapleButton.h"

namespace ms {
    MapleButton::MapleButton(const nl::node &src, Point<int16_t> pos) {
    nl::node normal = src["normal"];

    // maple-offline: a bitmap node with origin/z children is a single texture, not frames.
    if (normal.size() > 1 && normal.data_type() != nl::node::type::bitmap) {
        animations_[Button::State::NORMAL] = normal;
    } else {
        textures_[Button::State::NORMAL] = normal["0"];
        // maple-offline: some v83 buttons store the bitmap directly in "normal".
        if (!textures_[Button::State::NORMAL].is_valid())
            textures_[Button::State::NORMAL] = normal;
    }

    textures_[Button::State::PRESSED] = src["pressed"]["0"];
    textures_[Button::State::MOUSEOVER] = src["mouseOver"]["0"];
    textures_[Button::State::DISABLED] = src["disabled"]["0"];
    if (!textures_[Button::State::DISABLED].is_valid())
        textures_[Button::State::DISABLED] = src["disabled"];

    // maple-offline: v83 buttons such as channel/<n> only ship normal/disabled.
    if (!textures_[Button::State::PRESSED].is_valid())
        textures_[Button::State::PRESSED] = textures_[Button::State::NORMAL];
    if (!textures_[Button::State::MOUSEOVER].is_valid())
        textures_[Button::State::MOUSEOVER] = textures_[Button::State::NORMAL];

    active_ = true;
    position_ = pos;
    state_ = Button::State::NORMAL;
}

MapleButton::MapleButton(const nl::node &src, int16_t x, int16_t y) :
    MapleButton(src, Point<int16_t>(x, y)) {}

MapleButton::MapleButton(const nl::node &src) :
    MapleButton(src, Point<int16_t>()) {}

void MapleButton::draw(Point<int16_t> parentpos) const {
    if (active_) {
        textures_[state_].draw(position_ + parentpos);
        animations_[state_].draw(position_ + parentpos, 1.0f);
    }
}

void MapleButton::update() {
    if (active_) {
        animations_[state_].update(6);
    }
}

Rectangle<int16_t> MapleButton::bounds(Point<int16_t> parentpos) const {
    Point<int16_t> lt;
    Point<int16_t> rb;

    if (textures_[state_].is_valid()) {
        lt = parentpos + position_ - textures_[state_].get_origin();
        rb = lt + textures_[state_].get_dimensions();
    } else {
        lt = parentpos + position_ - animations_[state_].get_origin();
        rb = lt + animations_[state_].get_dimensions();
    }

    return Rectangle<int16_t>(lt, rb);
}

int16_t MapleButton::width() const {
    return textures_[state_].width();
}

Point<int16_t> MapleButton::origin() const {
    return textures_[state_].get_origin();
}
}  // namespace ms