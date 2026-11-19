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
#include "UILoginWait.h"

#include "../../../Constants.h"

#include <nlnx/nx.hpp>
#include <utility>
#define _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING
#include "../../Net/PacketProcessor.h"
#include "../Components/MapleButton.h"

namespace ms {
UILoginWait::UILoginWait() : UILoginWait([]() {}) {}

UILoginWait::UILoginWait(std::function<void()> okhandler) :
    okhandler_(std::move(okhandler)) {
    nl::node Loading = nl::nx::ui_img("Login.img")["Notice"]["Loading"];
    nl::node backgrnd = Loading["backgrnd"];

    sprites_.emplace_back(backgrnd);
    sprites_.emplace_back(Loading["circle"], Point<int16_t>(127, 70));

    buttons_[Buttons::CANCEL] =
        std::make_unique<MapleButton>(Loading["BtCancel"],
                                      Point<int16_t>(101, 106));

    dimension_ = Texture(backgrnd).get_dimensions();
    // maple-offline: centred on the screen (the original 800x600 client
    // showed it in the middle; the wide view moved it to the left)
    position_ = Point<int16_t>(
            (Constants::Constants::get().get_viewwidth() - dimension_.x()) / 2,
            (Constants::Constants::get().get_viewheight() - dimension_.y()) / 2);
}

UIElement::Type UILoginWait::get_type() const {
    return TYPE;
}

void UILoginWait::close() {
    deactivate();
    okhandler_();
}

std::function<void()> UILoginWait::get_handler() {
    return okhandler_;
}

Button::State UILoginWait::button_pressed(uint16_t id) {
    PacketProcessor::get().reconnect();

    close();

    return Button::State::NORMAL;
}
}  // namespace ms