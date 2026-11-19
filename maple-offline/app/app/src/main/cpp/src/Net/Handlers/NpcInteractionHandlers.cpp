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
#include "NpcInteractionHandlers.h"
#include <vector>
#include "Helpers/ItemParser.h"
#include "../../IO/UITypes/UITrunk.h"

#include "../../IO/UITypes/UINotice.h"

#include "../IO/UI.h"
#include "../IO/UITypes/UINpcTalk.h"
#include "../IO/UITypes/UIShop.h"

namespace ms {
void NpcDialogueHandler::handle(InPacket &recv) const {
    recv.skip(1);

    int32_t npcid = recv.read_int();
    int8_t msgtype = recv.read_byte();  // 0 - textonly, 1 - yes/no, 4 -
                                        // selection, 12 - accept/decline
    int8_t speaker = recv.read_byte();
    std::string text = recv.read_string();

    uint8_t style_b0 = 0;
    uint8_t style_b1 = 0;

    std::string input_default;

    if (recv.length() > 0) {
        if (msgtype == 2) {
            // getText: default text, then max / min length
            input_default = recv.read_string();
        } else if (msgtype == 3 && recv.length() >= 4) {
            // getNumber: default, min, max
            input_default = std::to_string(recv.read_int());
        } else if (msgtype == 0) {
            // {b0, b1}
            // {0, 1} = next
            // {1, 0} = prev
            // {1, 1} = nextPrev
            // {0, 0} = ok
            style_b0 = recv.read_ubyte();
            style_b1 = recv.read_ubyte();
        }
    }

    UI::get().emplace<UINpcTalk>();
    UI::get().enable();

    if (auto npctalk = UI::get().get_element<UINpcTalk>()) {
        npctalk->get()
            .change_text(npcid, msgtype, style_b0, style_b1, speaker, text);

        if (msgtype == 2 || msgtype == 3) {
            npctalk->get().set_input_default(input_default);
        }
    }
}

void OpenNpcShopHandler::handle(InPacket &recv) const {
    int32_t npcid = recv.read_int();
    auto oshop = UI::get().get_element<UIShop>();

    if (!oshop) {
        return;
    }

    UIShop &shop = *oshop;

    shop.reset(npcid);

    int16_t size = recv.read_short();

    for (int i = 0; i < size; i++) {
        int32_t itemid = recv.read_int();
        int32_t price = recv.read_int();
        int32_t pitch = recv.read_int();
        int32_t time = recv.read_int();

        recv.skip(4);

        bool norecharge = recv.read_short() == 1;

        if (norecharge) {
            int16_t buyable = recv.read_short();

            shop.add_item(itemid, price, pitch, time, buyable);
        } else {
            recv.skip(4);

            int16_t rechargeprice = recv.read_short();
            int16_t slotmax = recv.read_short();

            shop.add_rechargable(itemid,
                                 price,
                                 pitch,
                                 time,
                                 rechargeprice,
                                 slotmax);
        }
    }
}
void ConfirmShopTransactionHandler::handle(InPacket &recv) const {
    int8_t code = recv.read_byte();

    const char *msg = nullptr;

    switch (code) {
        case 1: msg = "품절된 아이템입니다."; break;
        case 2: msg = "메소가 부족합니다."; break;
        case 3: msg = "인벤토리에 빈 칸이 있는지 확인해 주세요."; break;
        default: break;
    }

    if (msg) {
        UI::get().emplace<UIOk>(msg, [](bool) {});
    }

    UI::get().enable();
}
namespace {
std::vector<UITrunk::Entry> read_storage_items(InPacket &recv) {
    std::vector<UITrunk::Entry> items;
    uint8_t count = static_cast<uint8_t>(recv.read_byte());
    Inventory scratch;
    int16_t next[InventoryType::Id::LENGTH] = {};

    for (uint8_t i = 0; i < count; i++) {
        // peek the item id (after the 1-byte kind) to know its tab
        int8_t kind = recv.inspect_byte();
        (void) kind;
        size_t id_pos = 1;
        int32_t itemid = recv.inspect_int_at(id_pos);
        InventoryType::Id type = InventoryType::by_item_id(itemid);
        int16_t slot = ++next[type];

        ItemParser::parse_item(recv, type, slot, scratch);
        items.push_back({itemid, scratch.get_item_count(type, slot), type});
    }

    return items;
}
}  // namespace

void StorageHandler::handle(InPacket &recv) const {
    int8_t mode = recv.read_byte();

    switch (mode) {
        case 0x16: {  // open
            int32_t npcid = recv.read_int();
            uint8_t slots = static_cast<uint8_t>(recv.read_byte());
            recv.read_short();
            recv.read_short();
            recv.read_int();
            int32_t meso = recv.read_int();
            recv.read_short();
            std::vector<UITrunk::Entry> items = read_storage_items(recv);

            UI::get().emplace<UITrunk>(npcid, slots, meso, std::move(items));
            break;
        }
        case 0x09:    // taken out
        case 0x0D:    // stored
        case 0x0F: {  // arranged
            uint8_t slots = static_cast<uint8_t>(recv.read_byte());
            InventoryType::Id type = InventoryType::Id::NONE;

            if (mode == 0x0F) {
                recv.read_byte();
                recv.skip(10);
            } else {
                // tab as a bit field: 2 << type
                int16_t bits = recv.read_short();

                for (int t = 1; t <= 5; t++) {
                    if (bits == (2 << t)) {
                        type = static_cast<InventoryType::Id>(t);
                    }
                }

                recv.read_short();
                recv.read_int();
            }

            std::vector<UITrunk::Entry> items = read_storage_items(recv);

            if (auto trunk = UI::get().get_element<UITrunk>()) {
                trunk->get().set_items(slots, type, std::move(items));
            }

            break;
        }
        case 0x13: {  // mesos
            recv.read_byte();
            recv.read_short();
            recv.read_short();
            recv.read_int();
            int32_t meso = recv.read_int();

            if (auto trunk = UI::get().get_element<UITrunk>()) {
                trunk->get().set_meso(meso);
            }

            break;
        }
        case 0x0A: UI::get().emplace<UIOk>("인벤토리가 가득 찼습니다.", [](bool) {}); break;
        case 0x0B: UI::get().emplace<UIOk>("메소가 부족합니다.", [](bool) {}); break;
        case 0x0C: UI::get().emplace<UIOk>("이미 가지고 있는 고유 아이템입니다.", [](bool) {}); break;
        case 0x11: UI::get().emplace<UIOk>("창고가 가득 찼습니다.", [](bool) {}); break;
        default: break;
    }

    UI::get().enable();
}
}  // namespace ms
