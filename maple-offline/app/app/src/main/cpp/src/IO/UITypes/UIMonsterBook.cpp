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
#include "UIMonsterBook.h"

#include <algorithm>

#include "../../Data/ItemData.h"
#include "../../Util/StringHandling.h"
#include "../Components/MapleButton.h"

#include <nlnx/nx.hpp>

namespace ms {
namespace {
// book (475x349): left page x 25..236, right page 240..446
constexpr Point<int16_t> SLOT_SHEET(43, 30);
constexpr int16_t SLOT_X0 = 6;
constexpr int16_t SLOT_Y0 = 29;
constexpr int16_t SLOT_DX = 33;
constexpr int16_t SLOT_DY = 45;
constexpr int16_t SLOT_W = 31;
constexpr int16_t SLOT_H = 42;
constexpr Point<int16_t> PAGE_LABEL(130, 292);
constexpr int16_t RIGHT_CX = 343;
constexpr int16_t TAB_X = 4;
constexpr int16_t TAB_Y0 = 64;
constexpr int16_t TAB_DY = 24;
constexpr Point<int16_t> INFO_TAB(4, 24);

Point<int16_t> slot_pos(size_t s) {
    return SLOT_SHEET + Point<int16_t>(SLOT_X0 + SLOT_DX * static_cast<int16_t>(s % 5),
                                       SLOT_Y0 + SLOT_DY * static_cast<int16_t>(s / 5));
}

std::string with_commas(int64_t value) {
    std::string s = std::to_string(value);

    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
        s.insert(static_cast<size_t>(i), ",");
    }

    return s;
}

}  // namespace

UIMonsterBook::UIMonsterBook(const MonsterBook &book) :
    UIDragElement<PosMONSTERBOOK>(Point<int16_t>(475, 24)),
    book_(book),
    tab_(-1),
    page_(0),
    selected_(-1) {
    nl::node src = nl::nx::ui_img("UIWindow.img")["MonsterBook"];

    backgrnd_ = src["backgrnd"];
    card_slot_ = src["cardSlot"];
    info_page_ = src["infoPage"];
    cover_ = src["cover"];
    select_ = src["select"];
    full_mark_ = src["fullMark"];

    for (size_t i = 0; i < NUM_TABS; i++) {
        tab_normal_[i] = src["LeftTab"][std::to_string(i)]["normal"]["0"];
        tab_selected_[i] = src["LeftTab"][std::to_string(i)]["selected"]["0"];
    }

    info_normal_ = src["LeftTabInfo"]["0"]["normal"]["0"];
    info_selected_ = src["LeftTabInfo"]["0"]["selected"]["0"];

    buttons_[Buttons::BT_CLOSE] =
        std::make_unique<MapleButton>(src["BtClose"], Point<int16_t>(446, 8));
    buttons_[Buttons::BT_PREV] =
        std::make_unique<MapleButton>(src["arrowLeft"], PAGE_LABEL + Point<int16_t>(-70, -2));
    buttons_[Buttons::BT_NEXT] =
        std::make_unique<MapleButton>(src["arrowRight"], PAGE_LABEL + Point<int16_t>(46, -2));

    page_label_ = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::BROWN);
    mob_name_ = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::BLACK, "", 190);
    mob_stats_ = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
    card_count_ = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::BROWN);

    for (auto &label : info_values_) {
        label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::BLACK);
    }

    load_cards();
    select_tab(-1);

    dimension_ = Point<int16_t>(475, 349);
}

void UIMonsterBook::load_cards() {
    for (const nl::node &card : nl::nx::item["Consume"]["0238.img"]) {
        int32_t cardid = std::stoi(card.name());
        int32_t mobid = card["info"]["mob"];
        int32_t tab = (cardid % 10000) / 1000;

        if (tab < 0 || tab >= NUM_TABS || mobid == 0) {
            continue;
        }

        cards_[tab].push_back({ cardid, mobid });
    }

    for (auto &list : cards_) {
        std::sort(list.begin(), list.end(),
                  [](const Card &a, const Card &b) { return a.cardid < b.cardid; });
    }

    int32_t total = 0;
    int32_t special = 0;

    for (const auto &[id, level] : book_.get_cards()) {
        total++;

        if (id / 1000 == 8) {
            special++;
        }
    }

    // book level as the server computes it (client/MonsterBook.calculateLevel):
    // unique cards against 1, 11, 31, 61, ... (+10 x level each step)
    int32_t level = 0;
    int32_t next = 1;

    do {
        level++;
        next += level * 10;
    } while (total >= next);

    info_values_[0].change_text(std::to_string(level));
    info_values_[1].change_text(std::to_string(total));
    info_values_[2].change_text(std::to_string(special));
    info_values_[3].change_text(std::to_string(total - special));

    int32_t cover = book_.get_cover();
    info_values_[4].change_text(cover > 0 ? ItemData::get(cover).get_name() : "-");
}

int8_t UIMonsterBook::level_of(int32_t cardid) const {
    const auto &cards = book_.get_cards();
    auto it = cards.find(static_cast<int16_t>(cardid % 10000));

    return it != cards.end() ? it->second : 0;
}

int16_t UIMonsterBook::page_count() const {
    if (tab_ < 0) {
        return 1;
    }

    auto n = static_cast<int16_t>(cards_[tab_].size());

    return std::max<int16_t>(1, (n + PER_PAGE - 1) / PER_PAGE);
}

void UIMonsterBook::select_tab(int8_t tab) {
    tab_ = tab;
    selected_ = -1;
    set_page(0);
}

void UIMonsterBook::set_page(int16_t page) {
    page_ = std::max<int16_t>(0, std::min<int16_t>(page, page_count() - 1));
    page_label_.change_text(std::to_string(page_ + 1) + " / " + std::to_string(page_count()));

    bool paged = tab_ >= 0 && page_count() > 1;

    buttons_[Buttons::BT_PREV]->set_active(paged);
    buttons_[Buttons::BT_NEXT]->set_active(paged);
}

void UIMonsterBook::select_card(size_t index) {
    if (tab_ < 0 || index >= cards_[tab_].size()) {
        return;
    }

    selected_ = static_cast<int32_t>(index);

    const Card &card = cards_[tab_][index];
    std::string strid = string_format::extend_id(card.mobid, 7);
    nl::node src = nl::nx::mob[strid + ".img"];
    nl::node info = src["info"];

    std::string link = info["link"];
    nl::node look = link.empty() ? src : nl::nx::mob[string_format::extend_id(std::stoi(link), 7) + ".img"];

    mob_ = look["stand"] ? Animation(look["stand"]) : Animation(look["fly"]);

    int8_t level = level_of(card.cardid);
    std::string name = std::string(nl::nx::string["Mob.img"][std::to_string(card.mobid)]["name"]);

    mob_name_.change_text(level > 0 ? name : "???");

    if (level > 0) {
        int32_t mob_level = info["level"];
        int64_t hp = info["maxHP"];
        int64_t mp = info["maxMP"];
        int64_t exp = info["exp"];

        mob_stats_.change_text("레벨 : " + std::to_string(mob_level) + "\\n"
                               + "HP : " + with_commas(hp) + "\\n"
                               + "MP : " + with_commas(mp) + "\\n"
                               + "경험치 : " + with_commas(exp));
    } else {
        mob_stats_.change_text("카드를 모으면 정보가 나옵니다.");
    }

    card_count_.change_text("카드 " + std::to_string(level) + " / 5");
}

void UIMonsterBook::draw(float inter) const {
    Point<int16_t> o = position_;

    // tabs stick out on the left of the book
    (tab_ < 0 ? info_selected_ : info_normal_)
        .draw(DrawArgument(o + INFO_TAB - Point<int16_t>((tab_ < 0 ? info_selected_ : info_normal_).width(), 0)));

    for (int8_t t = 0; t < NUM_TABS; t++) {
        const Texture &tex = t == tab_ ? tab_selected_[t] : tab_normal_[t];

        tex.draw(DrawArgument(o + Point<int16_t>(TAB_X - tex.width(), TAB_Y0 + TAB_DY * t)));
    }

    backgrnd_.draw(DrawArgument(o));

    if (tab_ < 0) {
        // collection summary on the right page
        info_page_.draw(DrawArgument(o + Point<int16_t>(RIGHT_CX - 52, 60)));

        // label rows of infoPage: Book Master Lvl, Total, Special, Basic, Cover
        const int16_t rows[] = { 8, 58, 92, 110, 148 };
        Point<int16_t> page = o + Point<int16_t>(RIGHT_CX - 52, 60);

        for (size_t i = 0; i < info_values_.size(); i++) {
            info_values_[i].draw(DrawArgument(page + Point<int16_t>(i == 0 ? 100 : 90, rows[i] - 11)));
        }

        UIElement::draw_buttons(inter);

        return;
    }

    card_slot_.draw(DrawArgument(o + SLOT_SHEET));

    const auto &list = cards_[tab_];

    for (size_t s = 0; s < PER_PAGE; s++) {
        size_t index = static_cast<size_t>(page_) * PER_PAGE + s;

        if (index >= list.size()) {
            break;
        }

        Point<int16_t> p = o + slot_pos(s);
        int8_t level = level_of(list[index].cardid);

        if (level > 0) {
            ItemData::get(list[index].cardid)
                .get_icon(false)
                .draw(DrawArgument(p + Point<int16_t>(0, SLOT_H - 5)));

            if (level >= 5) {
                full_mark_.draw(DrawArgument(p + Point<int16_t>(SLOT_W - 9, 2)));
            }
        } else {
            cover_.draw(DrawArgument(p + Point<int16_t>(2, 40)));
        }

        if (static_cast<int32_t>(index) == selected_) {
            select_.draw(DrawArgument(p + Point<int16_t>(2, 40)));
        }
    }

    page_label_.draw(DrawArgument(o + PAGE_LABEL));

    if (selected_ >= 0) {
        // the monster shows once its card has been found
        if (level_of(cards_[tab_][selected_].cardid) > 0) {
            mob_.draw(DrawArgument(o + Point<int16_t>(RIGHT_CX, 150)), inter);
        }

        mob_name_.draw(DrawArgument(o + Point<int16_t>(RIGHT_CX, 34)));
        card_count_.draw(DrawArgument(o + Point<int16_t>(RIGHT_CX, 160)));
        mob_stats_.draw(DrawArgument(o + Point<int16_t>(RIGHT_CX - 70, 186)));
    }

    UIElement::draw_buttons(inter);
}

void UIMonsterBook::update() {
    UIElement::update();

    if (selected_ >= 0) {
        mob_.update();
    }
}

Button::State UIMonsterBook::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::BT_CLOSE: deactivate(); break;
        case Buttons::BT_PREV: set_page(page_ - 1); break;
        case Buttons::BT_NEXT: set_page(page_ + 1); break;
        default: break;
    }

    return Button::State::NORMAL;
}

Cursor::State UIMonsterBook::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    Cursor::State state = UIDragElement::send_cursor(clicked, cursorpos);

    if (!clicked || dragged_) {
        return state;
    }

    Point<int16_t> p = cursorpos - position_;

    // info tab and the nine card tabs, left of the book
    if (p.x() < TAB_X + 4 && p.x() > TAB_X - 46) {
        if (p.y() >= INFO_TAB.y() && p.y() < INFO_TAB.y() + 35) {
            select_tab(-1);

            return Cursor::State::CLICKING;
        }

        for (int8_t t = 0; t < NUM_TABS; t++) {
            int16_t y0 = TAB_Y0 + TAB_DY * t;

            if (p.y() >= y0 && p.y() < y0 + 20) {
                select_tab(t);

                return Cursor::State::CLICKING;
            }
        }
    }

    if (tab_ >= 0) {
        for (size_t s = 0; s < PER_PAGE; s++) {
            Point<int16_t> sp = slot_pos(s);

            if (p.x() >= sp.x() && p.x() < sp.x() + SLOT_W && p.y() >= sp.y()
                && p.y() < sp.y() + SLOT_H) {
                select_card(static_cast<size_t>(page_) * PER_PAGE + s);

                return Cursor::State::CLICKING;
            }
        }
    }

    return state;
}

void UIMonsterBook::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed && escape) {
        deactivate();
    }
}

bool UIMonsterBook::is_in_range(Point<int16_t> cursorpos) const {
    // the tabs stick out up to 46 px left of the book
    return Rectangle<int16_t>(position_ - Point<int16_t>(46, 0), position_ + dimension_)
        .contains(cursorpos);
}

UIElement::Type UIMonsterBook::get_type() const {
    return TYPE;
}
}  // namespace ms
