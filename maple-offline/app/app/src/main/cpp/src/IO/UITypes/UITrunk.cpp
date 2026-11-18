#include "UITrunk.h"

#include <algorithm>
#include <nlnx/nx.hpp>
#include <string>

#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "../Data/ItemData.h"
#include "../Gameplay/Stage.h"
#include "../Graphics/Geometry.h"
#include "../Net/OutPacket.h"
#include "../UI.h"
#include "../Util/StringHandling.h"
#include "UINotice.h"

namespace ms {
namespace {
// STORAGE(62): 4 take out (type, index), 5 store (slot, id, qty),
// 7 mesos (+deposit / -withdraw as the server expects), 8 close
class StoragePacket : public OutPacket {
public:
    explicit StoragePacket(int8_t mode) : OutPacket(62) { write_byte(mode); }

    static void take_out(InventoryType::Id type, int8_t index) {
        StoragePacket p(4);
        p.write_byte(static_cast<int8_t>(type));
        p.write_byte(index);
        p.dispatch();
    }

    static void store(int16_t slot, int32_t itemid, int16_t qty) {
        StoragePacket p(5);
        p.write_short(slot);
        p.write_int(itemid);
        p.write_short(qty);
        p.dispatch();
    }

    static void meso(int32_t amount) {
        StoragePacket p(7);
        p.write_int(amount);
        p.dispatch();
    }

    static void close() { StoragePacket(8).dispatch(); }
};

std::string with_commas(int64_t value) {
    std::string s = std::to_string(value);
    string_format::split_number(s);
    return s;
}
}  // namespace

UITrunk::UITrunk(int32_t npcid, uint8_t slots, int32_t meso, std::vector<Entry> items) :
    UIDragElement<PosSHOP>(),
    npcid_(npcid),
    slots_(slots),
    meso_(meso),
    stored_(std::move(items)),
    tab_(InventoryType::Id::EQUIP) {
    nl::node src = nl::nx::ui["UIWindow.img"]["Trunk"];
    Texture bg = src["backgrnd"];
    sprites_.emplace_back(src["backgrnd"]);

    buttons_[GET] = std::make_unique<MapleButton>(src["BtGet"], Point<int16_t>(126, 10));
    buttons_[GET_ALL] = std::make_unique<MapleButton>(src["BtGetAll"], Point<int16_t>(126, 31));
    buttons_[OUT_COIN] = std::make_unique<MapleButton>(src["BtOutCoin"], Point<int16_t>(160, 52));
    buttons_[PUT] = std::make_unique<MapleButton>(src["BtPut"], Point<int16_t>(356, 10));
    buttons_[IN_COIN] = std::make_unique<MapleButton>(src["BtInCoin"], Point<int16_t>(390, 31));
    buttons_[EXIT] = std::make_unique<MapleButton>(src["BtExit"], Point<int16_t>(356, 52));
    buttons_[GET_ALL]->set_active(false);  // not in the server protocol

    nl::node en = src["Tab"]["enabled"];
    nl::node dis = src["Tab"]["disabled"];

    for (uint16_t i = 0; i < 5; i++) {
        std::string k = std::to_string(i);
        Texture d = dis[k];
        Texture e = en[k];
        int16_t center = 258 + i * 44;
        buttons_[TAB0 + i] = std::make_unique<TwoSpriteButton>(
            dis[k], en[k], Point<int16_t>(center - d.width() / 2, 100),
            Point<int16_t>(center - e.width() / 2, 100));
    }

    std::string strid = std::to_string(npcid);
    strid.insert(0, 7 - std::min<size_t>(7, strid.size()), '0');
    npc_ = nl::nx::npc[strid + ".img"]["stand"]["0"];

    store_meso_ = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);
    own_meso_ = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);
    slots_label_ = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::DARKGREY);

    dimension_ = bg.get_dimensions();
    drag_area_ = Point<int16_t>(dimension_.x(), 10);

    rebuild();
}

UITrunk::Row UITrunk::make_row(int32_t itemid, int16_t count, int16_t slot) const {
    const ItemData &data = ItemData::get(itemid);

    return Row{data.get_icon(false),
               Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, data.get_name(), 150),
               Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::DARKGREY,
                    count > 1 ? "x " + std::to_string(count) : ""),
               slot,
               itemid,
               count};
}

void UITrunk::rebuild() {
    left_.clear();
    right_.clear();

    int16_t index = 0;

    for (const Entry &e : stored_) {
        if (e.type == tab_) {
            left_.push_back(make_row(e.itemid, e.count, index));
            index++;
        }
    }

    const Inventory &inv = Stage::get().get_player().get_inventory();
    uint8_t max = inv.get_slotmax(tab_);

    for (int16_t slot = 1; slot <= max; slot++) {
        int32_t id = inv.get_item_id(tab_, slot);

        if (id != 0) {
            right_.push_back(make_row(id, inv.get_item_count(tab_, slot), slot));
        }
    }

    left_offset_ = std::clamp<int16_t>(left_offset_, 0,
                                       std::max<int16_t>(0, static_cast<int16_t>(left_.size()) - ROWS_));
    right_offset_ = std::clamp<int16_t>(right_offset_, 0,
                                        std::max<int16_t>(0, static_cast<int16_t>(right_.size()) - RIGHT_ROWS_));

    if (left_sel_ >= static_cast<int16_t>(left_.size())) {
        left_sel_ = -1;
    }

    if (right_sel_ >= static_cast<int16_t>(right_.size())) {
        right_sel_ = -1;
    }

    for (uint16_t i = 0; i < 5; i++) {
        buttons_[TAB0 + i]->set_state(i + 1 == tab_ ? Button::State::PRESSED : Button::State::NORMAL);
    }

    slots_label_.change_text(std::to_string(stored_.size()) + " / " + std::to_string(slots_));
}

void UITrunk::set_items(uint8_t slots, InventoryType::Id type, std::vector<Entry> items) {
    // the server resends one tab after each change (or everything when
    // arranging): replace exactly that part
    if (type == InventoryType::Id::NONE) {
        stored_.clear();
    } else {
        stored_.erase(std::remove_if(stored_.begin(), stored_.end(),
                                     [&](const Entry &e) { return e.type == type; }),
                      stored_.end());
    }

    stored_.insert(stored_.end(), items.begin(), items.end());
    slots_ = slots;
    left_sel_ = right_sel_ = -1;
    rebuild();
}

void UITrunk::set_meso(int32_t meso) {
    meso_ = meso;
}

void UITrunk::update() {
    UIElement::update();

    store_meso_.change_text(with_commas(meso_));
    own_meso_.change_text(with_commas(Stage::get().get_player().get_inventory().get_meso()));
}

void UITrunk::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    npc_.draw(DrawArgument(position_ + Point<int16_t>(56, 76), true));

    auto draw_rows = [&](const std::vector<Row> &rows, int16_t offset, int16_t sel, int16_t x, int16_t y) {
        int16_t count = x == RIGHT_X_ ? RIGHT_ROWS_ : ROWS_;

        for (int16_t i = 0; i < count; i++) {
            int16_t idx = offset + i;

            if (idx >= static_cast<int16_t>(rows.size())) {
                break;
            }

            Point<int16_t> p = position_ + Point<int16_t>(x, y + i * ROW_H_);

            if (idx == sel) {
                ColorBox(ROW_W_ - 38, 34, Color::Name::MEDIUMBLUE, 0.3f)
                    .draw(DrawArgument(p + Point<int16_t>(38, 0)));
            }

            rows[idx].icon.draw(DrawArgument(p + Point<int16_t>(4, 34)));
            rows[idx].name.draw(p + Point<int16_t>(42, 2));
            rows[idx].count.draw(p + Point<int16_t>(42, 18));
        }
    };

    draw_rows(left_, left_offset_, left_sel_, LEFT_X_, LEFT_Y_);
    draw_rows(right_, right_offset_, right_sel_, RIGHT_X_, RIGHT_Y_);

    store_meso_.draw(position_ + Point<int16_t>(192, 295));
    own_meso_.draw(position_ + Point<int16_t>(422, 295));
    slots_label_.draw(position_ + Point<int16_t>(126, 72));

    UIElement::draw_buttons(alpha);
}

Cursor::State UITrunk::send_cursor(bool clicked, Point<int16_t> cursorpos) {
    last_cursor_ = cursorpos;
    Point<int16_t> rel = cursorpos - position_;

    auto hit = [&](int16_t x, int16_t y, int16_t offset, size_t size) -> int16_t {
        int16_t rows = x == RIGHT_X_ ? RIGHT_ROWS_ : ROWS_;

        if (rel.x() < x || rel.x() >= x + ROW_W_ || rel.y() < y || rel.y() >= y + rows * ROW_H_) {
            return -1;
        }

        int16_t idx = offset + (rel.y() - y) / ROW_H_;

        return idx < static_cast<int16_t>(size) ? idx : -1;
    };

    int16_t l = hit(LEFT_X_, LEFT_Y_, left_offset_, left_.size());
    int16_t r = hit(RIGHT_X_, RIGHT_Y_, right_offset_, right_.size());

    if (l >= 0 || r >= 0) {
        int32_t itemid = l >= 0 ? left_[l].itemid : right_[r].itemid;
        UI::get().show_item(Tooltip::Parent::SHOP, itemid);

        if (clicked) {
            if (l >= 0) {
                left_sel_ = l;
                right_sel_ = -1;
            } else {
                right_sel_ = r;
                left_sel_ = -1;
            }

            return Cursor::State::CLICKING;
        }

        return Cursor::State::CAN_CLICK;
    }

    UI::get().clear_tooltip(Tooltip::Parent::SHOP);

    return UIDragElement::send_cursor(clicked, cursorpos);
}

void UITrunk::send_scroll(double yoffset) {
    Point<int16_t> rel = last_cursor_ - position_;
    int16_t step = yoffset > 0 ? -1 : 1;

    if (rel.x() < RIGHT_X_) {
        left_offset_ = std::clamp<int16_t>(left_offset_ + step, 0,
                                           std::max<int16_t>(0, static_cast<int16_t>(left_.size()) - ROWS_));
    } else {
        right_offset_ = std::clamp<int16_t>(right_offset_ + step, 0,
                                            std::max<int16_t>(0, static_cast<int16_t>(right_.size()) - RIGHT_ROWS_));
    }
}

void UITrunk::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed && escape) {
        close();
    }
}

void UITrunk::close() {
    StoragePacket::close();
    UI::get().clear_tooltip(Tooltip::Parent::SHOP);
    deactivate();
}

Button::State UITrunk::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case GET:
            if (left_sel_ >= 0) {
                StoragePacket::take_out(tab_, static_cast<int8_t>(left_[left_sel_].slot));
            }

            break;
        case PUT:
            if (right_sel_ >= 0) {
                const Row row = right_[right_sel_];

                if (row.quantity > 1) {
                    UI::get().emplace<UIEnterNumber>("몇 개를 맡기시겠습니까?",
                                                     [row](int32_t qty) {
                                                         StoragePacket::store(row.slot, row.itemid,
                                                                              static_cast<int16_t>(qty));
                                                     },
                                                     row.quantity, row.quantity);
                } else {
                    StoragePacket::store(row.slot, row.itemid, 1);
                }
            }

            break;
        case IN_COIN: {
            auto own = static_cast<int32_t>(Stage::get().get_player().get_inventory().get_meso());

            if (own > 0) {
                // the server takes a negative amount as "put in"
                UI::get().emplace<UIEnterNumber>("맡길 메소를 입력하세요.",
                                                 [](int32_t m) { StoragePacket::meso(-m); },
                                                 own, 0);
            }

            break;
        }
        case OUT_COIN:
            if (meso_ > 0) {
                UI::get().emplace<UIEnterNumber>("찾을 메소를 입력하세요.",
                                                 [](int32_t m) { StoragePacket::meso(m); },
                                                 meso_, 0);
            }

            break;
        case EXIT: close(); return Button::State::NORMAL;
        default:
            if (buttonid >= TAB0 && buttonid <= TAB4) {
                tab_ = static_cast<InventoryType::Id>(buttonid - TAB0 + 1);
                left_offset_ = right_offset_ = 0;
                left_sel_ = right_sel_ = -1;
                rebuild();
                return Button::State::PRESSED;
            }

            break;
    }

    return Button::State::NORMAL;
}

UIElement::Type UITrunk::get_type() const {
    return TYPE;
}
}  // namespace ms
