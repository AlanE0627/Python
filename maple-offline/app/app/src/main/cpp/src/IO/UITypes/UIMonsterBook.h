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

#include "../../Character/MonsterBook.h"
#include "../../Graphics/Animation.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"
#include "../UIDragElement.h"

namespace ms {
// maple-offline: v83 monster book (UIWindow.img/MonsterBook). Left page:
// the cards of one tab (card id % 10000 / 1000), 25 per page; right page:
// the selected card's monster, or the collection summary on the info tab.
class UIMonsterBook : public UIDragElement<PosMONSTERBOOK> {
public:
    static constexpr Type TYPE = UIElement::Type::MONSTER_BOOK;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = true;

    UIMonsterBook(const MonsterBook &book);

    void draw(float inter) const override;
    void update() override;

    Button::State button_pressed(uint16_t buttonid) override;

    Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    bool is_in_range(Point<int16_t> cursorpos) const override;

    UIElement::Type get_type() const override;

private:
    struct Card {
        int32_t cardid;
        int32_t mobid;
    };

    void load_cards();
    void select_tab(int8_t tab);
    void select_card(size_t index);
    void set_page(int16_t page);
    int16_t page_count() const;
    int8_t level_of(int32_t cardid) const;

    static constexpr uint8_t NUM_TABS = 9;
    static constexpr uint8_t PER_PAGE = 25;

    enum Buttons : uint16_t { BT_CLOSE, BT_PREV, BT_NEXT };

    const MonsterBook &book_;

    Texture backgrnd_;
    Texture card_slot_;
    Texture info_page_;
    Texture cover_;
    Texture select_;
    Texture full_mark_;
    std::array<Texture, NUM_TABS> tab_normal_;
    std::array<Texture, NUM_TABS> tab_selected_;
    Texture info_normal_;
    Texture info_selected_;

    std::array<std::vector<Card>, NUM_TABS> cards_;
    int8_t tab_;  // -1 = info tab
    int16_t page_;
    int32_t selected_;  // index in the tab, -1 = none

    Text page_label_;
    Animation mob_;
    Text mob_name_;
    Text mob_stats_;
    Text card_count_;
    std::array<Text, 5> info_values_;
};
}  // namespace ms
