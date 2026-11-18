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

#include "../../Audio/Audio.h"
#include "../../Character/Inventory/Inventory.h"
#include "../OutPacket.h"

namespace ms {
// Packet which requests that the inventory is sorted
// Opcode: GATHER_ITEMS(69)
class GatherItemsPacket : public OutPacket {
public:
    GatherItemsPacket(InventoryType::Id type) :
        OutPacket(OutPacket::Opcode::GATHER_ITEMS) {
        write_time();
        write_byte(type);
    }
};

// Packet which requests that the inventory is sorted
// Opcode: SORT_ITEMS(70)
class SortItemsPacket : public OutPacket {
public:
    SortItemsPacket(InventoryType::Id type) :
        OutPacket(OutPacket::Opcode::SORT_ITEMS) {
        write_time();
        write_byte(type);
    }
};

// Packet which requests that an item is moved
// Opcode: MOVE_ITEM(71)
class MoveItemPacket : public OutPacket {
public:
    MoveItemPacket(InventoryType::Id type,
                   int16_t slot,
                   int16_t action,
                   int16_t qty) :
        OutPacket(OutPacket::Opcode::MOVE_ITEM) {
        write_time();
        write_byte(type);
        write_short(slot);
        write_short(action);
        write_short(qty);
    }
};

// Packet which requests that an item is equipped
// Opcode: MOVE_ITEM(71)
class EquipItemPacket : public MoveItemPacket {
public:
    EquipItemPacket(int16_t src, EquipSlot::Id dest) :
        MoveItemPacket(InventoryType::Id::EQUIP, src, -dest, 1) {}
};

// Packet which requests that an item is unequipped
// Opcode: MOVE_ITEM(71)
class UnequipItemPacket : public MoveItemPacket {
public:
    UnequipItemPacket(int16_t src, int16_t dest) :
        MoveItemPacket(InventoryType::Id::EQUIPPED, -src, dest, 1) {}
};

// A packet which requests that an 'USE' item is used
// Opcode: USE_ITEM(72)
// maple-offline: summon / dismiss a pet from the cash tab
// Opcode: SPAWN_PET(98): time, slot, 0, lead
class SpawnPetPacket : public OutPacket {
public:
    SpawnPetPacket(int16_t slot) : OutPacket(98) {
        write_time();
        write_byte(static_cast<int8_t>(slot));
        write_byte(0);
        write_byte(1);
    }
};

// maple-offline: feed the pet
// Opcode: PET_FOOD(76): time, slot, item id
class PetFoodPacket : public OutPacket {
public:
    PetFoodPacket(int16_t slot, int32_t itemid) : OutPacket(76) {
        write_time();
        write_short(slot);
        write_int(itemid);
    }
};

// maple-offline: sit on a chair item / stand up
// Opcode: USE_CHAIR(43) item id, CANCEL_CHAIR(42) -1
class UseChairPacket : public OutPacket {
public:
    UseChairPacket(int32_t itemid) : OutPacket(43) { write_int(itemid); }
};

class CancelChairPacket : public OutPacket {
public:
    CancelChairPacket() : OutPacket(42) { write_short(-1); }
};

class UseItemPacket : public OutPacket {
public:
    UseItemPacket(int16_t slot, int32_t itemid) :
        OutPacket(OutPacket::Opcode::USE_ITEM) {
        Sound(itemid).play();

        write_time();
        write_short(slot);
        write_int(itemid);
    }
};

// Requests using a scroll on an equip
// Opcode: SCROLL_EQUIP(86)
class ScrollEquipPacket : public OutPacket {
public:
    enum Flag : uint8_t { NONE = 0x00, UNKNOWN = 0x01, WHITESCROLL = 0x02 };

    ScrollEquipPacket(int16_t source, EquipSlot::Id target, uint8_t flags) :
        OutPacket(OutPacket::Opcode::SCROLL_EQUIP) {
        write_time();
        write_short(source);
        write_short(-target);
        write_short(flags);
    }

    ScrollEquipPacket(int16_t source, EquipSlot::Id target) :
        ScrollEquipPacket(source, target, 0) {}
};

// Packet which requests that mesos are dropped
// Opcode: DROP_MESOS(94)
class DropMesosPacket : public OutPacket {
public:
    DropMesosPacket(int32_t amount) : OutPacket(OutPacket::Opcode::DROP_MESOS) {
        Sound(Sound::Name::DROP).play();
        write_int(0);
        write_int(amount);
    }
};
// Pet command typed in chat (interact index from Item/Pet/<id>.img/interact)
// Opcode: PET_COMMAND(169)
class PetCommandPacket : public OutPacket {
public:
    PetCommandPacket(int32_t pet_uniqueid, uint8_t command) :
        OutPacket(OutPacket::Opcode::PET_COMMAND) {
        write_int(pet_uniqueid);
        write_int(0);
        write_byte(0);
        write_byte(command);
    }
};
}  // namespace ms