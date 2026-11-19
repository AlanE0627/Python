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
#include "CashShopHandlers.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/Window.h"
#include "Helpers/CashShopParser.h"
#include "../../Timer.h"
#include "../../IO/UITypes/UICashShop.h"

namespace ms {
void SetCashShopHandler::handle(InPacket &recv) const {
    CashShopParser::parseCharacterInfo(recv);

    recv.skip_byte();    // Not MTS
    recv.skip_string();  // account_name
    recv.skip_int();

    int16_t specialcashitem_size = recv.read_short();

    for (size_t i = 0; i < specialcashitem_size; i++) {
        recv.skip_int();   // sn
        recv.skip_int();   // mod
        recv.skip_byte();  // info
    }

    recv.skip(121);

    for (size_t cat = 1; cat <= 8; cat++) {
        for (size_t gender = 0; gender < 2; gender++) {
            for (size_t in = 0; in < 5; in++) {
                recv.skip_int();  // category
                recv.skip_int();  // gender
                recv.skip_int();  // commoditysn
            }
        }
    }

    recv.skip_int();
    recv.skip_short();
    recv.skip_byte();
    recv.skip_int();

    transition();

    UI::get().change_state(UI::State::CASHSHOP);
}

void SetCashShopHandler::transition() const {
    // maple-offline: the v83 cash shop is 800x600, drawn centred in the
    // current view, so the view size stays as it is

    float fadestep = 0.025f;

    Window::get().fadeout(fadestep, []() {
        GraphicsGL::get().clear();

        Stage::get().load(-1, 0);

        UI::get().enable();
        Timer::get().start();
        GraphicsGL::get().unlock();
    });

    GraphicsGL::get().lock();
    Stage::get().clear();
    Timer::get().start();
}

namespace {
UICashShop::LockerItem read_cash_item(InPacket &recv) {
    UICashShop::LockerItem item{};

    item.cashid = recv.read_long();
    recv.skip_int();  // account id
    recv.skip_int();
    item.itemid = recv.read_int();
    item.sn = recv.read_int();
    item.count = recv.read_short();
    recv.skip(13);    // gift from
    recv.skip_long(); // expiration
    recv.skip_long();

    return item;
}
}  // namespace

void CashShopOperationHandler::handle(InPacket &recv) const {
    auto cashshop = UI::get().get_element<UICashShop>();

    if (!cashshop) {
        return;
    }

    uint8_t mode = recv.read_ubyte();

    switch (mode) {
        case 0x4B: {  // cash locker list
            int16_t count = recv.read_short();
            std::vector<UICashShop::LockerItem> items;

            for (int16_t i = 0; i < count; i++) {
                items.push_back(read_cash_item(recv));
            }

            cashshop->get().set_locker(std::move(items));
            break;
        }
        case 0x57:  // bought one item
            cashshop->get().add_locker(read_cash_item(recv));
            break;
        case 0x68:  // moved to the item inventory
            cashshop->get().taken();
            break;
        case 0x5C:  // failure
            cashshop->get().show_message("구매 실패 (코드 " + std::to_string(recv.length() > 0 ? recv.read_ubyte() : 0) + ")");
            break;
        default: break;
    }
}

void QueryCashResultHandler::handle(InPacket &recv) const {
    auto cashshop = UI::get().get_element<UICashShop>();

    if (!cashshop) {
        return;
    }

    int32_t credit = recv.read_int();
    int32_t points = recv.read_int();
    int32_t prepaid = recv.read_int();

    cashshop->get().set_cash(credit, points, prepaid);
}
}  // namespace ms