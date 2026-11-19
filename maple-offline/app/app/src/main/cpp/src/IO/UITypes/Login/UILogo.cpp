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
#include "UILogo.h"
#include "../../../Graphics/Geometry.h"
#include "../../../Constants.h"

#include <nlnx/nx.hpp>

#include "../Audio/Audio.h"
#include "../Configuration.h"
#include "UILogin.h"

namespace ms {
UILogo::UILogo() :
    UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600)),
    nexon_ended_(false),
    wizet_ended_(false),
    user_clicked_(false) {
    Music("BgmUI.img/NxLogo").play_once();

    nl::node Logo = nl::nx::ui_img("Logo.img");

    nexon_ = Logo["Nexon"];
    wizet_ = Logo["Wizet"];
    wizet_end_ = Logo["Wizet"]["40"];
}

void UILogo::draw(float inter) const {
    UIElement::draw(inter);

    // maple-offline: every logo frame has its origin at its centre, so the
    // frames go to the centre of the (wide) screen on a white page
    int16_t vw = Constants::Constants::get().get_viewwidth();
    int16_t vh = Constants::Constants::get().get_viewheight();
    ColorBox(vw, vh, Color::Name::WHITE, 1.0f).draw(DrawArgument(Point<int16_t>(0, 0)));
    Point<int16_t> centre(vw / 2, vh / 2);
    // the drawings sit off-centre inside their frames (measured from the
    // last frames: Nexon +56,+7, Wizet +120,+103 from the frame origin)
    Point<int16_t> nexon_at = centre - Point<int16_t>(56, 7);
    Point<int16_t> wizet_at = centre - Point<int16_t>(120, 103);

    if (!user_clicked_) {
        if (!nexon_ended_) {
            nexon_.draw(nexon_at, inter);
        } else {
            if (!wizet_ended_) {
                wizet_.draw(wizet_at, inter);
            } else {
                wizet_end_.draw(wizet_at);
            }
        }
    } else {
        wizet_end_.draw(wizet_at);
    }
}

void UILogo::update() {
    UIElement::update();

    if (!nexon_ended_) {
        nexon_ended_ = nexon_.update();
    } else {
        if (!wizet_ended_) {
            wizet_ended_ = wizet_.update();
        } else {
            Configuration::get().set_start_shown(true);

            UI::get().remove(UIElement::Type::START);
            UI::get().emplace<UILogin>();
        }
    }
}

Cursor::State UILogo::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

    if (clicked && !user_clicked_) {
        user_clicked_ = true;
    }

    return ret;
}

UIElement::Type UILogo::get_type() const {
    return TYPE;
}
}  // namespace ms