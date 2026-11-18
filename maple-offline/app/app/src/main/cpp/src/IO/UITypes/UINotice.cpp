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
#include "UINotice.h"

#include "../../Graphics/Geometry.h"

#include <nlnx/nx.hpp>
#include <utility>

#include "../Audio/Audio.h"
#include "../Components/MapleButton.h"
#include "../UI.h"

namespace ms {
UINotice::UINotice(const std::string &message,
                   NoticeType t,
                   Text::Alignment a) :
    UIDragElement<PosNOTICE>(),
    type_(t),
    alignment_(a) {
    // maple-offline: v83 notice frame (Basic.img/Notice3: blue body, grey
    // button panel at the bottom).
    nl::node src = nl::nx::ui["Basic.img"]["Notice3"];

    top_ = src["t"];
    center_ = src["c"];
    centerbox_ = nl::node();
    box_ = nl::node();
    box2_ = nl::node();
    bottom_ = src["s"];
    bottombox_ = nl::node();

    if (type_ == NoticeType::YESNO) {
        position_.shift_y(-8);
        question_ = Text(Text::Font::A11M,
                         alignment_,
                         Color::Name::WHITE,
                         message,
                         200);
    } else if (type_ == NoticeType::ENTER_NUMBER) {
        position_.shift_y(-16);
        question_ = Text(Text::Font::A12M,
                         Text::Alignment::LEFT,
                         Color::Name::WHITE,
                         message,
                         200);
    } else if (type_ == NoticeType::OK) {
        uint16_t maxwidth = top_.width() - 6;

        position_.shift_y(-8);
        question_ = Text(Text::Font::A11M,
                         Text::Alignment::CENTER,
                         Color::Name::WHITE,
                         message,
                         maxwidth);
    }

    height_ = question_.height();
    dimension_ = Point<int16_t>(top_.width(),
                                top_.height() + body_height(type_ == NoticeType::ENTER_NUMBER)
                                    + bottom_.height());
    position_ = Point<int16_t>(position_.x() - dimension_.x() / 2,
                               position_.y() - dimension_.y() / 2);
    drag_area_ = Point<int16_t>(dimension_.x(), 20);

    if (type_ != NoticeType::ENTER_NUMBER) {
        Sound(Sound::Name::DLG_NOTICE).play();
    }
}

UINotice::UINotice(std::string message, NoticeType t) :
    UINotice(std::move(message), t, Text::Alignment::CENTER) {}

int16_t UINotice::body_height(bool textfield) const {
    int16_t text = height_ >= 32 ? height_ : 32;

    return text + 12 + (textfield ? 26 : 0);
}

void UINotice::draw(bool textfield) const {
    Point<int16_t> start = position_;
    int16_t body = body_height(textfield);

    top_.draw(start);
    start.shift_y(top_.height());

    center_.draw(DrawArgument(start, Point<int16_t>(0, body)));

    if (textfield) {
        // v83 has no input-box image here; a white field under the text.
        ColorBox(234, 20, Color::Name::WHITE, 1.0f)
            .draw(DrawArgument(position_ + Point<int16_t>(16, field_y() - 2)));
    }

    start.shift_y(body);
    bottom_.draw(start);

    int16_t ty = top_.height() + 6;

    if (alignment_ == Text::Alignment::LEFT || type_ == NoticeType::ENTER_NUMBER) {
        question_.draw(position_ + Point<int16_t>(18, ty));
    } else {
        question_.draw(position_ + Point<int16_t>(top_.width() / 2, ty));
    }
}

int16_t UINotice::field_y() const {
    return top_.height() + 6 + height_ + 6;
}

int16_t UINotice::box2offset(bool textfield) const {
    // y of the buttons: centred in the grey panel of the bottom part.
    return top_.height() + body_height(textfield) + 22;
}

UIYesNo::UIYesNo(std::string message,
                 std::function<void(bool yes)> yh,
                 Text::Alignment alignment) :
    UINotice(std::move(message), NoticeType::YESNO, alignment) {
    yesno_handler_ = std::move(yh);

    int16_t belowtext = box2offset(false);

    nl::node src = nl::nx::ui["Basic.img"];

    buttons_[Buttons::YES] =
        std::make_unique<MapleButton>(src["BtYes"],
                                      Point<int16_t>(60, belowtext));
    buttons_[Buttons::NO] =
        std::make_unique<MapleButton>(src["BtNo"],
                                      Point<int16_t>(141, belowtext));
}

UIYesNo::UIYesNo(std::string message,
                 std::function<void(bool yes)> yesnohandler) :
    UIYesNo(std::move(message),
            std::move(yesnohandler),
            Text::Alignment::CENTER) {}

void UIYesNo::draw(float alpha) const {
    UINotice::draw(false);
    UIElement::draw(alpha);
}

void UIYesNo::send_key(int32_t keycode, bool pressed, bool escape) {
    if (keycode == KeyAction::Id::RETURN) {
        yesno_handler_(true);
        deactivate();
    } else if (escape) {
        yesno_handler_(false);
        deactivate();
    }
}

UIElement::Type UIYesNo::get_type() const {
    return TYPE;
}

Button::State UIYesNo::button_pressed(uint16_t buttonid) {
    deactivate();

    switch (buttonid) {
        case Buttons::YES: yesno_handler_(true); break;
        case Buttons::NO: yesno_handler_(false); break;
    }

    return Button::State::PRESSED;
}

UIEnterNumber::UIEnterNumber(std::string message,
                             std::function<void(int32_t)> nh,
                             int32_t m,
                             int32_t quantity) :
    UINotice(std::move(message), NoticeType::ENTER_NUMBER) {
    numhandler = std::move(nh);
    max = m;

    int16_t belowtext = field_y();
    int16_t pos_y = box2offset(true);

    nl::node src = nl::nx::ui["Basic.img"];

    buttons_[Buttons::OK] =
        std::make_unique<MapleButton>(src["BtOK"], 60, pos_y);
    buttons_[Buttons::CANCEL] =
        std::make_unique<MapleButton>(src["BtCancel"], 141, pos_y);

    numfield = Textfield(Text::Font::A11M,
                         Text::Alignment::LEFT,
                         Color::Name::BLACK,
                         Rectangle<int16_t>(20, 246, belowtext, belowtext + 18),
                         10);
    numfield.change_text(std::to_string(quantity));

    numfield.set_enter_callback(
        [&](std::string numstr) { handlestring(std::move(numstr)); });

    numfield.set_key_callback(KeyAction::Id::ESCAPE, [&]() { deactivate(); });

    numfield.set_state(Textfield::State::FOCUSED);
}

void UIEnterNumber::draw(float alpha) const {
    UINotice::draw(true);
    UIElement::draw(alpha);

    numfield.draw(position_);
}

void UIEnterNumber::update() {
    UIElement::update();

    numfield.update(position_);
}

Cursor::State UIEnterNumber::send_cursor(bool clicked,
                                         Point<int16_t> cursorpos) {
    if (numfield.get_state() == Textfield::State::NORMAL) {
        Cursor::State nstate = numfield.send_cursor(cursorpos, clicked);

        if (nstate != Cursor::State::IDLE) {
            return nstate;
        }
    }

    return UIElement::send_cursor(clicked, cursorpos);
}

void UIEnterNumber::send_key(int32_t keycode, bool pressed, bool escape) {
    if (keycode == KeyAction::Id::RETURN) {
        handlestring(numfield.get_text());
        deactivate();
    } else if (escape) {
        deactivate();
    }
}

UIElement::Type UIEnterNumber::get_type() const {
    return TYPE;
}

Button::State UIEnterNumber::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::OK: handlestring(numfield.get_text()); break;
        case Buttons::CANCEL: deactivate(); break;
    }

    return Button::State::NORMAL;
}

void UIEnterNumber::handlestring(const std::string &numstr) {
    int num = -1;
    bool has_only_digits =
        (numstr.find_first_not_of("0123456789") == std::string::npos);

    auto okhandler = [&](bool) {
        numfield.set_state(Textfield::State::FOCUSED);
        buttons_[Buttons::OK]->set_state(Button::State::NORMAL);
    };

    if (!has_only_digits) {
        numfield.set_state(Textfield::State::DISABLED);
        UI::get().emplace<UIOk>("숫자만 입력할 수 있습니다.", okhandler);
        return;
    }
    num = std::stoi(numstr);

    if (num < 1) {
        numfield.set_state(Textfield::State::DISABLED);
        UI::get().emplace<UIOk>(
            "1 이상의 숫자를 입력해 주세요.",
            okhandler);
        return;
    }
    if (num > max) {
        numfield.set_state(Textfield::State::DISABLED);
        UI::get().emplace<UIOk>(
            std::to_string(max) + " 이하의 숫자를 입력해 주세요.",
            okhandler);
        return;
    }
    numhandler(num);
    deactivate();

    buttons_[Buttons::OK]->set_state(Button::State::NORMAL);
}

UIOk::UIOk(std::string message, std::function<void(bool ok)> oh) :
    UINotice(std::move(message), NoticeType::OK) {
    okhandler_ = std::move(oh);

    nl::node src = nl::nx::ui["Basic.img"];

    buttons_[Buttons::OK] =
        std::make_unique<MapleButton>(src["BtOK"], 100, box2offset(false));
}

void UIOk::draw(float alpha) const {
    UINotice::draw(false);
    UIElement::draw(alpha);
}

void UIOk::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (keycode == KeyAction::Id::RETURN) {
            okhandler_(true);
            deactivate();
        } else if (escape) {
            okhandler_(false);
            deactivate();
        }
    }
}

UIElement::Type UIOk::get_type() const {
    return TYPE;
}

Button::State UIOk::button_pressed(uint16_t buttonid) {
    deactivate();

    switch (buttonid) {
        case Buttons::OK: okhandler_(true); break;
    }

    return Button::State::NORMAL;
}
}  // namespace ms