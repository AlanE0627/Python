#include "FieldObjects.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <nlnx/nx.hpp>
#include <string>

#include "../Net/OutPacket.h"

namespace ms {
namespace {
constexpr int32_t NO_MAP = 999999999;
constexpr int32_t TOWN_DOOR_KEY = -1;

nl::node skill_node(int32_t skill) {
    std::string sid = std::to_string(skill);

    if (sid.size() < 7) {
        sid.insert(0, 7 - sid.size(), '0');
    }

    return nl::nx::skill[sid.substr(0, sid.size() - 4) + ".img"]["skill"][sid];
}

// USE_DOOR(133): owner id, 1 = from town to the door's map, 0 = to town
class UseDoorPacket : public OutPacket {
public:
    UseDoorPacket(int32_t owner, bool from_town) : OutPacket(133) {
        write_int(owner);
        write_byte(from_town ? 1 : 0);
    }
};
}  // namespace

void FieldObjects::add_door(int32_t owner, Point<int16_t> pos) {
    doors_[owner] = Door{pos, Animation(skill_node(2311002)["cDoor"]), false};
}

void FieldObjects::remove_door(int32_t owner) {
    doors_.erase(owner);
}

void FieldObjects::set_town_door(int32_t owner, int32_t townid, int32_t targetid, Point<int16_t> pos) {
    town_owner_ = owner;

    if (townid == NO_MAP || targetid == NO_MAP) {
        doors_.erase(TOWN_DOOR_KEY);
        return;
    }

    doors_[TOWN_DOOR_KEY] = Door{pos, Animation(skill_node(2311002)["mDoor"]), true};
}

void FieldObjects::add_mist(int32_t oid, int32_t skill, Rectangle<int16_t> box) {
    nl::node src = skill_node(skill);
    nl::node anim = src["tile"]["0"];

    if (anim.size() == 0) {
        anim = src["tile"];
    }

    mists_[oid] = Mist{box, Animation(anim)};
}

void FieldObjects::remove_mist(int32_t oid) {
    mists_.erase(oid);
}

void FieldObjects::clear() {
    doors_.clear();
    mists_.clear();
}

void FieldObjects::update() {
    for (auto &[owner, door] : doors_) {
        door.anim.update();
    }

    for (auto &[oid, mist] : mists_) {
        mist.anim.update();
    }
}

void FieldObjects::draw(double viewx, double viewy, float alpha) const {
    auto screen = [&](Point<int16_t> p) {
        return Point<int16_t>(static_cast<int16_t>(std::round(p.x() + viewx)),
                              static_cast<int16_t>(std::round(p.y() + viewy)));
    };

    for (const auto &[oid, mist] : mists_) {
        // tile the mist animation across its box, along the bottom edge
        int16_t width = mist.box.right() - mist.box.left();
        int16_t step = 100;

        for (int16_t x = 0; x <= width; x = static_cast<int16_t>(x + step)) {
            Point<int16_t> p(static_cast<int16_t>(mist.box.left() + x), mist.box.bottom());
            mist.anim.draw(DrawArgument(screen(p)), alpha);
        }
    }

    for (const auto &[owner, door] : doors_) {
        door.anim.draw(DrawArgument(screen(door.pos)), alpha);
    }
}

void FieldObjects::place_town_door(Point<int16_t> pos) {
    auto it = doors_.find(TOWN_DOOR_KEY);

    if (it != doors_.end()) {
        it->second.pos = pos;
    }
}

bool FieldObjects::town_door(Point<int16_t> &pos) const {
    auto it = doors_.find(TOWN_DOOR_KEY);

    if (it == doors_.end()) {
        return false;
    }

    pos = it->second.pos;
    return true;
}

bool FieldObjects::try_enter(Point<int16_t> playerpos, int32_t mapid) {
    for (const auto &[owner, door] : doors_) {
        if (std::abs(playerpos.x() - door.pos.x()) < 40 && std::abs(playerpos.y() - door.pos.y()) < 60) {
            // the town door is the player's own (single player)
            int32_t id = door.town ? town_owner_ : owner;
            UseDoorPacket(id, door.town).dispatch();
            return true;
        }
    }

    return false;
}
}  // namespace ms
