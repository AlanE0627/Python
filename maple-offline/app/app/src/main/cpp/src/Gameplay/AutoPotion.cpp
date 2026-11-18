#include "AutoPotion.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <nlnx/nx.hpp>
#include <string>

#include "../Configuration.h"
#include "../Net/Packets/InventoryPackets.h"
#include "Stage.h"

namespace ms::AutoPotion {
namespace {
// which kind of potion an item is: bit 1 HP, bit 2 MP
int kind_of(int32_t itemid) {
    if (itemid / 10000 != 200) {
        return 0;
    }

    char sid[16];
    std::snprintf(sid, sizeof(sid), "%08d", itemid);
    nl::node spec = nl::nx::item["Consume"]["0200.img"][std::string(sid)]["spec"];
    int kind = 0;

    if (static_cast<int32_t>(spec["hp"]) > 0 || static_cast<int32_t>(spec["hpR"]) > 0) {
        kind |= 1;
    }

    if (static_cast<int32_t>(spec["mp"]) > 0 || static_cast<int32_t>(spec["mpR"]) > 0) {
        kind |= 2;
    }

    return kind;
}

bool drink(int wanted) {
    const Inventory &inv = Stage::get().get_player().get_inventory();
    uint8_t max = inv.get_slotmax(InventoryType::Id::USE);

    for (int16_t slot = 1; slot <= max; slot++) {
        int32_t id = inv.get_item_id(InventoryType::Id::USE, slot);

        if (id != 0 && (kind_of(id) & wanted)) {
            UseItemPacket(slot, id).dispatch();
            return true;
        }
    }

    return false;
}
}  // namespace

void update() {
    using clock = std::chrono::steady_clock;
    static clock::time_point next_hp;
    static clock::time_point next_mp;

    Player &player = Stage::get().get_player();
    const CharStats &stats = player.get_stats();
    int32_t hp = stats.get_stat(MapleStat::Id::HP);
    int32_t mp = stats.get_stat(MapleStat::Id::MP);
    int32_t maxhp = std::max<int32_t>(1, stats.get_total(EquipStat::Id::HP));
    int32_t maxmp = std::max<int32_t>(1, stats.get_total(EquipStat::Id::MP));
    int hp_pct = Setting<AutoHpPercent>::get().load();
    int mp_pct = Setting<AutoMpPercent>::get().load();
    clock::time_point now = clock::now();

    // one potion per 0.6 s at most, as fast as the original pouch
    if (hp > 0 && hp_pct > 0 && hp * 100 < maxhp * hp_pct && now >= next_hp) {
        if (drink(1)) {
            next_hp = now + std::chrono::milliseconds(600);
        }
    }

    if (hp > 0 && mp_pct > 0 && mp * 100 < maxmp * mp_pct && now >= next_mp) {
        if (drink(2)) {
            next_mp = now + std::chrono::milliseconds(600);
        }
    }
}
}  // namespace ms::AutoPotion
