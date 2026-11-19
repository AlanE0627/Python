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
#include "UICashShop.h"

#include <algorithm>

#include "../../Character/Inventory/InventoryType.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "../Window.h"
#include "../../Timer.h"

#include <nlnx/nx.hpp>

namespace ms {
namespace {
// v83 CashShop.img layout, measured on Base/backgrnd (800x600)
constexpr Point<int16_t> TAB_POS(272, 14);
const int16_t TAB_X[] = { 3, 68, 120, 172, 224, 276, 328, 380, 450 };
const int16_t TAB_W[] = { 63, 50, 50, 50, 50, 50, 50, 50, 55 };
constexpr int16_t TAB_TOP = 10;
constexpr int16_t TAB_BOTTOM = 55;

constexpr int16_t CELL_X0 = 277;
constexpr int16_t CELL_DX = 206;
constexpr int16_t CELL_Y0 = 98;
constexpr int16_t CELL_DY = 81;

constexpr int16_t LOCKER_X0 = 21;
constexpr int16_t LOCKER_Y0 = 349;
constexpr int16_t LOCKER_D = 35;
constexpr int16_t LOCKER_CELL = 31;

constexpr Point<int16_t> PAGE_POS(480, 503);
constexpr Point<int16_t> LOCKER_PAGE_POS(205, 322);

Point<int16_t> cell_pos(size_t i) {
    return Point<int16_t>(CELL_X0 + CELL_DX * static_cast<int16_t>(i % 2),
                          CELL_Y0 + CELL_DY * static_cast<int16_t>(i / 2));
}

// commodity SN group -> tab. KMS 1.2.95 tab strip: 메인, 장비, 소비, 스페셜,
// 기타, 펫, 패키지, 도움말, 찜목록 (the v83 strip has EVENT and SET-UP
// instead; both put equipment in the 2xx groups, use in 3xx and so on)
int8_t tab_of(int32_t sn) {
    switch (sn / 10000000) {
        case 1: return 0;
        case 2: return 1;
        case 3: return 2;
        case 4: return 3;
        case 5: return 4;
        case 6: return 5;
        case 7:
        case 8: return 6;
        default: return -1;
    }
}

std::string with_commas(int32_t value) {
    std::string s = std::to_string(value);

    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
        s.insert(static_cast<size_t>(i), ",");
    }

    return s;
}

void exit_cashshop() {
    float fadestep = 0.025f;

    Window::get().fadeout(fadestep, []() {
        GraphicsGL::get().clear();
        ChangeMapPacket().dispatch();
    });

    GraphicsGL::get().lock();
    Stage::get().clear();
    Timer::get().start();
}
}  // namespace

UICashShop::UICashShop() :
    tab_(0),
    page_(0),
    message_time_(0),
    locker_page_(0),
    pending_take_(0),
    last_tap_time_(-1000),
    last_tap_slot_(-1) {
    nl::node src = nl::nx::ui_img("CashShop.img");

    backgrnd_ = src["Base"]["backgrnd"];
    cell_ = src["CSList"]["Base"];

    for (size_t i = 0; i < NUM_TABS; i++) {
        tabs_[i] = src["CSTab"]["Tab"][std::to_string(i + 1)];
    }

    int16_t vw = Constants::Constants::get().get_viewwidth();
    int16_t vh = Constants::Constants::get().get_viewheight();

    position_ = Point<int16_t>((vw - 800) / 2, (vh - 600) / 2);
    dimension_ = Point<int16_t>(800, 600);

    buttons_[Buttons::BT_EXIT] =
        std::make_unique<MapleButton>(src["CSStatus"]["BtExit"], Point<int16_t>(632, 540));

    for (size_t i = 0; i < PER_PAGE; i++) {
        buttons_[Buttons::BT_BUY0 + i] = std::make_unique<MapleButton>(
            src["CSList"]["BtBuy"], cell_pos(i) + Point<int16_t>(78, 53));

        names_[i] = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::BLACK, "", 118);
        prices_[i] = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::MEDIUMBLUE);
        details_[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
    }

    page_label_ = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
    prev_label_ = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "◀");
    next_label_ = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "▶");
    empty_label_ = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE,
                        "판매 중인 상품이 없습니다.");
    locker_page_label_ = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::EMPEROR);
    message_label_ = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::YELLOW,
                          Text::Background::NAMETAG);

    char_label_ = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK,
                       Stage::get().get_player().get_name());

    for (auto &label : cash_labels_) {
        label = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK, "0");
    }

    load_commodities();
    select_tab(0);
}

void UICashShop::load_commodities() {
    bool female = Stage::get().get_player().get_stats().get_female();

    for (const nl::node &sub : nl::nx::etc["Commodity.img"]) {
        if (sub["OnSale"].get_integer() != 1) {
            continue;
        }

        int32_t gender = sub["Gender"] ? sub["Gender"].get_integer() : 2;

        if (gender != 2 && gender != (female ? 1 : 0)) {
            continue;
        }

        Commodity c;
        c.sn = sub["SN"];
        c.itemid = sub["ItemId"];
        c.count = static_cast<int16_t>(sub["Count"] ? sub["Count"].get_integer() : 1);
        c.period = sub["Period"] ? sub["Period"].get_integer() : 0;
        c.price = 0;

        int8_t tab = tab_of(c.sn);

        if (tab < 0 || !ItemData::get(c.itemid).is_valid()) {
            continue;
        }

        lists_[tab].push_back(c);
    }

    for (auto &list : lists_) {
        std::sort(list.begin(), list.end(),
                  [](const Commodity &a, const Commodity &b) { return a.sn < b.sn; });
    }
}

Point<int16_t> UICashShop::origin() const {
    return position_;
}

int16_t UICashShop::page_count() const {
    auto n = static_cast<int16_t>(lists_[tab_].size());

    return std::max<int16_t>(1, (n + PER_PAGE - 1) / PER_PAGE);
}

void UICashShop::select_tab(uint8_t tab) {
    tab_ = tab;
    set_page(0);
}

void UICashShop::set_page(int16_t page) {
    page_ = std::max<int16_t>(0, std::min<int16_t>(page, page_count() - 1));

    const auto &list = lists_[tab_];

    for (size_t i = 0; i < PER_PAGE; i++) {
        size_t index = static_cast<size_t>(page_) * PER_PAGE + i;
        bool shown = index < list.size();

        buttons_[Buttons::BT_BUY0 + i]->set_active(shown);

        if (!shown) {
            continue;
        }

        const Commodity &c = list[index];
        const ItemData &data = ItemData::get(c.itemid);

        names_[i].change_text(data.get_name());
        prices_[i].change_text(with_commas(c.price) + " NX");

        std::string detail;

        if (c.count > 1) {
            detail = "수량 " + std::to_string(c.count);
        }

        if (c.period > 0) {
            detail += (detail.empty() ? "" : "  ") + std::to_string(c.period) + "일";
        } else if (InventoryType::by_item_id(c.itemid) == InventoryType::EQUIP) {
            detail += detail.empty() ? "영구" : "  영구";
        }

        details_[i].change_text(detail);
    }

    page_label_.change_text(std::to_string(page_ + 1) + " / " + std::to_string(page_count()));
}

void UICashShop::draw(float inter) const {
    Point<int16_t> o = origin();

    backgrnd_.draw(DrawArgument(o));
    tabs_[tab_].draw(DrawArgument(o + TAB_POS));

    const auto &list = lists_[tab_];

    for (size_t i = 0; i < PER_PAGE; i++) {
        size_t index = static_cast<size_t>(page_) * PER_PAGE + i;

        if (index >= list.size()) {
            break;
        }

        Point<int16_t> p = o + cell_pos(i);

        cell_.draw(DrawArgument(p));
        ItemData::get(list[index].itemid)
            .get_icon(false)
            .draw(DrawArgument(p + Point<int16_t>(5, 69), 2.0f, 2.0f));
        names_[i].draw(DrawArgument(p + Point<int16_t>(134, 4)));
        prices_[i].draw(DrawArgument(p + Point<int16_t>(80, 22)));
        details_[i].draw(DrawArgument(p + Point<int16_t>(118, 22)));
    }

    if (list.empty()) {
        empty_label_.draw(DrawArgument(o + Point<int16_t>(480, 280)));
    }

    prev_label_.draw(DrawArgument(o + PAGE_POS - Point<int16_t>(60, 2)));
    page_label_.draw(DrawArgument(o + PAGE_POS));
    next_label_.draw(DrawArgument(o + PAGE_POS + Point<int16_t>(60, -2)));

    // cash locker
    size_t per = LOCKER_COLS * LOCKER_ROWS;

    for (size_t s = 0; s < per; s++) {
        size_t index = static_cast<size_t>(locker_page_) * per + s;

        if (index >= locker_.size()) {
            break;
        }

        Point<int16_t> cell = o + Point<int16_t>(LOCKER_X0 + LOCKER_D * static_cast<int16_t>(s % LOCKER_COLS),
                                                 LOCKER_Y0 + LOCKER_D * static_cast<int16_t>(s / LOCKER_COLS));

        ItemData::get(locker_[index].itemid)
            .get_icon(false)
            .draw(DrawArgument(cell + Point<int16_t>(0, LOCKER_CELL)));
    }

    if (locker_.size() > per) {
        locker_page_label_.draw(DrawArgument(o + LOCKER_PAGE_POS));
    }

    char_label_.draw(DrawArgument(o + Point<int16_t>(95, 262)));

    for (size_t i = 0; i < cash_labels_.size(); i++) {
        cash_labels_[i].draw(DrawArgument(o + Point<int16_t>(520, 541 + 12 * static_cast<int16_t>(i))));
    }

    UIElement::draw_buttons(inter);

    if (message_time_ > 0) {
        message_label_.draw(DrawArgument(o + Point<int16_t>(400, 290)));
    }
}

void UICashShop::update() {
    UIElement::update();

    last_tap_time_++;

    if (message_time_ > 0) {
        message_time_--;
    }
}

Button::State UICashShop::button_pressed(uint16_t buttonid) {
    if (buttonid == Buttons::BT_EXIT) {
        exit_cashshop();

        return Button::State::NORMAL;
    }

    if (buttonid >= Buttons::BT_BUY0 && buttonid <= Buttons::BT_BUY_LAST) {
        buy(buttonid - Buttons::BT_BUY0);
    }

    return Button::State::NORMAL;
}

void UICashShop::buy(size_t cell) {
    size_t index = static_cast<size_t>(page_) * PER_PAGE + cell;
    const auto &list = lists_[tab_];

    if (index < list.size()) {
        CashShopBuyPacket(list[index].sn).dispatch();
    }
}

void UICashShop::take(size_t slot) {
    if (slot >= locker_.size() || pending_take_ != 0) {
        return;
    }

    const LockerItem &item = locker_[slot];

    pending_take_ = item.cashid;
    CashShopTakePacket(item.cashid, InventoryType::by_item_id(item.itemid)).dispatch();
}

Cursor::State UICashShop::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    Cursor::State state = UIElement::send_cursor(clicked, cursorpos);

    if (!clicked) {
        return state;
    }

    Point<int16_t> p = cursorpos - origin();

    // tabs
    if (p.y() >= TAB_POS.y() + TAB_TOP && p.y() <= TAB_POS.y() + TAB_BOTTOM) {
        for (uint8_t t = 0; t < NUM_TABS; t++) {
            int16_t x0 = TAB_POS.x() + TAB_X[t];

            if (p.x() >= x0 && p.x() < x0 + TAB_W[t]) {
                if (t != tab_) {
                    select_tab(t);
                }

                return Cursor::State::CLICKING;
            }
        }
    }

    // page arrows
    if (std::abs(p.y() - (PAGE_POS.y() + 6)) <= 12) {
        if (std::abs(p.x() - (PAGE_POS.x() - 60)) <= 18) {
            set_page(page_ - 1);

            return Cursor::State::CLICKING;
        }

        if (std::abs(p.x() - (PAGE_POS.x() + 60)) <= 18) {
            set_page(page_ + 1);

            return Cursor::State::CLICKING;
        }
    }

    // cash locker: tap the page label to flip, double tap an item to move it
    // into the item inventory
    size_t per = LOCKER_COLS * LOCKER_ROWS;

    if (locker_.size() > per && std::abs(p.x() - LOCKER_PAGE_POS.x()) <= 30
        && std::abs(p.y() - (LOCKER_PAGE_POS.y() + 6)) <= 10) {
        int16_t pages = static_cast<int16_t>((locker_.size() + per - 1) / per);

        locker_page_ = static_cast<int16_t>((locker_page_ + 1) % pages);
        locker_page_label_.change_text(std::to_string(locker_page_ + 1) + "/" + std::to_string(pages));

        return Cursor::State::CLICKING;
    }

    int16_t lx = p.x() - LOCKER_X0;
    int16_t ly = p.y() - LOCKER_Y0;

    if (lx >= 0 && ly >= 0 && lx < LOCKER_D * LOCKER_COLS && ly < LOCKER_D * LOCKER_ROWS
        && lx % LOCKER_D < LOCKER_CELL && ly % LOCKER_D < LOCKER_CELL) {
        auto slot = static_cast<int16_t>(locker_page_ * per + (ly / LOCKER_D) * LOCKER_COLS
                                         + lx / LOCKER_D);

        // update() runs about 125 times a second
        if (slot == last_tap_slot_ && last_tap_time_ < 60) {
            take(static_cast<size_t>(slot));
            last_tap_slot_ = -1;
        } else {
            last_tap_slot_ = slot;
            last_tap_time_ = 0;
        }

        return Cursor::State::CLICKING;
    }

    return state;
}

void UICashShop::send_scroll(double yoffset) {
    if (yoffset > 0) {
        set_page(page_ - 1);
    } else if (yoffset < 0) {
        set_page(page_ + 1);
    }
}

void UICashShop::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed && escape) {
        exit_cashshop();
    }
}

bool UICashShop::is_in_range(Point<int16_t>) const {
    return true;
}

UIElement::Type UICashShop::get_type() const {
    return TYPE;
}

void UICashShop::set_locker(std::vector<LockerItem> items) {
    locker_ = std::move(items);
    locker_page_ = 0;

    size_t per = LOCKER_COLS * LOCKER_ROWS;
    auto pages = static_cast<int16_t>(std::max<size_t>(1, (locker_.size() + per - 1) / per));

    locker_page_label_.change_text("1/" + std::to_string(pages));
}

void UICashShop::add_locker(const LockerItem &item) {
    locker_.push_back(item);

    size_t per = LOCKER_COLS * LOCKER_ROWS;
    auto pages = static_cast<int16_t>((locker_.size() + per - 1) / per);

    locker_page_ = static_cast<int16_t>(pages - 1);
    locker_page_label_.change_text(std::to_string(locker_page_ + 1) + "/" + std::to_string(pages));

    show_message(ItemData::get(item.itemid).get_name() + " 구매 완료 (캐시 보관함)");
}

void UICashShop::taken() {
    auto it = std::find_if(locker_.begin(), locker_.end(),
                           [&](const LockerItem &i) { return i.cashid == pending_take_; });

    if (it != locker_.end()) {
        show_message(ItemData::get(it->itemid).get_name() + " → 인벤토리");
        locker_.erase(it);
    }

    pending_take_ = 0;

    size_t per = LOCKER_COLS * LOCKER_ROWS;
    auto pages = static_cast<int16_t>(std::max<size_t>(1, (locker_.size() + per - 1) / per));

    locker_page_ = std::min<int16_t>(locker_page_, pages - 1);
    locker_page_label_.change_text(std::to_string(locker_page_ + 1) + "/" + std::to_string(pages));
}

void UICashShop::set_cash(int32_t credit, int32_t points, int32_t prepaid) {
    cash_labels_[0].change_text(with_commas(credit));
    cash_labels_[1].change_text(with_commas(prepaid));
    cash_labels_[2].change_text(with_commas(points));
}

void UICashShop::show_message(const std::string &text) {
    message_label_.change_text(text);
    message_time_ = 250;

    if (pending_take_ != 0 && text.find("실패") != std::string::npos) {
        pending_take_ = 0;
    }
}
}  // namespace ms
