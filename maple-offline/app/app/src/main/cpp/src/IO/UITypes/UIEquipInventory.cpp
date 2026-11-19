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
#include "UIEquipInventory.h"

#include <nlnx/nx.hpp>

#include "../../Audio/Audio.h"
#include "../../Data/ItemData.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "../../Gameplay/Stage.h"
#include "../UITypes/UIItemInventory.h"

namespace ms {
namespace {
const Point<int16_t> PET_CELL(12 + 33 * 3, 11 + 33 * 2);
auto fn_equip_item = []<typename... T>(T && ...args) {
    EquipItemPacket(std::forward<T>(args)...).dispatch();
};
auto fn_unequip_item = []<typename... T>(T && ...args) {
    UnequipItemPacket(std::forward<T>(args)...).dispatch();
};
}  // namespace

UIEquipInventory::UIEquipInventory(const Inventory &invent) :
    UIDragElement<PosEQINV>(),
    inventory_(invent),
    tab_(Buttons::BT_TAB1),
    has_pendant_slot_(false),
    has_pocket_slot_(false) {
    // maple-offline: v83 UIWindow.img/Equip/backgrnd (175x304). Cells are
    // 32x32 on a 33 px grid starting at (5,35); positions follow the labels
    // printed in the background.
    for (auto id : EquipSlot::values) {
        icon_positions_[id] = Point<int16_t>(-1000, -1000);
    }

    auto cell = [](int c, int r) { return Point<int16_t>(5 + 33 * c, 35 + 33 * r); };

    icon_positions_[EquipSlot::Id::HAT] = cell(1, 0);
    icon_positions_[EquipSlot::Id::MEDAL] = cell(0, 1);
    icon_positions_[EquipSlot::Id::FACE] = cell(1, 1);
    icon_positions_[EquipSlot::Id::RING1] = cell(3, 1);
    icon_positions_[EquipSlot::Id::RING2] = cell(4, 1);
    icon_positions_[EquipSlot::Id::EYE_ACC] = cell(2, 2);
    icon_positions_[EquipSlot::Id::EARACC] = cell(3, 2);
    icon_positions_[EquipSlot::Id::CAPE] = cell(0, 3);
    icon_positions_[EquipSlot::Id::TOP] = cell(1, 3);
    icon_positions_[EquipSlot::Id::PENDANT1] = cell(2, 3);
    icon_positions_[EquipSlot::Id::WEAPON] = cell(3, 3);
    icon_positions_[EquipSlot::Id::SHIELD] = cell(4, 3);
    icon_positions_[EquipSlot::Id::GLOVES] = cell(0, 4);
    icon_positions_[EquipSlot::Id::BOTTOM] = cell(1, 4);
    icon_positions_[EquipSlot::Id::BELT] = cell(2, 4);
    icon_positions_[EquipSlot::Id::RING3] = cell(3, 4);
    icon_positions_[EquipSlot::Id::RING4] = cell(4, 4);
    icon_positions_[EquipSlot::Id::SHOES] = cell(2, 5);
    icon_positions_[EquipSlot::Id::TAMEDMOB] = cell(0, 6);
    icon_positions_[EquipSlot::Id::SADDLE] = cell(1, 6);

    nl::node close = nl::nx::ui_img("Basic.img")["BtClose"];
    nl::node Equip = nl::nx::ui_img("UIWindow.img")["Equip"];

    background_[Buttons::BT_TAB0] = Equip["backgrnd"];
    pet_bg_ = Equip["pet"];
    Point<int16_t> bg_dimensions = background_[Buttons::BT_TAB0].get_dimensions();

    totem_dimensions_ = Point<int16_t>(0, 0);
    totem_adj_ = Point<int16_t>(0, 0);

    buttons_[Buttons::BT_CLOSE] =
        std::make_unique<MapleButton>(close, Point<int16_t>(bg_dimensions.x() - 18, 6));

    // Later-version tabs and buttons do not exist in v83; keep them inert.
    const uint16_t unused[] = { Buttons::BT_SLOT, Buttons::BT_EFFECT, Buttons::BT_SALON,
                                Buttons::BT_CONSUMESETTING, Buttons::BT_EXCEPTION,
                                Buttons::BT_SHOP };

    for (uint16_t id : unused) {
        buttons_[id] = std::make_unique<MapleButton>(nl::node());
        buttons_[id]->set_active(false);
    }

    for (int i = Buttons::BT_TAB0; i < Buttons::BT_TABE; i++) {
        buttons_[Buttons::BT_TAB0 + i] = std::make_unique<MapleButton>(nl::node());
        buttons_[Buttons::BT_TAB0 + i]->set_active(false);
    }

    dimension_ = bg_dimensions;
    drag_area_ = Point<int16_t>(bg_dimensions.x(), 20);

    load_icons();
    change_tab(Buttons::BT_TAB0);
}

void UIEquipInventory::draw(float alpha) const {
    background_[Buttons::BT_TAB0].draw(position_);

    for (auto [eq_slot, icon] : icons_) {
        if (icon) {
            icon->draw(position_ + icon_positions_[eq_slot]);
        }
    }

    if (pet_visible()) {
        pet_bg_.draw(position_ + pet_origin());

        if (pet_icon_) {
            pet_icon_->draw(position_ + pet_origin() + PET_CELL);
        }
    }

    UIElement::draw_buttons(alpha);
}

bool UIEquipInventory::pet_visible() const {
    if (inventory_.get_item_id(InventoryType::Id::EQUIPPED, PET_SLOT)) {
        return true;
    }

    return Stage::get().get_player().get_pet(0) != nullptr;
}

Point<int16_t> UIEquipInventory::pet_origin() const {
    return Point<int16_t>(dimension_.x(), 0);
}

bool UIEquipInventory::in_pet_cell(Point<int16_t> cursorpos) const {
    if (!pet_visible()) {
        return false;
    }

    Point<int16_t> lt = position_ + pet_origin() + PET_CELL;

    return Rectangle<int16_t>(lt, lt + Point<int16_t>(32, 32)).contains(cursorpos);
}

void UIEquipInventory::update_pet_slot() {
    if (int32_t item_id =
            inventory_.get_item_id(InventoryType::Id::EQUIPPED, PET_SLOT)) {
        pet_icon_ = std::make_unique<Icon>(std::make_unique<EquipIcon>(PET_SLOT),
                                           ItemData::get(item_id).get_icon(false),
                                           -1);
    } else {
        pet_icon_.reset();
    }

    clear_tooltip();
}

Button::State UIEquipInventory::button_pressed(uint16_t id) {
    switch (id) {
        case Buttons::BT_CLOSE: toggle_active(); break;
        case Buttons::BT_TAB0:
        case Buttons::BT_TAB1:
        case Buttons::BT_TAB2:
        case Buttons::BT_TAB3: change_tab(id); return Button::State::IDENTITY;
        default: break;
    }

    return Button::State::NORMAL;
}

void UIEquipInventory::update_slot(EquipSlot::Id slot) {
    if (int32_t item_id =
            inventory_.get_item_id(InventoryType::Id::EQUIPPED, slot)) {
        const Texture &texture = ItemData::get(item_id).get_icon(false);

        icons_[slot] = std::make_unique<Icon>(std::make_unique<EquipIcon>(slot),
                                              texture,
                                              -1);
    } else if (icons_[slot]) {
        icons_[slot].release();
    }

    clear_tooltip();
}

void UIEquipInventory::load_icons() {
    icons_.clear();

    for (auto iter : EquipSlot::values) {
        update_slot(iter);
    }

    update_pet_slot();
}

Cursor::State UIEquipInventory::send_cursor(bool pressed,
                                            Point<int16_t> cursorpos) {
    Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);

    if (dragged_) {
        clear_tooltip();

        return dstate;
    }

    if (in_pet_cell(cursorpos)) {
        if (pet_icon_) {
            if (pressed) {
                pet_icon_->start_drag(cursorpos - position_ - pet_origin() - PET_CELL);
                UI::get().drag_icon(pet_icon_.get());
                clear_tooltip();

                return Cursor::State::GRABBING;
            }

            UI::get().show_equip(Tooltip::Parent::EQUIP_INVENTORY, PET_SLOT);

            return Cursor::State::CAN_GRAB;
        }

        clear_tooltip();

        return Cursor::State::IDLE;
    }

    EquipSlot::Id slot = slot_by_position(cursorpos);

    if (auto *icon = icons_[slot].get()) {
        if (pressed) {
            icon->start_drag(cursorpos - position_ - icon_positions_[slot]);

            UI::get().drag_icon(icon);

            clear_tooltip();

            return Cursor::State::GRABBING;
        }
        show_equip(slot);

        return Cursor::State::CAN_GRAB;
    }
    clear_tooltip();

    return Cursor::State::IDLE;
}

void UIEquipInventory::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (escape) {
            toggle_active();
        } else if (keycode == KeyAction::Id::TAB) {
            uint16_t newtab = tab_ + 1;

            if (newtab >= Buttons::BT_TABE) {
                newtab = Buttons::BT_TAB0;
            }

            change_tab(newtab);
        }
    }
}

UIElement::Type UIEquipInventory::get_type() const {
    return TYPE;
}

void UIEquipInventory::doubleclick(Point<int16_t> cursorpos) {
    if (in_pet_cell(cursorpos)) {
        if (pet_icon_) {
            if (int16_t freeslot =
                    inventory_.find_free_slot(InventoryType::Id::EQUIP)) {
                fn_unequip_item(PET_SLOT, freeslot);
            }
        }

        return;
    }

    EquipSlot::Id slot = slot_by_position(cursorpos);

    if (icons_[slot]) {
        if (int16_t freeslot =
                inventory_.find_free_slot(InventoryType::Id::EQUIP)) {
            fn_unequip_item(slot, freeslot);
        }
    }
}

bool UIEquipInventory::is_in_range(Point<int16_t> cursorpos) const {
    Rectangle<int16_t> bounds =
        Rectangle<int16_t>(position_, position_ + dimension_);

    Rectangle<int16_t> totem_bounds =
        Rectangle<int16_t>(position_, position_ + totem_dimensions_);
    totem_bounds.shift(totem_adj_);

    if (pet_visible()) {
        Rectangle<int16_t> pet_bounds(position_ + pet_origin(),
                                      position_ + pet_origin() + pet_bg_.get_dimensions());

        if (pet_bounds.contains(cursorpos)) {
            return true;
        }
    }

    return bounds.contains(cursorpos) || totem_bounds.contains(cursorpos);
}

bool UIEquipInventory::send_icon(const Icon &icon, Point<int16_t> cursorpos) {
    if (in_pet_cell(cursorpos)) {
        icon.drop_on_equips(static_cast<EquipSlot::Id>(PET_SLOT));

        return true;
    }

    if (EquipSlot::Id slot = slot_by_position(cursorpos)) {
        icon.drop_on_equips(slot);
    }

    return true;
}

void UIEquipInventory::toggle_active() {
    clear_tooltip();

    UIElement::toggle_active();
}

void UIEquipInventory::modify(int16_t pos, int8_t mode, int16_t arg) {
    if (pos == PET_SLOT || arg == PET_SLOT) {
        update_pet_slot();

        if (pos == PET_SLOT && (mode == 0 || mode == 3)) {
            return;
        }
    }

    EquipSlot::Id eqpos = EquipSlot::by_id(pos);
    EquipSlot::Id eqarg = EquipSlot::by_id(arg);

    switch (mode) {
        case 0:
        case 3: update_slot(eqpos); break;
        case 2:
            update_slot(eqpos);
            update_slot(eqarg);
            break;
    }
}

void UIEquipInventory::show_equip(EquipSlot::Id slot) {
    UI::get().show_equip(Tooltip::Parent::EQUIP_INVENTORY, slot);
}

void UIEquipInventory::clear_tooltip() {
    UI::get().clear_tooltip(Tooltip::Parent::EQUIP_INVENTORY);
}

EquipSlot::Id UIEquipInventory::slot_by_position(
    Point<int16_t> cursorpos) const {
    if (tab_ != Buttons::BT_TAB0) {
        return EquipSlot::Id::NONE;
    }

    for (auto [eq_slot, icon_pos] : icon_positions_) {
        Rectangle<int16_t> iconrect =
            Rectangle<int16_t>(position_ + icon_pos,
                               position_ + icon_pos + Point<int16_t>(32, 32));

        if (iconrect.contains(cursorpos)) {
            return eq_slot;
        }
    }

    return EquipSlot::Id::NONE;
}

void UIEquipInventory::change_tab(uint16_t tabid) {
    uint8_t oldtab = tab_;
    tab_ = tabid;

    if (oldtab != tab_) {
        clear_tooltip();

        buttons_[oldtab]->set_state(Button::State::NORMAL);
        buttons_[tab_]->set_state(Button::State::PRESSED);

        if (tab_ == Buttons::BT_TAB0) {
            buttons_[Buttons::BT_SLOT]->set_active(true);
        } else {
            buttons_[Buttons::BT_SLOT]->set_active(false);
        }

        if (tab_ == Buttons::BT_TAB2) {
            buttons_[Buttons::BT_CONSUMESETTING]->set_active(true);
            buttons_[Buttons::BT_EXCEPTION]->set_active(true);
        } else {
            buttons_[Buttons::BT_CONSUMESETTING]->set_active(false);
            buttons_[Buttons::BT_EXCEPTION]->set_active(false);
        }

        if (tab_ == Buttons::BT_TAB3) {
            buttons_[Buttons::BT_SHOP]->set_active(true);
        } else {
            buttons_[Buttons::BT_SHOP]->set_active(false);
        }
    }
}

UIEquipInventory::EquipIcon::EquipIcon(int16_t s) {
    source_ = s;
}

void UIEquipInventory::EquipIcon::drop_on_stage() const {
    Sound(Sound::Name::DRAG_END).play();
}

void UIEquipInventory::EquipIcon::drop_on_equips(EquipSlot::Id slot) const {
    if (source_ == slot) {
        Sound(Sound::Name::DRAG_END).play();
    }
}

bool UIEquipInventory::EquipIcon::drop_on_items(InventoryType::Id tab,
                                                EquipSlot::Id eqslot,
                                                int16_t slot,
                                                bool equip) const {
    if (tab != InventoryType::Id::EQUIP) {
        if (auto iteminventory = UI::get().get_element<UIItemInventory>()) {
            if (iteminventory->get().is_active()) {
                iteminventory->get().change_tab(InventoryType::Id::EQUIP);
                return false;
            }
        }
    }

    if (equip) {
        if (eqslot == source_) {
            fn_equip_item(slot, eqslot);
        }
    } else {
        fn_unequip_item(source_, slot);
    }

    return true;
}

Icon::IconType UIEquipInventory::EquipIcon::get_type() {
    return Icon::IconType::EQUIP;
}
}  // namespace ms