// maple-offline: Mystic Door (field door + town door) and mists (Poison
// Mist, Smokescreen, Recovery). Both arrive as their own packets and were
// not handled by the client.
#pragma once

#include <cstdint>
#include <map>

#include "../Graphics/Animation.h"
#include "../Template/Point.h"
#include "../Template/Rectangle.h"

namespace ms {
class FieldObjects {
public:
    // SPAWN_DOOR: a door opened on this (hunting) map
    void add_door(int32_t owner, Point<int16_t> pos);
    void remove_door(int32_t owner);

    // SPAWN_PORTAL: the owner's door in town (townid 999999999 removes it)
    void set_town_door(int32_t owner, int32_t townid, int32_t targetid, Point<int16_t> pos);

    void add_mist(int32_t oid, int32_t skill, Rectangle<int16_t> box);
    void remove_mist(int32_t oid);

    void clear();

    void update();
    void draw(double viewx, double viewy, float alpha) const;

    // the town door, if one is open on this map
    bool town_door(Point<int16_t> &pos) const;

    // the town door's spot is only known once the town map is loaded
    void place_town_door(Point<int16_t> pos);

    // Up key near a door: asks the server to warp through it
    bool try_enter(Point<int16_t> playerpos, int32_t mapid);

private:
    struct Door {
        Point<int16_t> pos;
        Animation anim;
        bool town;
    };

    struct Mist {
        Rectangle<int16_t> box;
        Animation anim;
    };

    std::map<int32_t, Door> doors_;   // owner -> door
    std::map<int32_t, Mist> mists_;   // oid -> mist
    int32_t town_owner_ = 0;
};
}  // namespace ms
