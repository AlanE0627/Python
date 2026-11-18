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
#include "UIChannel.h"

#include <nlnx/nx.hpp>

#include "../../Gameplay/Stage.h"
#include "../Audio/Audio.h"
#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../KeyAction.h"

namespace ms {
UIChannel::UIChannel(uint8_t wid, uint8_t ch, uint8_t ch_count) :
    UIDragElement<PosCHANNEL>(),
    current_channel_(ch),
    selected_channel_(ch),
    channel_count_(ch_count) {
    uint8_t selected_world = wid;

    // maple-offline: v83 UIWindow.img/Channel — t (67) + c rows + s (36),
    // five channels per row, channel0 = normal, channel1 = current,
    // channel2 = selected.
    nl::node Channel = nl::nx::ui["UIWindow.img"]["Channel"];

    int16_t rows = static_cast<int16_t>((channel_count_ + 4) / 5);
    int16_t body = rows * 20 + 6;
    Texture top = Channel["t"];

    sprites_.emplace_back(Channel["t"]);
    sprites_.emplace_back(Channel["c"], DrawArgument(Point<int16_t>(0, 67), Point<int16_t>(0, body)));
    sprites_.emplace_back(Channel["s"], Point<int16_t>(0, 67 + body));
    sprites_.emplace_back(Channel["world"][selected_world], Point<int16_t>(16, 30));

    buttons_[Buttons::CHANGE] = std::make_unique<MapleButton>(
        Channel["BtChange"], Point<int16_t>(436 - 91 - 47 - 18, 67 + body + 10));
    buttons_[Buttons::CANCEL] = std::make_unique<MapleButton>(
        Channel["BtCancel"], Point<int16_t>(436 - 47 - 12, 67 + body + 10));

    channel_[true] = Channel["channel2"];
    channel_[false] = Channel["channel1"];
    normal_cell_ = Channel["channel0"];

    for (size_t i = 0; i < channel_count_; i++) {
        int16_t x = static_cast<int16_t>(i % 5);
        int16_t y = static_cast<int16_t>(i / 5);
        Point<int16_t> cell(CELL_X0 + CELL_DX * x, CELL_Y0 + 20 * y);
        Texture num = Channel["ch"][i];

        ch_.emplace_back(Channel["ch"][i], cell + Point<int16_t>(34 - num.width() / 2, 5));
        cells_.push_back(cell);
        buttons_[Buttons::CH + i] = std::make_unique<AreaButton>(cell, channel_[true].get_dimensions());

        if (i == selected_channel_) {
            current_channel_x_ = cell.x();
            current_channel_y_ = cell.y();
            selected_channel_x_ = current_channel_x_;
            selected_channel_y_ = current_channel_y_;
        }
    }

    dimension_ = Point<int16_t>(top.width(), 67 + body + 36);
    drag_area_ = Point<int16_t>(dimension_.x(), 20);

    old_state_ = { current_channel_, current_channel_x_, current_channel_y_ };
}

void UIChannel::draw(float inter) const {
    UIElement::draw_sprites(inter);

    for (const auto &cell : cells_) {
        normal_cell_.draw(position_ + cell);
    }

    if (current_channel_ == selected_channel_) {
        channel_[true].draw(DrawArgument(position_.x() + selected_channel_x_,
                                         position_.y() + selected_channel_y_));
    } else {
        channel_[true].draw(DrawArgument(position_.x() + selected_channel_x_,
                                         position_.y() + selected_channel_y_));
        channel_[false].draw(DrawArgument(position_.x() + current_channel_x_,
                                          position_.y() + current_channel_y_));
    }

    for (const auto &sprite : ch_) {
        sprite.draw(position_, inter);
    }

    UIElement::draw_buttons(inter);
}

void UIChannel::update() {
    UIElement::update();

    for (auto &sprite : ch_) {
        sprite.update();
    }
}

void UIChannel::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (escape) {
            cancel();
        } else if (keycode == KeyAction::Id::RETURN) {
            change_channel();
        } else if (keycode == KeyAction::Id::UP) {
            if (selected_channel_ > 4) {
                selected_channel_ -= 5;
            } else {
                for (size_t i = 0; i < 3; i++) {
                    selected_channel_ += 5;
                }
            }

            if (selected_channel_ == current_channel_) {
                if (selected_channel_ > 4) {
                    selected_channel_ -= 5;
                } else {
                    for (size_t i = 0; i < 3; i++) {
                        selected_channel_ += 5;
                    }
                }
            }

            update_selected_channel_position();
        } else if (keycode == KeyAction::Id::DOWN) {
            if (selected_channel_ < 15) {
                selected_channel_ += 5;
            } else {
                for (size_t i = 0; i < 3; i++) {
                    selected_channel_ -= 5;
                }
            }

            if (selected_channel_ == current_channel_) {
                if (selected_channel_ < 15) {
                    selected_channel_ += 5;
                } else {
                    for (size_t i = 0; i < 3; i++) {
                        selected_channel_ -= 5;
                    }
                }
            }

            update_selected_channel_position();
        } else if (keycode == KeyAction::Id::LEFT) {
            if (selected_channel_ != 0) {
                selected_channel_--;
            } else {
                selected_channel_ = channel_count_ - 1;
            }

            if (selected_channel_ == current_channel_) {
                if (selected_channel_ != 0) {
                    selected_channel_--;
                } else {
                    selected_channel_ = channel_count_ - 1;
                }
            }

            update_selected_channel_position();
        } else if (keycode == KeyAction::Id::RIGHT) {
            if (selected_channel_ != channel_count_ - 1) {
                selected_channel_++;
            } else {
                selected_channel_ = 0;
            }

            if (selected_channel_ == current_channel_) {
                if (selected_channel_ != channel_count_ - 1) {
                    selected_channel_++;
                } else {
                    selected_channel_ = 0;
                }
            }

            update_selected_channel_position();
        }
    }
}

Cursor::State UIChannel::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

    if (dragged_) {
        return dstate;
    }

    Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

    for (size_t i = 0; i < channel_count_ + Buttons::CH; i++) {
        if (buttons_[i]->is_active()
            && buttons_[i]->bounds(position_).contains(cursorpos)) {
            if (buttons_[i]->get_state() == Button::State::NORMAL) {
                if (i < Buttons::CH) {
                    Sound(Sound::Name::BUTTON_OVER).play();

                    buttons_[i]->set_state(Button::State::MOUSEOVER);
                    ret = Cursor::State::CAN_CLICK;
                } else {
                    buttons_[i]->set_state(Button::State::MOUSEOVER);
                    ret = Cursor::State::IDLE;
                }
            } else if (buttons_[i]->get_state() == Button::State::MOUSEOVER) {
                if (clicked) {
                    if (i < Buttons::CH) {
                        Sound(Sound::Name::BUTTON_CLICK).play();
                    }

                    buttons_[i]->set_state(button_pressed(i));

                    ret = Cursor::State::IDLE;
                } else {
                    if (i < Buttons::CH) {
                        ret = Cursor::State::CAN_CLICK;
                    } else {
                        ret = Cursor::State::IDLE;
                    }
                }
            }
        } else if (buttons_[i]->get_state() == Button::State::MOUSEOVER) {
            buttons_[i]->set_state(Button::State::NORMAL);
        }
    }

    return ret;
}

UIElement::Type UIChannel::get_type() const {
    return TYPE;
}

Button::State UIChannel::button_pressed(uint16_t buttonid) {
    if (buttonid < Buttons::CH) {
        switch (buttonid) {
            case Buttons::CANCEL: cancel(); break;
            case Buttons::CHANGE: change_channel(); break;
            default: break;
        }
    } else {
        if (buttonid - Buttons::CH == current_channel_) {
            return Button::State::NORMAL;
        }

        selected_channel_ = buttonid - Buttons::CH;
        update_selected_channel_position();
    }

    return Button::State::NORMAL;
}

void UIChannel::cancel() {
    deactivate();

    current_channel_ = old_state_.current_channel;
    selected_channel_ = old_state_.current_channel;
    selected_channel_x_ = old_state_.current_channel_x;
    selected_channel_y_ = old_state_.current_channel_y;
}

void UIChannel::change_channel() {
    deactivate();
    Stage::get().change_channel(selected_channel_);

    current_channel_ = selected_channel_;

    update_selected_channel_position();

    current_channel_x_ = selected_channel_x_;
    current_channel_y_ = selected_channel_y_;

    old_state_.current_channel = selected_channel_;
    old_state_.current_channel = selected_channel_;
    old_state_.current_channel_x = selected_channel_x_;
    old_state_.current_channel_y = selected_channel_y_;
}

void UIChannel::update_selected_channel_position() {
    size_t x = 0;
    size_t y = 0;

    for (size_t i = 0; i < channel_count_; i++) {
        if (x >= 5) {
            x = 0;
            y++;
        }

        if (i == selected_channel_) {
            selected_channel_x_ = CELL_X0 + CELL_DX * x;
            selected_channel_y_ = CELL_Y0 + 20 * y;
            break;
        }

        x++;
    }
}
}  // namespace ms