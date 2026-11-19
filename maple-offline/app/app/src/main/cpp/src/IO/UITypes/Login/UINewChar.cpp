// maple-offline: v83 character creation. See UINewChar.h.
#include "UINewChar.h"

#include <nlnx/nx.hpp>
#include <cstdio>

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Data/ItemData.h"
#include "../../Net/Packets/CharCreationPackets.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "UICharSelect.h"
#include "UILoginNotice.h"
#include "UIRaceSelect.h"

namespace ms {
namespace {
const char *const HAIR_COLORS[8] = { "Black", "Red",    "Orange", "Blonde",
                                     "Green", "Blue",   "Purple", "Brown" };
const char *const SKINS[4] = { "Light", "Tan", "Dark", "Pale" };

// Board placement inside the 800x600 frame (right side of each scene).
const Point<int16_t> BOARD_POS = Point<int16_t>(515, 55);
const Point<int16_t> NAMEBOARD_POS = Point<int16_t>(527, 110);
}  // namespace

UINewChar::UINewChar(Race race) :
    UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600)),
    race_(race),
    naming_(false),
    focus_pending_(false),
    named_(false),
    select_row_(0) {
    nl::node Login = nl::nx::ui_img("Login.img");
    nl::node Common = Login["Common"];
    nl::node mkinfo = nl::nx::etc["MakeCharInfo.img"];

    nl::node board;
    nl::node male, female;
    LoginScene::Step step;
    Rectangle<int16_t> name_rect;
    Color::Name name_color = Color::Name::WHITE;

    switch (race_) {
        case CYGNUS:
            board = Login["NewCharKnight"];
            male = mkinfo["PremiumCharMale"];
            female = mkinfo["PremiumCharFemale"];
            step = LoginScene::NEW_KNIGHT;
            job_ = 0;
            rows_off_ = Point<int16_t>(13, 102);
            ok_off_ = Point<int16_t>(30, 320);
            cancel_off_ = Point<int16_t>(115, 320);
            name_rect = Rectangle<int16_t>(Point<int16_t>(25, 106), Point<int16_t>(166, 124));
            name_ok_off_ = Point<int16_t>(15, 177);
            name_cancel_off_ = Point<int16_t>(101, 177);
            preview_pos_ = Point<int16_t>(390, 352);
            break;
        case ARAN:
            board = Login["NewCharAran"];
            male = mkinfo["OrientCharMale"];
            female = mkinfo["OrientCharFemale"];
            step = LoginScene::NEW_ARAN;
            job_ = 2;
            rows_off_ = Point<int16_t>(35, 88);
            ok_off_ = Point<int16_t>(50, 278);
            cancel_off_ = Point<int16_t>(140, 278);
            name_rect = Rectangle<int16_t>(Point<int16_t>(36, 93), Point<int16_t>(176, 114));
            name_ok_off_ = Point<int16_t>(22, 148);
            name_cancel_off_ = Point<int16_t>(108, 148);
            name_color = Color::Name::BLACK;
            preview_pos_ = Point<int16_t>(390, 352);
            break;
        case EXPLORER:
        default:
            board = Login["NewChar"];
            male = mkinfo["Info"]["CharMale"];
            female = mkinfo["Info"]["CharFemale"];
            step = LoginScene::NEW_EXPLORER;
            job_ = 1;
            rows_off_ = Point<int16_t>(12, 102);
            ok_off_ = Point<int16_t>(29, 320);
            cancel_off_ = Point<int16_t>(114, 320);
            name_rect = Rectangle<int16_t>(Point<int16_t>(24, 106), Point<int16_t>(165, 124));
            name_ok_off_ = Point<int16_t>(14, 177);
            name_cancel_off_ = Point<int16_t>(100, 177);
            preview_pos_ = Point<int16_t>(400, 332);
            break;
    }

    LoginScene::get().set_step(step);
    position_ = LoginScene::get().frame_origin();

    version_ = Text(Text::Font::A11M,
                    Text::Alignment::LEFT,
                    Color::Name::LEMONGRASS,
                    "Ver. " + Configuration::get().get_version());

    sprites_.emplace_back(Common["frame"], Point<int16_t>(400, 300));
    sprites_.emplace_back(Common["step"]["3"], Point<int16_t>(40, 0));

    board_pos_ = BOARD_POS;
    nameboard_pos_ = NAMEBOARD_POS;
    charset_ = board["charSet"];
    charname_ = board["charName"];

    for (uint8_t i = 0; i < NUM_ROWS; i++) {
        row_tex_[i] = board["avatarSel"][std::to_string(i)]["normal"];
        row_text_[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);

        Point<int16_t> row = board_pos_ + rows_off_ + Point<int16_t>(0, i * 18);

        buttons_[BT_ROW0 + i * 2] =
            std::make_unique<MapleButton>(board["BtLeft"], row + Point<int16_t>(61, 1));
        buttons_[BT_ROW0 + i * 2 + 1] =
            std::make_unique<MapleButton>(board["BtRight"], row + Point<int16_t>(183, 1));
    }

    buttons_[BT_OK] = std::make_unique<MapleButton>(board["BtYes"], board_pos_ + ok_off_);
    buttons_[BT_CANCEL] = std::make_unique<MapleButton>(board["BtNo"], board_pos_ + cancel_off_);
    buttons_[BT_BACK] = std::make_unique<MapleButton>(Common["BtStart"], Point<int16_t>(0, 515));

    namechar_ = Textfield(Text::Font::A13M,
                          Text::Alignment::LEFT,
                          name_color,
                          Rectangle<int16_t>(nameboard_pos_ + name_rect.get_left_top(),
                                             nameboard_pos_ + name_rect.get_right_bottom()),
                          12);
    namechar_.set_state(Textfield::State::DISABLED);
    namechar_.set_enter_callback([&](const std::string &) { button_pressed(BT_OK); });
    namechar_.set_key_callback(KeyAction::Id::ESCAPE, [&]() { button_pressed(BT_CANCEL); });

    for (size_t g = 0; g < 2; g++) {
        nl::node src = g == 0 ? male : female;

        for (const auto &cat : src) {
            int num = std::stoi(cat.name());

            for (const auto &idnode : cat) {
                int32_t value = idnode;

                switch (num) {
                    case 0: faces_[g].push_back(value); break;
                    case 1: hairs_[g].push_back(value); break;
                    case 2: haircolors_[g].push_back(value); break;
                    case 3: skins_[g].push_back(value); break;
                    case 4: tops_[g].push_back(value); break;
                    case 5: bots_[g].push_back(value); break;
                    case 6: shoes_[g].push_back(value); break;
                    case 7: weapons_[g].push_back(value); break;
                    default: break;
                }
            }
        }
    }

    sel_.fill(0);
    randomize_look();

    newchar_.set_direction(false);

    dimension_ = Point<int16_t>(800, 600);
}

size_t UINewChar::row_count(Row row) const {
    size_t g = sel_[GENDER];

    switch (row) {
        case FACE: return faces_[g].size();
        case HAIR: return hairs_[g].size();
        case HAIRCOLOR: return haircolors_[g].size();
        case SKIN: return skins_[g].size();
        case TOP: return tops_[g].size();
        case BOTTOM: return bots_[g].size();
        case SHOES: return shoes_[g].size();
        case WEAPON: return weapons_[g].size();
        case GENDER: return 2;
        default: return 0;
    }
}

void UINewChar::cycle(Row row, int dir) {
    size_t n = row_count(row);

    if (n <= 1) {
        return;
    }

    sel_[row] = (sel_[row] + n + dir) % n;

    if (row == GENDER) {
        randomize_look();
    } else {
        apply_look();
    }
}

void UINewChar::apply_look() {
    size_t g = sel_[GENDER];

    newchar_.set_body(skins_[g][sel_[SKIN]]);
    newchar_.set_face(faces_[g][sel_[FACE]]);
    newchar_.set_hair(hairs_[g][sel_[HAIR]] + haircolors_[g][sel_[HAIRCOLOR]]);
    newchar_.add_equip(tops_[g][sel_[TOP]]);
    newchar_.add_equip(bots_[g][sel_[BOTTOM]]);
    newchar_.add_equip(shoes_[g][sel_[SHOES]]);
    newchar_.add_equip(weapons_[g][sel_[WEAPON]]);

    refresh_names();
}

void UINewChar::randomize_look() {
    for (uint8_t r = 0; r < GENDER; r++) {
        size_t n = row_count(static_cast<Row>(r));
        sel_[r] = n > 0 ? randomizer_.next_int(n) : 0;
    }

    apply_look();
}

std::string UINewChar::row_value(Row row) const {
    size_t g = sel_[GENDER];

    switch (row) {
        case FACE: return newchar_.get_face() ? newchar_.get_face()->get_name() : "";
        case HAIR: return newchar_.get_hair() ? newchar_.get_hair()->get_name() : "";
        case HAIRCOLOR: {
            int32_t c = haircolors_[g][sel_[HAIRCOLOR]];
            return c >= 0 && c < 8 ? HAIR_COLORS[c] : std::to_string(c);
        }
        case SKIN: {
            int32_t s = skins_[g][sel_[SKIN]];
            return s >= 0 && s < 4 ? SKINS[s] : std::to_string(s);
        }
        case TOP: return ItemData::get(tops_[g][sel_[TOP]]).get_name();
        case BOTTOM: return ItemData::get(bots_[g][sel_[BOTTOM]]).get_name();
        case SHOES: return ItemData::get(shoes_[g][sel_[SHOES]]).get_name();
        case WEAPON: return ItemData::get(weapons_[g][sel_[WEAPON]]).get_name();
        case GENDER: return g == 0 ? "Male" : "Female";
        default: return "";
    }
}

void UINewChar::refresh_names() {
    for (uint8_t r = 0; r < NUM_ROWS; r++) {
        // Fit the value cell between the arrows (about 104 px).
        std::string value = row_value(static_cast<Row>(r));
        row_text_[r].change_text(value);

        while (row_text_[r].width() > 104 && value.size() > 3) {
            value.pop_back();
            row_text_[r].change_text(value + "..");
        }

        bool cyclable = row_count(static_cast<Row>(r)) > 1;
        buttons_[BT_ROW0 + r * 2]->set_active(!naming_ && cyclable);
        buttons_[BT_ROW0 + r * 2 + 1]->set_active(!naming_ && cyclable);
    }
}

void UINewChar::set_naming(bool naming) {
    naming_ = naming;

    if (naming_) {
        buttons_[BT_OK]->set_position(nameboard_pos_ + name_ok_off_);
        buttons_[BT_CANCEL]->set_position(nameboard_pos_ + name_cancel_off_);
        focus_pending_ = true;
    } else {
        buttons_[BT_OK]->set_position(board_pos_ + ok_off_);
        buttons_[BT_CANCEL]->set_position(board_pos_ + cancel_off_);
        namechar_.set_state(Textfield::State::DISABLED);
    }

    buttons_[BT_OK]->set_state(Button::State::NORMAL);
    buttons_[BT_CANCEL]->set_state(Button::State::NORMAL);

    refresh_names();
}

void UINewChar::draw(float inter) const {
    UIElement::draw_sprites(inter);

    newchar_.draw(position_ + preview_pos_, inter);

    if (!naming_) {
        charset_.draw(position_ + board_pos_);

        for (uint8_t r = 0; r < NUM_ROWS; r++) {
            Point<int16_t> row = position_ + board_pos_ + rows_off_ + Point<int16_t>(0, r * 18);

            row_tex_[r].draw(row);
            row_text_[r].draw(row + Point<int16_t>(129, 1));
        }

        // Controller cursor: highlight the active row.
        Point<int16_t> hl = position_ + board_pos_ + rows_off_ + Point<int16_t>(0, select_row_ * 18);
        ColorBox(200, 17, Color::Name::WHITE, 0.25f).draw(DrawArgument(hl));
    } else {
        charname_.draw(position_ + nameboard_pos_);
        namechar_.draw(position_);
    }

    UIElement::draw_buttons(inter);

    version_.draw(position_ + Point<int16_t>(707, 1));
}

void UINewChar::update() {
    UIElement::update();

    newchar_.update(Constants::TIMESTEP);

    if (naming_) {
        // Focus after the click that opened the board has been processed;
        // focusing inside that click would be undone by the same press.
        if (focus_pending_ && !named_) {
            focus_pending_ = false;
            namechar_.set_state(Textfield::State::FOCUSED);
        }

        namechar_.update(position_);
    }
}

Cursor::State UINewChar::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    if (naming_ && namechar_.get_state() == Textfield::State::NORMAL
        && namechar_.get_bounds().contains(cursorpos)) {
        if (clicked) {
            namechar_.set_state(Textfield::State::FOCUSED);
            return Cursor::State::CLICKING;
        }

        return Cursor::State::IDLE;
    }

    return UIElement::send_cursor(clicked, cursorpos);
}

void UINewChar::send_key(int32_t keycode, bool pressed, bool escape) {
    if (!pressed) {
        return;
    }

    if (escape) {
        button_pressed(BT_CANCEL);
        return;
    }

    if (keycode == KeyAction::Id::RETURN) {
        button_pressed(BT_OK);
        return;
    }

    if (naming_) {
        return;
    }

    // Controller / keyboard: up-down picks a row, left-right changes it.
    switch (keycode) {
        case KeyAction::Id::UP: select_row_ = (select_row_ + NUM_ROWS - 1) % NUM_ROWS; break;
        case KeyAction::Id::DOWN: select_row_ = (select_row_ + 1) % NUM_ROWS; break;
        case KeyAction::Id::LEFT: cycle(static_cast<Row>(select_row_), -1); break;
        case KeyAction::Id::RIGHT: cycle(static_cast<Row>(select_row_), 1); break;
        default: break;
    }
}

UIElement::Type UINewChar::get_type() const {
    return TYPE;
}

void UINewChar::send_naming_result(bool nameused) {
    if (named_) {
        return;
    }

#ifdef MS_DESKTOP_DEBUG
    std::fprintf(stderr, "[dbg] naming result used=%d\n", nameused);
#endif
    if (!nameused) {
        named_ = true;

        size_t g = sel_[GENDER];

        CreateCharPacket(namechar_.get_text(),
                         job_,
                         faces_[g][sel_[FACE]],
                         hairs_[g][sel_[HAIR]],
                         static_cast<uint8_t>(haircolors_[g][sel_[HAIRCOLOR]]),
                         static_cast<uint8_t>(skins_[g][sel_[SKIN]]),
                         tops_[g][sel_[TOP]],
                         bots_[g][sel_[BOTTOM]],
                         shoes_[g][sel_[SHOES]],
                         weapons_[g][sel_[WEAPON]],
                         g == 1)
            .dispatch();
        // The new entry arrives as ADD_NEWCHAR_ENTRY; its handler adds the
        // character and returns to the character select screen.
    } else {
        std::function<void()> onok = [&]() {
            focus_pending_ = true;
            buttons_[BT_OK]->set_state(Button::State::NORMAL);
            buttons_[BT_CANCEL]->set_state(Button::State::NORMAL);
        };

        UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NAME_IN_USE, onok);
    }
}

void UINewChar::return_to_charselect() {
    Sound(Sound::Name::SCROLL_UP).play();

    UI::get().remove(UIElement::Type::CLASS_CREATION);
    UI::get().remove(UIElement::Type::RACE_SELECT);

    if (auto charselect = UI::get().get_element<UICharSelect>()) {
        charselect->get().makeactive();
        LoginScene::get().set_step(LoginScene::CHAR_SELECT);
    }
}

Button::State UINewChar::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case BT_BACK:
            Sound(Sound::Name::SCROLL_UP).play();
            UI::get().remove(UIElement::Type::CLASS_CREATION);
            UI::get().emplace<UIRaceSelect>();
            return Button::State::NORMAL;
        case BT_OK: {
            if (!naming_) {
                set_naming(true);
                return Button::State::NORMAL;
            }

            if (named_) {
                return Button::State::NORMAL;
            }

            std::string name = namechar_.get_text();

            if (name.empty()) {
                return Button::State::NORMAL;
            }

            bool legal = name.size() >= 4 && name.size() <= 12;

            if (legal) {
                if (auto raceselect = UI::get().get_element<UIRaceSelect>()) {
                    legal = raceselect->get().check_name(name);
                }
            }

            namechar_.set_state(Textfield::State::DISABLED);
            buttons_[BT_OK]->set_state(Button::State::DISABLED);
            buttons_[BT_CANCEL]->set_state(Button::State::DISABLED);

#ifdef MS_DESKTOP_DEBUG
            std::fprintf(stderr, "[dbg] name ok '%s' legal %d\n", name.c_str(), legal);
#endif
            if (legal) {
                NameCharPacket(name).dispatch();
                return Button::State::DISABLED;
            }

            std::function<void()> okhandler = [&]() {
                focus_pending_ = true;
                buttons_[BT_OK]->set_state(Button::State::NORMAL);
                buttons_[BT_CANCEL]->set_state(Button::State::NORMAL);
            };

            UI::get().emplace<UILoginNotice>(UILoginNotice::Message::ILLEGAL_NAME, okhandler);

            return Button::State::DISABLED;
        }
        case BT_CANCEL:
            if (naming_) {
                set_naming(false);
            } else {
                button_pressed(BT_BACK);
            }

            return Button::State::NORMAL;
        default:
            if (buttonid >= BT_ROW0) {
                uint16_t idx = buttonid - BT_ROW0;
                select_row_ = idx / 2;
                cycle(static_cast<Row>(idx / 2), (idx % 2) ? 1 : -1);
            }

            return Button::State::NORMAL;
    }
}
}  // namespace ms
