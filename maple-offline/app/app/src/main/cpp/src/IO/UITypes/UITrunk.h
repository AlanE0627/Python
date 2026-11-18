// maple-offline: v83 storage window (UIWindow.img/Trunk, 463x318).
// Left: the items kept in storage for the selected tab; right: the
// player's items of the same tab. Click a row to select it, then use
// Get / Put; the coin buttons move mesos.
#pragma once

#include <vector>

#include "../../Character/Inventory/Inventory.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"
#include "../UIDragElement.h"

namespace ms {
class UITrunk : public UIDragElement<PosSHOP> {
public:
    static constexpr Type TYPE = UIElement::Type::TRUNK;
    static constexpr bool FOCUSED = true;
    static constexpr bool TOGGLED = false;

    struct Entry {
        int32_t itemid;
        int16_t count;
        InventoryType::Id type;
    };

    UITrunk(int32_t npcid, uint8_t slots, int32_t meso, std::vector<Entry> items);

    void draw(float alpha) const override;
    void update() override;
    void send_key(int32_t keycode, bool pressed, bool escape) override;
    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
    void send_scroll(double yoffset) override;
    UIElement::Type get_type() const override;

    // server updates
    // type NONE = the whole storage was resent (arrange)
    void set_items(uint8_t slots, InventoryType::Id type, std::vector<Entry> items);
    void set_meso(int32_t meso);

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    enum Buttons : uint16_t {
        GET, GET_ALL, OUT_COIN, PUT, IN_COIN, EXIT,
        TAB0, TAB1, TAB2, TAB3, TAB4
    };

    struct Row {
        Texture icon;
        Text name;
        Text count;
        int16_t slot;   // inventory slot (right side) / index in tab (left)
        int32_t itemid;
        int16_t quantity;
    };

    void rebuild();
    void close();
    Row make_row(int32_t itemid, int16_t count, int16_t slot) const;

    static constexpr int16_t ROWS_ = 5;
    static constexpr int16_t RIGHT_ROWS_ = 4;
    static constexpr int16_t LEFT_X_ = 6;
    static constexpr int16_t RIGHT_X_ = 236;
    static constexpr int16_t LEFT_Y_ = 87;
    static constexpr int16_t RIGHT_Y_ = 127;
    static constexpr int16_t ROW_H_ = 40;
    static constexpr int16_t ROW_W_ = 200;

    int32_t npcid_;
    uint8_t slots_;
    int32_t meso_;
    std::vector<Entry> stored_;
    InventoryType::Id tab_;

    std::vector<Row> left_;
    std::vector<Row> right_;
    int16_t left_offset_ = 0;
    int16_t right_offset_ = 0;
    int16_t left_sel_ = -1;
    int16_t right_sel_ = -1;

    Texture npc_;
    Text store_meso_;
    Text own_meso_;
    Text slots_label_;
    Point<int16_t> last_cursor_;
};
}  // namespace ms
