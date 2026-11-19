#include <cstdio>
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
#include "UICharSelect.h"

#include "../../Audio/Audio.h"
#include "../../Character/Job.h"
#include "../../Configuration.h"
#include "../../Net/Packets/SelectCharPackets.h"
#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "UILoginNotice.h"
#include "UIRaceSelect.h"
#include "UISoftKey.h"
#include "UIWorldSelect.h"
#include "../../LoginScene.h"
#include "../Components/TwoSpriteButton.h"

#define NOMINMAX
#ifdef WIN32
#include <windows.h>
#endif

#include <nlnx/nx.hpp>
#include <utility>

namespace ms {
namespace {
auto fn_select_char_pic = []<typename... T>(T && ...args) {
    SelectCharPicPacket(std::forward<T>(args)...).dispatch();
};

auto fn_select_char = []<typename... T>(T && ...args) {
    SelectCharPacket(std::forward<T>(args)...).dispatch();
};

auto fn_delete_char_pic = []<typename... T>(T && ...args) {
    DeleteCharPicPacket(std::forward<T>(args)...).dispatch();
};

auto fn_delete_char = []<typename... T>(T && ...args) {
    DeleteCharPacket(std::forward<T>(args)...).dispatch();
};

auto fn_register_pic = []<typename... T>(T && ...args) {
    RegisterPicPacket(std::forward<T>(args)...).dispatch();
};
}  // namespace

namespace {
// v83 world names by world id (Login.img/WorldSelect/world/<id>).
const char *const WORLD_NAMES[] = { "Scania",  "Bera",   "Broa",    "Windia",
                                    "Khaini",  "Bellocan", "Mardia", "Kradia",
                                    "Yellonde", "Demethos", "Galicia", "El Nido",
                                    "Zenith",  "Arcania", "Chaos",   "Nova",
                                    "Renegades" };

// Frame-relative layout of the v83 character-select stop (800x600 frame).
// Signboard obj (login/CharSelect/signboard/0, map 253,-1258, origin 66,119)
// has its top-left at frame (587,123); buttons sit centred on its planks.
const Point<int16_t> BT_SELECT_POS = Point<int16_t>(606, 139);
const Point<int16_t> BT_NEW_POS = Point<int16_t>(605, 179);
const Point<int16_t> BT_DELETE_POS = Point<int16_t>(604, 226);
const Point<int16_t> PAGE_L_POS = Point<int16_t>(215, 470);
const Point<int16_t> PAGE_R_POS = Point<int16_t>(520, 470);
const Point<int16_t> SCROLL_POS = Point<int16_t>(345, 40);
const Point<int16_t> CHARINFO_OFF = Point<int16_t>(17, 24);
const int16_t SLOT_X[3] = { 265, 380, 495 };
const int16_t SLOT_FEET_Y = 357;
}  // namespace

UICharSelect::UICharSelect(std::vector<CharEntry> c,
                           int8_t char_count,
                           int32_t s,
                           int8_t rp) :
    UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600)),
    characters_(std::move(c)),
    characters_count_(char_count),
    slots_(s),
    require_pic_(rp) {
    LoginScene::get().set_step(LoginScene::CHAR_SELECT);
    position_ = LoginScene::get().frame_origin();

    version_ = Text(Text::Font::A11M,
                    Text::Alignment::LEFT,
                    Color::Name::LEMONGRASS,
                    "Ver. " + Configuration::get().get_version());

    selected_character_ = Setting<DefaultCharacter>::get().load();
    selected_page = selected_character_ / PAGESIZE_;
    page_count_ = std::max<int>(1, std::ceil((double)slots_ / (double)PAGESIZE_));

    nl::node Login = nl::nx::ui_img("Login.img");
    nl::node Common = Login["Common"];
    nl::node CharSelect = Login["CharSelect"];

    sprites_.emplace_back(Common["frame"], Point<int16_t>(400, 300));
    sprites_.emplace_back(Common["step"]["2"], Point<int16_t>(40, 0));

    world_plaque_ = Common["selectWorld"];
    uint8_t world_id = Configuration::get().get_worldid();
    uint8_t channel_id = Configuration::get().get_channelid();
    std::string world_name = world_id < sizeof(WORLD_NAMES) / sizeof(*WORLD_NAMES)
                                 ? WORLD_NAMES[world_id]
                                 : "World " + std::to_string(world_id);
    world_label_ = Text(Text::Font::A12B,
                        Text::Alignment::CENTER,
                        Color::Name::WHITE,
                        world_name + "  Ch." + std::to_string(channel_id + 1));

    scroll_pos_ = SCROLL_POS;
    scroll_open_ = CharSelect["scroll"]["0"];
    scroll_full_ = CharSelect["scroll"]["0"]["3"];
    scroll_done_ = false;
    charinfo_ = CharSelect["charInfo"];

    nametag_ = CharSelect["nameTag"];

    buttons_[Buttons::CHARACTER_SELECT] =
        std::make_unique<MapleButton>(CharSelect["BtSelect"], BT_SELECT_POS);
    buttons_[Buttons::CHARACTER_NEW] =
        std::make_unique<MapleButton>(CharSelect["BtNew"], BT_NEW_POS);
    buttons_[Buttons::CHARACTER_DELETE] =
        std::make_unique<MapleButton>(CharSelect["BtDelete"], BT_DELETE_POS);
    buttons_[Buttons::PAGE_LEFT] =
        std::make_unique<TwoSpriteButton>(CharSelect["pageL"]["0"]["0"],
                                          CharSelect["pageL"]["1"]["0"],
                                          PAGE_L_POS);
    buttons_[Buttons::PAGE_RIGHT] =
        std::make_unique<TwoSpriteButton>(CharSelect["pageR"]["0"]["0"],
                                          CharSelect["pageR"]["1"]["0"],
                                          PAGE_R_POS);
    buttons_[Buttons::BACK] =
        std::make_unique<MapleButton>(Common["BtStart"], Point<int16_t>(0, 515));

    for (size_t i = 0; i < PAGESIZE_; i++) {
        Point<int16_t> feet = slot_feet(i);
        buttons_[Buttons::CHARACTER_SLOT0 + i] = std::make_unique<AreaButton>(
            feet - Point<int16_t>(30, 80),
            Point<int16_t>(60, 85));
    }

    for (size_t i = 0; i < InfoLabel::NUM_LABELS; i++) {
        info_labels_[i] = Text(Text::Font::A11M,
                               Text::Alignment::CENTER,
                               Color::Name::BLACK);
    }

    for (auto &entry : characters_) {
        char_looks_.emplace_back(entry.look);
        nametags_.emplace_back(nametag_, Text::Font::A12M, entry.stats.name);
    }

    empty_slot_glow_ = CharSelect["character"]["0"];
    empty_slot_ = CharSelect["character"]["1"]["0"];
    selected_effect_ = CharSelect["effect"]["0"];
    selected_beam_ = CharSelect["effect"]["1"];

    update_buttons();

    if (characters_count_ > 0) {
        if (selected_character_ < characters_count_) {
            update_selected_character();
        } else {
            select_last_slot();
        }
    }

    if (Configuration::get().get_auto_login()) {
        fn_select_char_pic(Configuration::get().get_auto_pic(),
                           Configuration::get().get_auto_cid());
    }

    dimension_ = Point<int16_t>(800, 600);
}

Point<int16_t> UICharSelect::slot_feet(size_t slot) const {
    return Point<int16_t>(SLOT_X[slot % PAGESIZE_], SLOT_FEET_Y);
}

void UICharSelect::draw(float inter) const {
    UIElement::draw_sprites(inter);

    version_.draw(position_ + Point<int16_t>(707, 1));

    world_plaque_.draw(position_ + Point<int16_t>(196, 0));
    world_label_.draw(position_ + Point<int16_t>(196 + 98, 18));

    for (int i = 0; i < PAGESIZE_; i++) {
        size_t index = i + selected_page * PAGESIZE_;
        Point<int16_t> feet = position_ + slot_feet(i);

        if (index < static_cast<size_t>(characters_count_)) {
            bool selected = index == selected_character_;

            // effect/0 (sparkles) and effect/1 (light beam) share one anchor
            // above the head; the beam's last frame reaches the feet.
            Point<int16_t> anchor = feet - Point<int16_t>(0, 337);

            if (selected) {
                selected_beam_.draw(anchor, inter);
            }

            char_looks_[index].draw(DrawArgument(feet, false), inter);

            if (selected) {
                selected_effect_.draw(anchor, inter);
            }

            nametags_[index].draw(feet);
        } else if (index < static_cast<size_t>(slots_)) {
            empty_slot_glow_.draw(feet, inter);
            empty_slot_.draw(feet);
        }
    }

    if (selected_character_ < characters_count_
        && selected_character_ / PAGESIZE_ == selected_page) {
        Point<int16_t> spos = position_ + scroll_pos_;

        if (!scroll_done_) {
            scroll_open_.draw(spos, inter);
        } else {
            scroll_full_.draw(spos);

            Point<int16_t> ipos = spos + CHARINFO_OFF;
            charinfo_.draw(ipos + charinfo_.get_origin());

            static const Point<int16_t> centers[InfoLabel::NUM_LABELS] = {
                Point<int16_t>(109, -3), // JOB
                Point<int16_t>(64, 14),  // LV
                Point<int16_t>(154, 14), // FAME
                Point<int16_t>(64, 32),  // STR
                Point<int16_t>(154, 32), // INT
                Point<int16_t>(64, 50),  // DEX
                Point<int16_t>(154, 50), // LUK
                Point<int16_t>(91, 94)   // world ranking
            };

            for (size_t i = 0; i < InfoLabel::NUM_LABELS; i++) {
                info_labels_[i].draw(ipos + centers[i]);
            }
        }
    }

    UIElement::draw_buttons(inter);
}

void UICharSelect::update() {
    UIElement::update();

    for (auto &charlook : char_looks_) {
        charlook.update(Constants::TIMESTEP);
    }

    if (!scroll_done_ && scroll_open_.update()) {
        scroll_done_ = true;
    }

    selected_effect_.update();
    selected_beam_.update();
    empty_slot_glow_.update();
}

void UICharSelect::doubleclick(Point<int16_t> cursorpos) {
    if (selected_character_ / PAGESIZE_ != selected_page) {
        return;
    }

    uint16_t button_index =
        selected_character_ % PAGESIZE_ + Buttons::CHARACTER_SLOT0;
    auto &btit = buttons_[button_index];

    if (btit->is_active() && btit->bounds(position_).contains(cursorpos)
        && selected_character_ < characters_count_) {
        button_pressed(Buttons::CHARACTER_SELECT);
    }
}

Cursor::State UICharSelect::send_cursor(bool clicked,
                                        Point<int16_t> cursorpos) {
    Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

    for (auto &[btnid, button] : buttons_) {
        if (button->is_active()
            && button->bounds(position_).contains(cursorpos)) {
            if (button->get_state() == Button::State::NORMAL) {
                Sound(Sound::Name::BUTTON_OVER).play();

                button->set_state(Button::State::MOUSEOVER);
                ret = Cursor::State::CAN_CLICK;
            } else if (button->get_state() == Button::State::MOUSEOVER) {
                if (clicked) {
                    Sound(Sound::Name::BUTTON_CLICK).play();

                    button->set_state(button_pressed(btnid));

                    ret = Cursor::State::IDLE;
                } else {
                    ret = Cursor::State::CAN_CLICK;
                }
            }
        } else if (button->get_state() == Button::State::MOUSEOVER) {
            button->set_state(Button::State::NORMAL);
        }
    }

    return ret;
}

void UICharSelect::send_key(int32_t keycode, bool pressed, bool escape) {
    if (!pressed) {
        return;
    }

    if (escape) {
        button_pressed(Buttons::BACK);
    } else if (keycode == KeyAction::Id::RETURN) {
        if (selected_character_ < characters_count_) {
            button_pressed(Buttons::CHARACTER_SELECT);
        }
    } else if (keycode == KeyAction::Id::LEFT) {
        if (selected_character_ > 0 && selected_character_ <= characters_count_) {
            uint8_t next = selected_character_ - 1;

            if (next / PAGESIZE_ != selected_page) {
                selected_page = next / PAGESIZE_;
                update_buttons();
            }

            button_pressed(next % PAGESIZE_ + Buttons::CHARACTER_SLOT0);
        }
    } else if (keycode == KeyAction::Id::RIGHT) {
        if (selected_character_ + 1 < characters_count_) {
            uint8_t next = selected_character_ + 1;

            if (next / PAGESIZE_ != selected_page) {
                selected_page = next / PAGESIZE_;
                update_buttons();
            }

            button_pressed(next % PAGESIZE_ + Buttons::CHARACTER_SLOT0);
        }
    }
}

UIElement::Type UICharSelect::get_type() const {
    return TYPE;
}

void UICharSelect::add_character(CharEntry &&character) {
    char_looks_.emplace_back(character.look);
    nametags_.emplace_back(nametag_, Text::Font::A12M, character.stats.name);
    characters_.emplace_back(std::forward<CharEntry>(character));

    characters_count_++;

    if (characters_count_ == 1) {
        update_buttons();
        update_selected_character();
    }
}

void UICharSelect::post_add_character() {
    // Show the page that holds the new character and select it.
    selected_page = (characters_count_ - 1) / PAGESIZE_;

    update_buttons();
    select_last_slot();

    LoginScene::get().set_step(LoginScene::CHAR_SELECT);
    makeactive();
}

void UICharSelect::remove_character(int32_t id) {
    for (size_t i = 0; i < characters_.size(); i++) {
        if (characters_[i].id == id) {
            char_looks_.erase(char_looks_.begin() + i);
            nametags_.erase(nametags_.begin() + i);
            characters_.erase(characters_.begin() + i);

            characters_count_--;

            if (selected_page > 0 && characters_count_ > 0
                && (characters_count_ - 1) / PAGESIZE_ < selected_page) {
                selected_page = (characters_count_ - 1) / PAGESIZE_;
            }

            update_buttons();

            if (selected_character_ < characters_count_) {
                update_selected_character();
            } else {
                select_last_slot();
            }

            return;
        }
    }
}

const CharEntry &UICharSelect::get_character(int32_t id) {
    for (auto &character : characters_) {
        if (character.id == id) {
            return character;
        }
    }

    std::cout << "Invalid character id: [" << id << "]" << std::endl;

    static const CharEntry null_character = { {}, {}, 0 };

    return null_character;
}

bool UICharSelect::update_character(int32_t id, const StatsEntry &stats) {
    for (auto &character : characters_) {
        if (character.id == id) {
            character.stats = stats;
            return true;
        }
    }

    return false;
}

Button::State UICharSelect::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::CHARACTER_SELECT: {
            if (selected_character_ >= characters_count_) {
                break;
            }

            Setting<DefaultCharacter>::get().save(selected_character_);
            int32_t id = characters_[selected_character_].id;

            switch (require_pic_) {
                case 0: {
                    std::function<void()> onok = [&]() { request_pic(); };

                    UI::get().emplace<UILoginNotice>(
                        UILoginNotice::Message::PIC_REQ,
                        onok);
                } break;
                case 1: {
                    std::function<void(const std::string &)> onok =
                        [id](const std::string &pic) {
                            fn_select_char_pic(pic, id);
                        };

                    UI::get().emplace<UISoftKey>(onok);
                } break;
                default: fn_select_char(id); break;
            }
        } break;
        case Buttons::CHARACTER_NEW:
            Sound(Sound::Name::SCROLL_UP).play();

            deactivate();

            UI::get().emplace<UIRaceSelect>();
            break;
        case Buttons::CHARACTER_DELETE: {
            if (selected_character_ >= characters_count_) {
                break;
            }

            int32_t id = characters_[selected_character_].id;

            switch (require_pic_) {
                case 0:
                    UI::get().emplace<UILoginNotice>(
                        UILoginNotice::Message::CHAR_DEL_FAIL_NO_PIC);
                    break;
                case 1: {
                    std::function<void()> onok = [id]() {
                        std::function<void(const std::string &)> onpic =
                            [id](const std::string &pic) {
                                fn_delete_char_pic(pic, id);
                            };

                        UI::get().emplace<UISoftKey>(onpic);
                    };

                    UI::get().emplace<UILoginNotice>(
                        UILoginNotice::Message::DELETE_CONFIRMATION,
                        onok);
                } break;
                default: {
                    // No PIC in single-player: confirm, then delete.
                    std::function<void()> onok = [id]() { fn_delete_char(id); };

                    UI::get().emplace<UILoginNotice>(
                        UILoginNotice::Message::DELETE_CONFIRMATION,
                        onok);
                } break;
            }
        } break;
        case Buttons::PAGE_LEFT:
            if (page_count_ > 1) {
                selected_page = selected_page > 0 ? selected_page - 1 : page_count_ - 1;
                update_buttons();
                select_last_slot();
            }
            break;
        case Buttons::PAGE_RIGHT:
            if (page_count_ > 1) {
                selected_page = selected_page + 1 < page_count_ ? selected_page + 1 : 0;
                update_buttons();

                if (selected_page * PAGESIZE_ < characters_count_) {
                    button_pressed(Buttons::CHARACTER_SLOT0);
                }
            }
            break;
        case Buttons::BACK:
            deactivate();

            Sound(Sound::Name::SCROLL_UP).play();

            if (auto worldselect = UI::get().get_element<UIWorldSelect>()) {
                worldselect->get().makeactive();
                LoginScene::get().set_step(LoginScene::WORLD_SELECT);
            }

            break;
        default:
            if (buttonid >= Buttons::CHARACTER_SLOT0) {
                uint8_t previous_character = selected_character_;
                uint8_t next = buttonid - Buttons::CHARACTER_SLOT0
                               + selected_page * PAGESIZE_;

                if (next >= characters_count_) {
                    break;
                }

                selected_character_ = next;

                if (previous_character != selected_character_) {
                    if (previous_character < characters_count_) {
                        char_looks_[previous_character].set_stance(
                            Stance::Id::STAND1);
                        nametags_[previous_character].set_selected(false);
                    }

                    update_selected_character();
                }
            }

            break;
    }

    return Button::State::NORMAL;
}

void UICharSelect::update_buttons() {
    for (int i = 0; i < PAGESIZE_; i++) {
        size_t index = i + selected_page * PAGESIZE_;

        buttons_[Buttons::CHARACTER_SLOT0 + i]->set_state(
            index < static_cast<size_t>(characters_count_)
                ? Button::State::NORMAL
                : Button::State::DISABLED);
    }

    buttons_[Buttons::CHARACTER_NEW]->set_state(
        characters_count_ >= slots_ ? Button::State::DISABLED
                                    : Button::State::NORMAL);

    bool has_character = characters_count_ > 0;

    buttons_[Buttons::CHARACTER_SELECT]->set_state(
        has_character ? Button::State::NORMAL : Button::State::DISABLED);
    buttons_[Buttons::CHARACTER_DELETE]->set_state(
        has_character ? Button::State::NORMAL : Button::State::DISABLED);

    buttons_[Buttons::PAGE_LEFT]->set_active(page_count_ > 1);
    buttons_[Buttons::PAGE_RIGHT]->set_active(page_count_ > 1);
}

void UICharSelect::update_selected_character() {
    Sound(Sound::Name::CHAR_SELECT).play();

    char_looks_[selected_character_].set_stance(Stance::Id::WALK1);
    nametags_[selected_character_].set_selected(true);

    const StatsEntry &stats = characters_[selected_character_].stats;

    for (size_t i = 0; i < InfoLabel::NUM_LABELS; i++) {
        info_labels_[i].change_text(get_infolabel(i, stats));
    }

    scroll_open_.reset();
    scroll_done_ = false;
    selected_effect_.reset();
    selected_beam_.reset();
}

void UICharSelect::select_last_slot() {
    for (int i = PAGESIZE_ - 1; i >= 0; i--) {
        size_t index = i + selected_page * PAGESIZE_;

        if (index < static_cast<size_t>(characters_count_)) {
            button_pressed(i + Buttons::CHARACTER_SLOT0);

            return;
        }
    }
}

std::string UICharSelect::get_infolabel(size_t index,
                                        const StatsEntry &stats) const {
    switch (index) {
        case InfoLabel::JOB:
            return Job(stats.stats[MapleStat::Id::JOB]).get_name();
        case InfoLabel::LEVEL:
            return std::to_string(stats.stats[MapleStat::Id::LEVEL]);
        case InfoLabel::FAME:
            return std::to_string(static_cast<int16_t>(stats.stats[MapleStat::Id::FAME]));
        case InfoLabel::STR:
            return std::to_string(stats.stats[MapleStat::Id::STR]);
        case InfoLabel::INT:
            return std::to_string(stats.stats[MapleStat::Id::INT]);
        case InfoLabel::DEX:
            return std::to_string(stats.stats[MapleStat::Id::DEX]);
        case InfoLabel::LUK:
            return std::to_string(stats.stats[MapleStat::Id::LUK]);
        case InfoLabel::RANK:
            return stats.rank.first > 0 ? std::to_string(stats.rank.first) : "-";
        default: break;
    }

    return "";
}
void UICharSelect::request_pic() {
    std::function<void(const std::string &)> enterpic =
        [&](const std::string &entered_pic) {
            std::function<void(const std::string &)> verifypic =
                [&, entered_pic](const std::string &verify_pic) {
                    if (entered_pic == verify_pic) {
                        check_pic(entered_pic);
                    } else {
                        std::function<void()> onreturn = [&]() {
                            request_pic();
                        };

                        UI::get().emplace<UILoginNotice>(
                            UILoginNotice::Message::PASSWORD_IS_INCORRECT,
                            onreturn);
                    }
                };

            UI::get().emplace<UISoftKey>(
                verifypic,
                []() {},
                "Please re-enter your new PIC.",
                Point<int16_t>(24, 0));
        };

    UI::get().emplace<UISoftKey>(
        enterpic,
        []() {},
        "Your new PIC must at least be 6 characters long.",
        Point<int16_t>(24, 0));
}

void UICharSelect::check_pic(const std::string &entered_pic) const {
    const char *pStr = entered_pic.c_str();

    if (pStr == nullptr) {
        return;
    }

    int count = 0;
    char m = ' ';
    bool reptitive = false;

    while (*pStr) {
        if (*pStr == m) {
            count++;
        } else {
            count = 0;
            m = *pStr;
        }

        if (count > 2) {
            reptitive = true;
            break;
        }

        pStr++;
    }

    if (reptitive) {
        UI::get().emplace<UILoginNotice>(UILoginNotice::Message::PIC_REPITIVE);
    } else {
        fn_register_pic(characters_[selected_character_].id, entered_pic);
    }
}
}  // namespace ms