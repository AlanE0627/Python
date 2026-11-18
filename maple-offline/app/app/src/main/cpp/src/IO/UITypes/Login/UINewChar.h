// maple-offline: v83 character creation (Login.img/NewChar, NewCharKnight,
// NewCharAran). One implementation for the three races; each race keeps its
// own UI class name so existing callers keep working.
#pragma once

#include <array>
#include <vector>

#include "../../Character/Look/CharLook.h"
#include "../../LoginScene.h"
#include "../../Util/Randomizer.h"
#include "../Components/Textfield.h"
#include "../UIElement.h"

namespace ms {
class UINewChar : public UIElement {
public:
    static constexpr Type TYPE = UIElement::Type::CLASS_CREATION;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = false;

    enum Race : uint8_t { EXPLORER, CYGNUS, ARAN };

    explicit UINewChar(Race race);

    void draw(float inter) const override;

    void update() override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

    void send_naming_result(bool nameused);

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    enum Row : uint8_t {
        FACE,
        HAIR,
        HAIRCOLOR,
        SKIN,
        TOP,
        BOTTOM,
        SHOES,
        WEAPON,
        GENDER,
        NUM_ROWS
    };

    enum Buttons : uint16_t { BT_BACK, BT_OK, BT_CANCEL, BT_ROW0 };

    size_t row_count(Row row) const;
    void cycle(Row row, int dir);
    void apply_look();
    void randomize_look();
    void refresh_names();
    void set_naming(bool naming);
    void return_to_charselect();
    std::string row_value(Row row) const;

    Race race_;
    uint8_t job_;
    Text version_;
    Texture charset_;
    Texture charname_;
    std::array<Texture, NUM_ROWS> row_tex_;
    std::array<Text, NUM_ROWS> row_text_;
    Point<int16_t> board_pos_;
    Point<int16_t> rows_off_;
    Point<int16_t> ok_off_;
    Point<int16_t> cancel_off_;
    Point<int16_t> nameboard_pos_;
    Point<int16_t> name_ok_off_;
    Point<int16_t> name_cancel_off_;
    Point<int16_t> preview_pos_;
    Textfield namechar_;
    CharLook newchar_;
    Randomizer randomizer_;
    std::array<std::vector<int32_t>, 2> faces_, hairs_, tops_, bots_, shoes_,
        weapons_;
    std::array<std::vector<int32_t>, 2> haircolors_, skins_;
    std::array<size_t, NUM_ROWS> sel_;
    bool naming_;
    bool focus_pending_;
    bool named_;
    int16_t select_row_;
};
}  // namespace ms
