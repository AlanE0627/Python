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

#include <array>
#include <vector>

#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"
#include "../../Character/Look/CharLook.h"
#include "../UIElement.h"

namespace ms {
// maple-offline: v83 cash shop (CashShop.img, 800x600 screen centred in the
// view). Commodities come from Etc Commodity.img; every price is 0 because
// the bundled server sells everything for free.
class UICashShop : public UIElement {
public:
    static constexpr Type TYPE = UIElement::Type::CASHSHOP;
    static constexpr bool FOCUSED = true;
    static constexpr bool TOGGLED = false;

    UICashShop();

    void draw(float inter) const override;
    void update() override;

    Button::State button_pressed(uint16_t buttonid) override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_scroll(double yoffset) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    bool is_in_range(Point<int16_t> cursorpos) const override;

    UIElement::Type get_type() const override;

    // cash locker contents (server 0x4B list, 0x57 bought item)
    struct LockerItem {
        int64_t cashid;
        int32_t itemid;
        int32_t sn;
        int16_t count;
    };

    void set_locker(std::vector<LockerItem> items);
    void add_locker(const LockerItem &item);
    // the item asked for with take() reached the item inventory
    void taken();
    void set_cash(int32_t credit, int32_t points, int32_t prepaid);
    void show_message(const std::string &text);

private:
    struct Commodity {
        int32_t sn;
        int32_t itemid;
        int16_t count;
        int32_t period;
        int32_t price;
    };

    void load_commodities();
    void select_tab(uint8_t tab);
    void set_page(int16_t page);
    int16_t page_count() const;
    void buy(size_t cell);
    void take(size_t slot);
    Point<int16_t> origin() const;

    static constexpr uint8_t NUM_TABS = 9;
    static constexpr uint8_t PER_PAGE = 10;
    static constexpr uint8_t LOCKER_COLS = 6;
    static constexpr uint8_t LOCKER_ROWS = 2;

    enum Buttons : uint16_t {
        BT_EXIT,
        BT_BUY0,
        BT_BUY_LAST = BT_BUY0 + PER_PAGE - 1
    };

    Texture backgrnd_;
    std::array<Texture, NUM_TABS> tabs_;
    Texture cell_;

    // commodities per tab (0 = MAIN ... 8 = WISH LIST)
    std::array<std::vector<Commodity>, NUM_TABS> lists_;
    uint8_t tab_;
    int16_t page_;

    std::array<Text, PER_PAGE> names_;
    std::array<Text, PER_PAGE> prices_;
    std::array<Text, PER_PAGE> details_;
    Text page_label_;
    Text prev_label_;
    Text next_label_;
    Text empty_label_;
    Text char_label_;
    std::array<Text, 3> cash_labels_;
    Text message_label_;
    int16_t message_time_;

    std::vector<LockerItem> locker_;
    int16_t locker_page_;
    Text locker_page_label_;
    // try-on preview: the player's look with tapped cash equips put on
    CharLook preview_;
    int64_t pending_take_;
    int64_t last_tap_time_;
    int16_t last_tap_slot_;
};
}  // namespace ms
