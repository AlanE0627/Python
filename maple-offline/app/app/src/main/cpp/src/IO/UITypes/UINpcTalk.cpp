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
#include "UINpcTalk.h"

#include <nlnx/nx.hpp>

#include "../Components/MapleButton.h"
#include "../Gameplay/Stage.h"
#include "../Net/Packets/NpcInteractionPackets.h"
#include "../UI.h"
#include "GameInfo.h"
#include <sstream>
#include <regex>

#include "../../Util/MapleText.h"
#include "../../Graphics/Geometry.h"
#include "../KeyAction.h"

namespace ms {
    namespace {
        auto fn_npc_talk_more = []<typename... T>(T &&...args) {
            NpcTalkMorePacket(std::forward<T>(args)...).dispatch();
        };
    }  // namespace

    UINpcTalk::UINpcTalk() :
            offset_(0),
            unit_rows_(0),
            row_max_(0),
            show_slider_(false),
            draw_text_(false),
            formatted_text_(""),
            formatted_text_pos_(0),
            timestep_(0) {
        // maple-offline: v83 dialog parts live in UIWindow.img/UtilDlgEx.
        nl::node UIWindow2 = nl::nx::ui_img("UIWindow.img");
        nl::node UtilDlgEx = UIWindow2["UtilDlgEx"];

        top_ = UtilDlgEx["t"];
        fill_ = UtilDlgEx["c"];
        bottom_ = UtilDlgEx["s"];
        nametag_ = UtilDlgEx["bar"];

        min_height_ = 8 * fill_.height() + 14;

        buttons_[Buttons::ALL_LEVEL] =
                std::make_unique<MapleButton>(UtilDlgEx["BtAllLevel"]);
        buttons_[Buttons::CLOSE] =
                std::make_unique<MapleButton>(UtilDlgEx["BtClose"]);
        buttons_[Buttons::MY_LEVEL] =
                std::make_unique<MapleButton>(UtilDlgEx["BtMyLevel"]);
        buttons_[Buttons::NEXT] =
                std::make_unique<MapleButton>(UtilDlgEx["BtNext"]);

        // TODO: Replace when _inlink is fixed
        // buttons[Buttons::NO] = std::make_unique<MapleButton>(UtilDlgEx["BtNo"]);

        nl::node Quest = UIWindow2["Quest"];

        buttons_[Buttons::NO] = std::make_unique<MapleButton>(UtilDlgEx["BtNo"]);
        buttons_[Buttons::OK] = std::make_unique<MapleButton>(UtilDlgEx["BtOK"]);
        buttons_[Buttons::PREV] =
                std::make_unique<MapleButton>(UtilDlgEx["BtPrev"]);
        buttons_[Buttons::QAFTER] =
                std::make_unique<MapleButton>(UtilDlgEx["BtQAfter"]);
        buttons_[Buttons::QCNO] =
                std::make_unique<MapleButton>(UtilDlgEx["BtQCNo"]);
        buttons_[Buttons::QCYES] =
                std::make_unique<MapleButton>(UtilDlgEx["BtQCYes"]);
        buttons_[Buttons::QGIVEUP] =
                std::make_unique<MapleButton>(UtilDlgEx["BtQGiveup"]);
        buttons_[Buttons::QNO] = std::make_unique<MapleButton>(UtilDlgEx["BtQNo"]);
        buttons_[Buttons::QSTART] =
                std::make_unique<MapleButton>(UtilDlgEx["BtQStart"]);
        buttons_[Buttons::QYES] =
                std::make_unique<MapleButton>(UtilDlgEx["BtQYes"]);
        buttons_[Buttons::YES] = std::make_unique<MapleButton>(UtilDlgEx["BtYes"]);

        name_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);

        onmoved_ = [&](bool upwards) {
            int16_t shift = upwards ? -unit_rows_ : unit_rows_;
            bool above = offset_ + shift >= 0;
            bool below = offset_ + shift <= row_max_ - unit_rows_;

            if (above && below) {
                offset_ += shift;
                reset_content_pos();
            }
        };

        UI::get().remove_textfield();
    }

    void UINpcTalk::draw(float inter) const {
        Point<int16_t> drawpos = position_;
        top_.draw(drawpos);
        drawpos.shift_y(top_.height());
        fill_.draw(DrawArgument(drawpos, Point<int16_t>(0, height_)));
        drawpos.shift_y(height_);
        bottom_.draw(drawpos);
        drawpos.shift_y(bottom_.height());

        UIElement::draw(inter);

        int16_t speaker_y = (top_.height() + height_ + bottom_.height()) / 2;
        Point<int16_t> speaker_pos = position_ + Point<int16_t>(22, 11 + speaker_y);
        Point<int16_t> center_pos =
                speaker_pos + Point<int16_t>(nametag_.width() / 2, 0);

        speaker_.draw(DrawArgument(center_pos, true));
        nametag_.draw(speaker_pos);
        name_.draw(center_pos + Point<int16_t>(0, -4));

        if (show_slider_) {
            int16_t text_min_height = position_.y() + top_.height() - 1;
            int init_height = 19;
            for (auto &element: content_.text_content_) {
                element.draw(position_ + Point<int16_t>(162, init_height - offset_ * 248),
                             Range<int16_t>(text_min_height, text_min_height + height_));
                init_height += element.height();
            }
            for (auto &element: content_.text_button_content_) {
                element.draw(position_ + Point<int16_t>(162, init_height - offset_ * 248),
                             Range<int16_t>(text_min_height, text_min_height + height_));
                init_height += element.height();
            }
            slider_.draw(position_);
        } else {
            if (has_input_) {
                ColorBox(304, 20, Color::Name::WHITE, 1.0f)
                    .draw(DrawArgument(position_ + Point<int16_t>(168, height_ + 15)));
                input_.draw(position_);
            }

            int16_t init_height = 19;
            for (auto &element: content_.text_content_) {
                element.draw(position_ + Point<int16_t>(166, init_height));
                init_height += element.height();
            }
            for (size_t i = 0; i < content_.text_button_content_.size(); i++) {
                auto &element = content_.text_button_content_[i];

                if (type_ == TalkType::SEND_SIMPLE && static_cast<int16_t>(i) == key_sel_) {
                    ColorBox(330, element.height(), Color::Name::MEDIUMBLUE, 0.15f)
                        .draw(DrawArgument(position_ + Point<int16_t>(160, init_height + 2)));
                }

                element.draw(position_ + Point<int16_t>(166, init_height));
                init_height += element.height();
            }
        }
    }

    void UINpcTalk::set_input_default(const std::string &text) {
        if (has_input_) {
            input_.change_text(text);
        }
    }

    void UINpcTalk::submit_input() {
        if (!has_input_) {
            return;
        }

        std::string text = input_.get_text();

        has_input_ = false;
        input_.set_state(Textfield::State::DISABLED);
        deactivate();

        if (raw_type_ == 3) {
            int32_t number = 0;

            try {
                number = std::stoi(text.empty() ? "0" : text);
            } catch (...) {
                number = 0;
            }

            NpcTalkNumberPacket(number).dispatch();
        } else {
            NpcTalkMorePacket(text).dispatch();
        }
    }

    void UINpcTalk::update() {
        UIElement::update();

        if (has_input_) {
            input_.update(position_);
        }

        // TODO: Need to think of a new way to show text gradually when open NPC dialog
//        if (draw_text_) {
//            if (timestep_ > 4) {
//                if (formatted_text_pos_ < formatted_text_.size()) {
//                    std::string t = content_.text_.get_text();
//                    char c = formatted_text_[formatted_text_pos_];
//
//                    content_.text_.change_text(t + c);
//
//                    formatted_text_pos_++;
//                    timestep_ = 0;
//                } else {
//                    draw_text_ = false;
//                }
//            } else {
//                timestep_++;
//            }
//        }
    }

    void UINpcTalk::set_local(LocalResponder responder) {
        local_ = std::move(responder);
    }

    void UINpcTalk::respond(int8_t type, int8_t action) {
        if (local_) {
            LocalResponder cb = std::move(local_);
            local_ = nullptr;
            cb(action, -1);
        } else {
            // the server wants the dialog's own msgtype back (0 for the
            // next/prev/ok pages), not this client's TalkType value: scripts
            // read "mode == 0 && type > 0" as declining and end the talk
            (void) type;
            fn_npc_talk_more(raw_type_, action);
        }
    }

    void UINpcTalk::respond(int32_t selection) {
#ifdef MS_DESKTOP_DEBUG
        fprintf(stderr, "[dbg] npctalk respond sel %d local %d\n", selection, (int)(bool)local_);
#endif
        if (local_) {
            LocalResponder cb = std::move(local_);
            local_ = nullptr;
            cb(1, selection);
        } else {
            fn_npc_talk_more(selection);
        }
    }

    Button::State UINpcTalk::button_pressed(uint16_t buttonid) {
        deactivate();

        // maple-offline: drop the option buttons first; a local responder
        // may put the next page into this same window before we return
        for (auto &element: content_.text_button_content_) {
            auto it = buttons_.find(Buttons::TEXT_BUTTON + element.get_option_number());
            if (it != buttons_.end()) {
                buttons_.erase(it);
            }
        }

        switch (type_) {
            case TalkType::SEND_NEXT:
            case TalkType::SEND_OK:
                // Type = 0
                switch (buttonid) {
                    case Buttons::CLOSE:
                        respond(type_, -1);
                        break;
                    case Buttons::NEXT:
                    case Buttons::OK:
                        respond(type_, 1);
                        break;
                }
                break;
            case TalkType::SEND_PREV:
                // Type = 0, previous page and OK (sendPrev)
                switch (buttonid) {
                    case Buttons::CLOSE:
                        respond(type_, -1);
                        break;
                    case Buttons::OK:
                        respond(type_, 1);
                        break;
                    case Buttons::PREV:
                        respond(type_, 0);
                        break;
                }
                break;
            case TalkType::SEND_NEXT_PREV:
                // Type = 0
                switch (buttonid) {
                    case Buttons::CLOSE:
                        respond(type_, -1);
                        break;
                    case Buttons::NEXT:
                        respond(type_, 1);
                        break;
                    case Buttons::PREV:
                        respond(type_, 0);
                        break;
                }
                break;
            case TalkType::SEND_YESNO:
                // Type = 1
                switch (buttonid) {
                    case Buttons::CLOSE:
                        respond(type_, -1);
                        break;
                    case Buttons::NO:
                        respond(type_, 0);
                        break;
                    case Buttons::YES:
                        respond(type_, 1);
                        break;
                }
                break;
            case TalkType::SEND_ACCEPT_DECLINE:
                // Type = 1
                switch (buttonid) {
                    case Buttons::CLOSE:
                        respond(type_, -1);
                        break;
                    case Buttons::QNO:
                        respond(type_, 0);
                        break;
                    case Buttons::QYES:
                        respond(type_, 1);
                        break;
                }
                break;
            case TalkType::SEND_GET_TEXT:
            case TalkType::SEND_GET_NUMBER:
                switch (buttonid) {
                    case Buttons::CLOSE:
                        input_.set_state(Textfield::State::DISABLED);
                        respond(raw_type_, 0);
                        break;
                    case Buttons::OK:
                        submit_input();
                        return Button::State::NORMAL;
                }
                break;
            case TalkType::SEND_SIMPLE:
                // Type = 4
                switch (buttonid) {
                    case Buttons::CLOSE:
                        respond(type_, 0);
                        break;
                    default:
                        // the button id already carries the option number
                        respond(static_cast<int32_t>(buttonid - Buttons::TEXT_BUTTON));
                        break;
                }
                break;
            default:
                break;
        }

        return Button::State::NORMAL;
    }

    Cursor::State UINpcTalk::send_cursor(bool clicked, Point<int16_t> cursorpos) {
        Point<int16_t> cursor_relative = cursorpos - position_;

        if (show_slider_ && slider_.isenabled()) {
            if (Cursor::State sstate =
                    slider_.send_cursor(cursor_relative, clicked)) {
                return sstate;
            }
        }

        // maple-offline: list options are matched directly; the generic
        // button pass does not report clicks on inactive text buttons
        if (clicked && !draw_text_ && type_ == TalkType::SEND_SIMPLE) {
            for (auto &element: content_.text_button_content_) {
                if (element.bounds(position_).contains(cursorpos)) {
                    button_pressed(Buttons::TEXT_BUTTON + element.get_option_number());
                    return Cursor::State::IDLE;
                }
            }
        }

        if (has_input_ && clicked
            && Rectangle<int16_t>(position_ + Point<int16_t>(168, height_ + 15),
                                  position_ + Point<int16_t>(472, height_ + 35))
                   .contains(cursorpos)) {
            input_.set_state(Textfield::State::FOCUSED);

            return Cursor::State::CLICKING;
        }

        Cursor::State estate = UIElement::send_cursor(clicked, cursorpos);

        if (estate == Cursor::State::CLICKING && clicked) {
            if (draw_text_) {
                draw_text_ = false;
                content_.text_.change_text(formatted_text_);
            } else {
                for (auto &element: content_.text_button_content_) {
                    if (element.bounds(position_).contains(cursorpos)) {
                        button_pressed(Buttons::TEXT_BUTTON + element.get_option_number());
                        break;
                    }
                }
            }
        }

        return estate;
    }

    void UINpcTalk::send_key(int32_t keycode, bool pressed, bool escape) {
        if (!pressed) {
            return;
        }

        if (escape) {
            deactivate();

            if (local_) {
                // closing a client-side dialog just ends it
                local_ = nullptr;
            } else {
                respond(type_, 0);
            }

            return;
        }

        // maple-offline: keyboard / controller (d-pad + confirm) navigation
        auto &options = content_.text_button_content_;

        if (keycode == KeyAction::Id::UP || keycode == KeyAction::Id::DOWN) {
            if (type_ == TalkType::SEND_SIMPLE && !options.empty()) {
                int16_t count = static_cast<int16_t>(options.size());
                int16_t step = keycode == KeyAction::Id::UP ? -1 : 1;
                key_sel_ = static_cast<int16_t>((key_sel_ + step + count) % count);
            }
        } else if (keycode == KeyAction::Id::RETURN) {
            if (draw_text_) {
                draw_text_ = false;
                content_.text_.change_text(formatted_text_);
                return;
            }

            switch (type_) {
                case TalkType::SEND_SIMPLE:
                    if (key_sel_ >= 0 && key_sel_ < static_cast<int16_t>(options.size())) {
                        button_pressed(Buttons::TEXT_BUTTON + options[key_sel_].get_option_number());
                    }
                    break;
                case TalkType::SEND_OK: button_pressed(Buttons::OK); break;
                case TalkType::SEND_NEXT:
                case TalkType::SEND_NEXT_PREV: button_pressed(Buttons::NEXT); break;
                case TalkType::SEND_YESNO: button_pressed(Buttons::YES); break;
                case TalkType::SEND_ACCEPT_DECLINE: button_pressed(Buttons::QYES); break;
                default: break;
            }
        }
    }

    UIElement::Type UINpcTalk::get_type() const {
        return TYPE;
    }

    UINpcTalk::TalkType UINpcTalk::get_by_value(int8_t value,
                                                uint8_t style_b0,
                                                uint8_t style_b1) {
        if (value > TalkType::NONE && value < TalkType::LENGTH) {
            if (value == 0) {
                bool is_send_ok = (style_b0 == 0) && (style_b1 == 0);
                bool is_next_prev = (style_b0 == 1) && (style_b1 == 1);
                bool is_next = (style_b0 == 0) && (style_b1 == 1);
                bool is_prev = (style_b0 == 1) && (style_b1 == 0);

                if (is_send_ok) {
                    return TalkType::SEND_OK;
                }
                if (is_next_prev) {
                    return TalkType::SEND_NEXT_PREV;
                }
                if (is_next) {
                    return TalkType::SEND_NEXT;
                }
                if (is_prev) {
                    return TalkType::SEND_PREV;
                }
            } else if (value == 4) {
                return TalkType::SEND_SIMPLE;
            } else if (value == 2) {
                return TalkType::SEND_GET_TEXT;
            } else if (value == 3) {
                return TalkType::SEND_GET_NUMBER;
            }
            return static_cast<TalkType>(value);
        }

        return TalkType::NONE;
    }

// TODO: Move this to GraphicsGL?
    std::string UINpcTalk::format_text(const std::string &tx,
                                       const int32_t &npcid) {
        // maple-offline: shared tag resolver (#p/#t/#m/#o/#h/#c ...)
        return MapleText::format(tx, true);
    }

    void UINpcTalk::change_text(int32_t npcid,
                                int8_t msgtype,
                                uint8_t style_b0,
                                uint8_t style_b1,
                                int8_t speakerbyte,
                                const std::string &tx) {
        type_ = get_by_value(msgtype, style_b0, style_b1);
        raw_type_ = msgtype;

        timestep_ = 0;
        // maple-offline: gradual text is disabled (see update()), so the
        // first click must not be spent on "show the rest of the text"
        draw_text_ = false;
        key_sel_ = 0;
        formatted_text_pos_ = 0;
        formatted_text_ = format_text(tx, npcid);
        content_ = convert_npc_text(formatted_text_);

        for (auto &element: content_.text_button_content_) {
            buttons_[Buttons::TEXT_BUTTON +
                     element.get_option_number()] = std::make_unique<TextButton>(element);
        }

        content_.text_ = Text(Text::Font::A18M,
                              Text::Alignment::LEFT,
                              Color::Name::DARKGREY,
                              formatted_text_,
                              320);

        int16_t text_height = content_.text_.height();

        content_.text_.change_text("");

        if (speakerbyte == 0) {
            std::string strid = std::to_string(npcid);
            strid.insert(0, 7 - strid.size(), '0');
            strid.append(".img");

            speaker_ = nl::nx::npc[strid]["stand"]["0"];

            std::string namestr =
                    nl::nx::string["Npc.img"][std::to_string(npcid)]["name"];
            name_.change_text(namestr);
        } else {
            speaker_ = Texture();
            name_.change_text("");
        }

        height_ = min_height_ + 20;
        show_slider_ = false;

        if (text_height > min_height_) {
            if (text_height > MAX_HEIGHT_) {
                height_ = MAX_HEIGHT_;
                show_slider_ = true;
                row_max_ = text_height / 248 + 1;
                unit_rows_ = 1;

                int16_t slider_y = top_.height() - 7;
                slider_ = Slider(Slider::Type::DEFAULT_SILVER,
                                 Range<int16_t>(slider_y, slider_y + height_),
                                 top_.width() - 26,
                                 unit_rows_,
                                 row_max_,
                                 onmoved_);
            } else {
                height_ = text_height;
            }
        }

        reset_content_pos();

        for (auto &button: buttons_) {
            button.second->set_active(false);
            button.second->set_state(Button::State::NORMAL);
        }

        int16_t y_cord = height_ + 48;

        buttons_[Buttons::CLOSE]->set_position(Point<int16_t>(9, y_cord));
        buttons_[Buttons::CLOSE]->set_active(true);

        switch (type_) {
            case TalkType::SEND_OK:
                buttons_[Buttons::OK]->set_position(Point<int16_t>(469, y_cord));
                buttons_[Buttons::OK]->set_active(true);
                break;
            case TalkType::SEND_YESNO: {
                Point<int16_t> yes_position = Point<int16_t>(389, y_cord);

                buttons_[Buttons::YES]->set_position(yes_position);
                buttons_[Buttons::NO]->set_position(yes_position
                                                    + Point<int16_t>(65, 0));
                buttons_[Buttons::YES]->set_active(true);
                buttons_[Buttons::NO]->set_active(true);
                break;
            }
            case TalkType::SEND_NEXT:
                buttons_[Buttons::NEXT]->set_position(
                        Point<int16_t>(461, y_cord - 30));
                buttons_[Buttons::NEXT]->set_active(true);
                break;
            case TalkType::SEND_PREV:
                buttons_[Buttons::PREV]->set_position(Point<int16_t>(367, y_cord));
                buttons_[Buttons::PREV]->set_active(true);
                buttons_[Buttons::OK]->set_position(Point<int16_t>(469, y_cord));
                buttons_[Buttons::OK]->set_active(true);
                break;
            case TalkType::SEND_NEXT_PREV:
                buttons_[Buttons::NEXT]->set_position(
                        Point<int16_t>(461, y_cord - 30));
                buttons_[Buttons::PREV]->set_position(Point<int16_t>(367, y_cord));
                buttons_[Buttons::NEXT]->set_active(true);
                buttons_[Buttons::PREV]->set_active(true);
                break;
            case TalkType::SEND_ACCEPT_DECLINE:
                buttons_[Buttons::QYES]->set_position(Point<int16_t>(386, y_cord));
                buttons_[Buttons::QNO]->set_position(Point<int16_t>(451, y_cord));
                buttons_[Buttons::QYES]->set_active(true);
                buttons_[Buttons::QNO]->set_active(true);
                break;
            case TalkType::SEND_GET_TEXT:
            case TalkType::SEND_GET_NUMBER:
                buttons_[Buttons::OK]->set_position(Point<int16_t>(469, y_cord));
                buttons_[Buttons::OK]->set_active(true);
                break;
            case TalkType::SEND_SIMPLE:
            default:
                break;
        }

        has_input_ = type_ == TalkType::SEND_GET_TEXT || type_ == TalkType::SEND_GET_NUMBER;

        if (has_input_) {
            input_ = Textfield(Text::Font::A13M,
                               Text::Alignment::LEFT,
                               Color::Name::BLACK,
                               Rectangle<int16_t>(172, 470, height_ + 16, height_ + 34),
                               type_ == TalkType::SEND_GET_NUMBER ? 10 : 24);
            input_.set_enter_callback([this](const std::string &) { submit_input(); });
            input_.set_state(Textfield::State::FOCUSED);
        } else {
            input_.set_state(Textfield::State::DISABLED);
        }

        position_ = Point<int16_t>(400 - top_.width() / 2, 240 - height_ / 2);
        dimension_ = Point<int16_t>(top_.width(), height_ + 120);
    }

    UINpcTalk::NPCTalkContent UINpcTalk::convert_npc_text(std::string &formatted_text) {
        std::string s = formatted_text;
        std::string t;

        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\r') {
                t += '\n';
                if ((i + 1 < s.size()) && (s[i + 1] == '\n')) {
                    i++;
                }
            } else {
                t += s[i];
            }
        }

        std::istringstream iss(t);
        std::string line;
        NPCTalkContent content;

        while (std::getline(iss, line, '\n')) {
            content.set_content(line);
        }

        return content;
    }

    void UINpcTalk::NPCTalkContent::set_content(std::string &line) {
        std::regex pattern(R"#(#L(\d{1,2})#)#");
        std::smatch match;

        if (std::regex_search(line, match, pattern)) {
            if (line.find("#b") != std::string::npos) {
                line_color_ = Color::Name::BLUE;
            }
            selection_ = std::stoi(match[1].str());
            // maple-offline: the option markers are not part of the label
            line = std::regex_replace(line, pattern, "");
            line = std::regex_replace(line, std::regex("#l"), "");
            TextButton button = TextButton(Text(Text::Font::A18M,
                                                Text::Alignment::LEFT,
                                                line_color_,
                                                line,
                                                320), selection_);
            text_button_content_.emplace_back(button);
        } else {
            text_content_.emplace_back(Text::Font::A18M,
                                       Text::Alignment::LEFT,
                                       Color::Name::DARKGREY,
                                       line,
                                       320);
            if (line.rfind("#b") == (line.length() - 2)) {
                line_color_ = Color::Name::BLUE;
            }
        }
    }

    void UINpcTalk::reset_content_pos() {
        if (show_slider_) {
            int init_height = 19;
            for (auto &element: content_.text_content_) {
                init_height += element.height();
            }
            for (auto &element: content_.text_button_content_) {
                element.set_position(Point<int16_t>(162, init_height - offset_ * 248));
                init_height += element.height();
            }
        } else {
            int16_t init_height = 19;
            for (auto &element: content_.text_content_) {
                init_height += element.height();
            }
            for (auto &element: content_.text_button_content_) {
                element.set_position(Point<int16_t>(166, init_height));
                init_height += element.height();
            }
        }
    }
}  // namespace ms