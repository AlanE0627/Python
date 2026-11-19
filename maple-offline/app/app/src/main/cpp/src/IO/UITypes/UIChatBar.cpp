// maple-offline: v83 chat bar. See UIChatBar.h.
#include "UIChatBar.h"

#include <cstdio>

#include <nlnx/nx.hpp>

#include "../../Configuration.h"
#include "../../Constants.h"
#include "../../Graphics/Geometry.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../../Gameplay/Stage.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ms {
namespace {
auto fn_general_chat = []<typename... T>(T && ...args) {
    GeneralChatPacket(std::forward<T>(args)...).dispatch();
};

// The v83 client turns a chat line that matches one of the pet's command
// words (String/PetDialog.img/<id>/c<n>, '|' separated) into PET_COMMAND,
// using the interact entry whose level band l0..l1 covers the pet's level.
void try_pet_commands(std::string msg) {
    std::transform(msg.begin(), msg.end(), msg.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    Player &player = Stage::get().get_player();

    for (uint8_t i = 0; i < 3; i++) {
        PetLook *pet = player.get_pet(i);

        if (!pet) {
            continue;
        }

        std::string sid = std::to_string(pet->get_itemid());
        uint8_t level = player.get_inventory().get_pet_level(pet->get_itemid());
        nl::node interact = nl::nx::item["Pet"][sid + ".img"]["interact"];
        nl::node dialog = nl::nx::string["PetDialog.img"][sid];

        for (const auto &entry : interact) {
            int32_t l0 = entry["l0"].get_integer(1);
            int32_t l1 = entry["l1"].get_integer(30);

            if (level < l0 || level > l1) {
                continue;
            }

            std::string words = dialog[entry["command"].get_string()].get_string();
            std::stringstream ss(words);
            std::string word;
            bool match = false;

            while (std::getline(ss, word, '|')) {
                match = match || (!word.empty() && word == msg);
            }

            if (match) {
                PetCommandPacket(pet->get_uniqueid(),
                                 static_cast<uint8_t>(std::stoi(entry.name())))
                    .dispatch();
                break;
            }
        }
    }
}
}  // namespace

UIChatBar::UIChatBar() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(570, 30)) {
    is_chat_open_ = Setting<Chatopen>::get().load();
    is_chatfield_open_ = false;
    last_pos_ = 0;

    int16_t vh = Constants::Constants::get().get_viewheight();
    position_ = Point<int16_t>(0, vh - 71);

    nl::node base = nl::nx::ui_img("StatusBar.img")["base"];
    log_edge_ = base["chat"];
    target_box_ = base["chatTarget"];
    target_label_ = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "전체");

    chatfield_ = Textfield(Text::Font::A12M,
                           Text::Alignment::LEFT,
                           Color::Name::BLACK,
                           Rectangle<int16_t>(Point<int16_t>(90, 7), Point<int16_t>(556, 25)),
                           0);
    chatfield_.set_state(Textfield::State::DISABLED);

    chatfield_.set_enter_callback([&](std::string msg) {
        size_t last = msg.find_last_not_of(' ');

        if (last != std::string::npos) {
            msg.erase(last + 1);
            fn_general_chat(msg, true);
            try_pet_commands(msg);
            last_entered_.push_back(msg);
            last_pos_ = last_entered_.size();
        }

        chatfield_.change_text("");
        toggle_chatfield(false);
    });

    chatfield_.set_key_callback(KeyAction::Id::UP, [&]() {
        if (last_pos_ > 0) {
            last_pos_--;
            chatfield_.change_text(last_entered_[last_pos_]);
        }
    });

    chatfield_.set_key_callback(KeyAction::Id::DOWN, [&]() {
        if (!last_entered_.empty() && last_pos_ < last_entered_.size() - 1) {
            last_pos_++;
            chatfield_.change_text(last_entered_[last_pos_]);
        }
    });

    chatfield_.set_key_callback(KeyAction::Id::ESCAPE, [&]() { toggle_chatfield(false); });

    // maple-offline: no client-made English greeting (not in the original)

    dimension_ = Point<int16_t>(570, 30);
}

int16_t UIChatBar::visible_rows() const {
    int16_t rows = is_chat_open_ || is_chatfield_open_ ? ROWS_OPEN_ : ROWS_CLOSED_;

    return std::min<int16_t>(rows, static_cast<int16_t>(lines_.size()));
}

void UIChatBar::draw(float inter) const {
    // Log above the bar: newest line at the bottom, translucent backing.
    int16_t rows = visible_rows();

    if (rows > 0) {
        int16_t height = rows * ROW_HEIGHT_ + 6;
        Point<int16_t> top = position_ - Point<int16_t>(0, height);

        ColorBox(LOG_WIDTH_, height, Color::Name::BLACK, 0.35f).draw(DrawArgument(top));
        log_edge_.draw(top - Point<int16_t>(0, 5));

        int16_t y = -3 - ROW_HEIGHT_;

        for (int16_t i = 0; i < rows; i++) {
            const Text &line = lines_[lines_.size() - 1 - i];
            line.draw(position_ + Point<int16_t>(6, y - 2));
            y -= ROW_HEIGHT_;
        }
    }

    target_box_.draw(position_ + Point<int16_t>(5, 8));
    target_label_.draw(position_ + Point<int16_t>(45, 9));

    chatfield_.draw(position_);

    UIElement::draw_buttons(inter);
}

void UIChatBar::update() {
    UIElement::update();

    chatfield_.update(position_);

    for (auto iter : message_cooldowns_) {
        iter.second -= Constants::TIMESTEP;
    }
}

void UIChatBar::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (keycode == KeyAction::Id::RETURN) {
            toggle_chatfield();
        } else if (escape) {
            toggle_chatfield(false);
        }
    }
}

bool UIChatBar::is_in_range(Point<int16_t> cursorpos) const {
    Rectangle<int16_t> bounds(position_ + Point<int16_t>(0, 4), position_ + Point<int16_t>(570, 30));

    return bounds.contains(cursorpos);
}

Cursor::State UIChatBar::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    if (chatfield_.get_bounds().contains(cursorpos)) {
        if (clicking && !is_chatfield_open_) {
            toggle_chatfield(true);
        }

        return Cursor::State::IDLE;
    }

    return UIElement::send_cursor(clicking, cursorpos);
}

UIElement::Type UIChatBar::get_type() const {
    return TYPE;
}

void UIChatBar::send_chatline(const std::string &line, LineType type) {
    Color::Name color = Color::Name::WHITE;

    switch (type) {
        case LineType::RED: color = Color::Name::RED; break;
        case LineType::BLUE: color = Color::Name::MEDIUMBLUE; break;
        case LineType::YELLOW: color = Color::Name::YELLOW; break;
        case LineType::GREEN: color = Color::Name::GREEN; break;
        default: color = Color::Name::WHITE; break;
    }

    // One Text per wrapped row keeps the row layout simple.
    Text probe(Text::Font::A11M, Text::Alignment::LEFT, color, line, 0);

    if (probe.width() <= LOG_WIDTH_ - 12) {
        lines_.emplace_back(probe);
    } else {
        std::string rest = line;

        while (!rest.empty()) {
            size_t n = rest.size();
            Text part(Text::Font::A11M, Text::Alignment::LEFT, color, rest.substr(0, n), 0);

            while (n > 1 && part.width() > LOG_WIDTH_ - 12) {
                n--;

                // Do not split a UTF-8 sequence.
                while (n > 1 && (static_cast<unsigned char>(rest[n]) & 0xC0) == 0x80) {
                    n--;
                }

                part.change_text(rest.substr(0, n));
            }

            lines_.emplace_back(part);
            rest = rest.substr(n);
        }
    }

    while (lines_.size() > MAX_LINES_) {
        lines_.pop_front();
    }
}

void UIChatBar::display_message(Messages::Type line, UIChatBar::LineType type) {
    if (message_cooldowns_[line] > 0) {
        return;
    }

    send_chatline(Messages::messages[line], type);

    message_cooldowns_[line] = MESSAGE_COOLDOWN_;
}

void UIChatBar::toggle_chat() {
    toggle_chat(!is_chat_open_);
}

void UIChatBar::toggle_chat(bool chat_open) {
    is_chat_open_ = chat_open;
    Setting<Chatopen>::get().save(is_chat_open_);
}

void UIChatBar::toggle_chatfield() {
    toggle_chatfield(!is_chatfield_open_);
}

void UIChatBar::toggle_chatfield(bool chatfield_open) {
    is_chatfield_open_ = chatfield_open;

    if (is_chatfield_open_) {
        chatfield_.set_state(Textfield::State::FOCUSED);
    } else {
        // DISABLED also releases keyboard focus; NORMAL kept the field
        // focused in the UI, so Ctrl/Alt never reached the game after a chat.
        chatfield_.set_state(Textfield::State::DISABLED);
    }
}

bool UIChatBar::is_chat_open() {
    return is_chat_open_;
}

bool UIChatBar::is_chatfield_open() {
    return is_chatfield_open_;
}

Button::State UIChatBar::button_pressed(uint16_t) {
    return Button::State::NORMAL;
}
}  // namespace ms
