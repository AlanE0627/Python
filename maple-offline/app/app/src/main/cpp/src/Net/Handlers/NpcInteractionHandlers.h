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

#include "../PacketHandler.h"

namespace ms {
// Handler for a packet which contains NPC dialog
class NpcDialogueHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Opens an NPC shop defined by the packet's contents
// maple-offline: result of a shop buy/sell (0 ok, 1 sold out, 2 not enough
// mesos, 3 inventory full). Without a handler failures were silent.
class ConfirmShopTransactionHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

class OpenNpcShopHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};
}  // namespace ms