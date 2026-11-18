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

#include <deque>

#include "../Components/Textfield.h"
#include "../Messages.h"
#include "../UIElement.h"

namespace ms {
// maple-offline: v83 chat. Input sits in the white strip at the top of the
// status bar (StatusBar.img/base/backgrnd2); the log is drawn just above it.
class UIChatBar : public UIElement {
public:
    static constexpr Type TYPE = UIElement::Type::CHAT_BAR;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = true;

    enum LineType { UNK0, WHITE, RED, BLUE, YELLOW, GREEN };

    UIChatBar();

    void draw(float inter) const override;

    void update() override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    bool is_in_range(Point<int16_t> cursorpos) const override;

    Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

    UIElement::Type get_type() const override;

    void send_chatline(const std::string &line, LineType type);

    void display_message(Messages::Type line, UIChatBar::LineType type);

    void toggle_chat();

    void toggle_chat(bool chat_open);

    void toggle_chatfield();

    void toggle_chatfield(bool chatfield_open);

    bool is_chat_open();

    bool is_chatfield_open();

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    int16_t visible_rows() const;

    static constexpr int16_t ROW_HEIGHT_ = 13;
    static constexpr int16_t ROWS_OPEN_ = 6;
    static constexpr int16_t ROWS_CLOSED_ = 2;
    static constexpr size_t MAX_LINES_ = 128;
    static constexpr time_t MESSAGE_COOLDOWN_ = 1'000;
    static constexpr int16_t LOG_WIDTH_ = 566;

    bool is_chat_open_;
    bool is_chatfield_open_;
    Texture log_edge_;
    Texture target_box_;
    Text target_label_;
    Textfield chatfield_;
    std::deque<Text> lines_;
    EnumMap<Messages::Type, time_t> message_cooldowns_;
    std::vector<std::string> last_entered_;
    size_t last_pos_;
};
}  // namespace ms
