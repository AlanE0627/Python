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

#include <functional>

#include "../Components/Slider.h"
#include "../Components/Textfield.h"
#include "../../Character/Look/CharLook.h"
#include "../Graphics/Text.h"
#include "../UIElement.h"
#include "TextButton.h"

namespace ms {
class UINpcTalk : public UIElement {
public:
    enum TalkType : int8_t {
        NONE = -1,
        SEND_TEXT_ONLY,
        SEND_YESNO,
        SEND_SIMPLE = 4,
        SEND_ACCEPT_DECLINE = 12,
        // below are determined by 2 style bytes when msgtype = 0
        SEND_OK,
        SEND_NEXT,
        SEND_NEXT_PREV,
        SEND_PREV,
        SEND_GET_TEXT,
        SEND_GET_NUMBER,
        SEND_STYLE,
        LENGTH
    };

    struct NPCTalkContent {
        void  set_content(std::string& line);

        Text text_;
        std::vector<Text> text_content_;
        std::vector<TextButton> text_button_content_;
        Color::Name line_color_ = Color::Name::DARKGREY;
        int16_t selection_;
    };

    static constexpr Type TYPE = UIElement::Type::NPC_TALK;
    static constexpr bool FOCUSED = true;
    static constexpr bool TOGGLED = false;

    UINpcTalk();

    void draw(float inter) const override;

    void update() override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

    void change_text(int32_t npcid,
                     int8_t msgtype,
                     uint8_t style_b0,
                     uint8_t style_b1,
                     int8_t speaker,
                     const std::string &text);

    // maple-offline: client-side dialogs (quest menus, quest Say.img text)
    // answer through this callback instead of NPC_TALK_MORE.
    // action: -1 closed, 0 no/prev, 1 yes/next/ok; selection for lists.
    using LocalResponder = std::function<void(int8_t action, int32_t selection)>;
    void set_local(LocalResponder responder);

    // NPC asks for text (msgtype 2) or a number (msgtype 3): initial value
    void set_input_default(const std::string &text);

    // style choice (msgtype 7): hair / face / skin ids to preview and pick
    void set_styles(std::vector<int32_t> styles);

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    TalkType get_by_value(int8_t value, uint8_t style_b0, uint8_t style_b1);

    std::string format_text(const std::string &tx, const int32_t &npcid);

    NPCTalkContent convert_npc_text(std::string& formatted_text_);

    void reset_content_pos();

    void respond(int8_t type, int8_t action);
    void respond(int32_t selection);

    void submit_input();

    Textfield input_;
    bool has_input_ = false;
    std::vector<int32_t> styles_;
    size_t style_index_ = 0;
    CharLook style_look_;

    void show_style();
    int8_t raw_type_ = 0;

    LocalResponder local_;
    int16_t key_sel_ = 0;

    static constexpr int16_t MAX_HEIGHT_ = 248;

    enum Buttons {
        ALL_LEVEL,
        CLOSE,
        MY_LEVEL,
        NEXT,
        NO,
        OK,
        PREV,
        QAFTER,
        QCNO,
        QCYES,
        QGIVEUP,
        QNO,
        QSTART,
        QYES,
        YES,
        TEXT_BUTTON
    };

    Texture top_;
    Texture fill_;
    Texture bottom_;
    Texture nametag_;
    Texture speaker_;

    NPCTalkContent content_;
    Text name_;

    int16_t height_;
    int16_t offset_;
    int16_t unit_rows_;
    int16_t row_max_;
    int16_t min_height_;

    bool show_slider_;
    bool draw_text_;
    Slider slider_;
    TalkType type_;
    std::string formatted_text_;
    size_t formatted_text_pos_;
    uint16_t timestep_;

    std::function<void(bool)> onmoved_;
};
}  // namespace ms