// maple-offline: summons (Silver Hawk, Puppet, Octopus, Summon Dragon,
// Beholder ...). The v83 client draws them and drives their attacks: it
// picks targets in range on a timer and reports the hits with
// SUMMON_ATTACK; the server applies the damage.
#pragma once

#include <cstdint>
#include <map>

#include "../Graphics/Animation.h"
#include "../Template/Point.h"

namespace ms {
class Summons {
public:
    void add(int32_t owner, int32_t oid, int32_t skill, int8_t level, Point<int16_t> pos,
             int8_t movetype, bool can_attack);
    void remove(int32_t oid);
    void clear();

    void update();
    void draw(double viewx, double viewy, float alpha) const;

private:
    struct Summon {
        int32_t owner;
        int32_t oid;
        int32_t skill;
        int8_t level;
        Point<int16_t> pos;
        int8_t movetype;
        bool can_attack;
        bool flip;
        bool attacking;
        int32_t cooldown;   // ticks until the next attack
        Animation stand;
        Animation attack;
        Animation *current;
    };

    void try_attack(Summon &s);

    std::map<int32_t, Summon> summons_;
};
}  // namespace ms
