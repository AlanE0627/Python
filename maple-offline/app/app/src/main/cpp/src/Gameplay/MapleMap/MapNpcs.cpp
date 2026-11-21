#include <cstdio>
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
#include "MapNpcs.h"

#include <cstdlib>

#include "../Net/Packets/NpcInteractionPackets.h"
#include "Npc.h"
#include "../QuestTalk.h"
#include "UI.h"
#include "UINpcTalk.h"

namespace ms {
namespace {
auto fn_talk_to_npc = []<typename... T>(T && ...args) {
    TalkToNPCPacket(std::forward<T>(args)...).dispatch();
};
}  // namespace

void MapNpcs::draw(Layer::Id layer,
                   double viewx,
                   double viewy,
                   float alpha) const {
    npcs_.draw(layer, viewx, viewy, alpha);
}

void MapNpcs::update(const Physics &physics) {
    for (; !spawns_.empty(); spawns_.pop()) {
        const NpcSpawn &spawn = spawns_.front();

        int32_t oid = spawn.get_oid();
        auto npc = npcs_.get<MapObject>(oid);

        if (npc) {
            npc->get().makeactive();
        } else {
            npcs_.add(spawn.instantiate(physics));
        }
    }

    npcs_.update(physics);
}

void MapNpcs::spawn(NpcSpawn &&spawn) {
    spawns_.emplace(std::move(spawn));
}

void MapNpcs::remove(int32_t oid) {
    if (auto npc = npcs_.get<MapObject>(oid)) {
        npc->get().deactivate();
    }
}

void MapNpcs::clear() {
    npcs_.clear();
}

MapObjects *MapNpcs::get_npcs() {
    return &npcs_;
}

void MapNpcs::talk_to(Npc *npc) {
    // maple-offline: quests at this NPC come first (v83 order)
    if (QuestTalk::open(npc->get_id(), npc->get_oid(), npc->isscripted())) {
        return;
    }

    // the server answers every NPC now (its script, a shop, or the NPC's own
    // default line), so nothing is shown locally here
    fn_talk_to_npc(npc->get_oid());
}

bool MapNpcs::talk_nearest(Point<int16_t> position) {
    Npc *best = nullptr;
    int32_t best_dist = 0;

    for (auto &map_object : npcs_) {
        Npc *npc = static_cast<Npc *>(map_object.second.get());

        if (!npc || !npc->is_active()) {
            continue;
        }

        Point<int16_t> p = npc->get_position();
        int32_t dx = std::abs(p.x() - position.x());
        int32_t dy = std::abs(p.y() - position.y());

        if (dx <= 150 && dy <= 100 && (!best || dx < best_dist)) {
            best = npc;
            best_dist = dx;
        }
    }

    if (best) {
        talk_to(best);
    }

    return best != nullptr;
}

Cursor::State MapNpcs::send_cursor(bool pressed,
                                   Point<int16_t> position,
                                   Point<int16_t> viewpos) {
    for (auto &map_object : npcs_) {
        Npc *npc = static_cast<Npc *>(map_object.second.get());

        if (npc && npc->is_active() && npc->inrange(position, viewpos)) {
            if (pressed) {
                talk_to(npc);
                return Cursor::State::IDLE;
            }
            return Cursor::State::CAN_CLICK;
        }
    }

    return pressed ? Cursor::State::CLICKING : Cursor::State::IDLE;
}
}  // namespace ms