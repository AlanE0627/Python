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

#include <algorithm>
#include <vector>

#include "../Gameplay/Combat/Attack.h"
#include "../OutPacket.h"

namespace ms {
// Notifies the server of an attack. The opcode is determined by the attack
// type. Attack::CLOSE = CLOSE_ATTACK(44) Attack::RANGED = RANGED_ATTACK(45)
// Attack::MAGIC = MAGIC_ATTACK(46)
class AttackPacket : public OutPacket {
public:
    AttackPacket(const AttackResult &attack) :
        OutPacket(opcodefor(attack.type)) {
        skip(1);

        write_byte((attack.mobcount << 4) | attack.hitcount);
        write_int(attack.skill);

        if (attack.charge > 0) {
            write_int(attack.charge);
        }

        skip(8);

        write_byte(attack.display);
        write_byte(attack.toleft);
        write_byte(attack.stance);

        skip(1);

        write_byte(attack.speed);

        if (attack.type == Attack::Type::RANGED) {
            skip(1);
            write_byte(attack.toleft);
            skip(7);

            // Hurricane, Piercing Arrow, Rapid Fire (and the Wind Archer
            // Hurricane) carry 4 more bytes here
            if (attack.skill == 3121004 || attack.skill == 3221001
                || attack.skill == 5221004 || attack.skill == 13111002) {
                skip(4);
            }
        } else {
            skip(4);
        }

        for (const auto &damage_to_mob : attack.damage_lines) {
            write_int(damage_to_mob.first);

            skip(14);

            for (const auto &single_damage : damage_to_mob.second) {
                write_int(single_damage.first);
                // TODO: Add critical here
            }

            if (attack.skill != 5221004) {
                skip(4);
            }
        }
    }

private:
    static OutPacket::Opcode opcodefor(Attack::Type type) {
        switch (type) {
            case Attack::Type::CLOSE: return OutPacket::Opcode::CLOSE_ATTACK;
            case Attack::Type::RANGED: return OutPacket::Opcode::RANGED_ATTACK;
            default: return OutPacket::Opcode::MAGIC_ATTACK;
        }
    }
};

// Tells the server that the player took damage.
// Opcode: TAKE_DAMAGE(48)
class TakeDamagePacket : public OutPacket {
public:
    enum From : int8_t { TOUCH = -1 };

    TakeDamagePacket(int8_t from,
                     uint8_t element,
                     int32_t damage,
                     int32_t mobid,
                     int32_t oid,
                     uint8_t direction) :
        OutPacket(OutPacket::Opcode::TAKE_DAMAGE) {
        write_time();
        write_byte(from);
        write_byte(element);
        write_int(damage);
        write_int(mobid);
        write_int(oid);
        write_byte(direction);
    }

    // From mob attack result.
    TakeDamagePacket(const MobAttackResult &result, From from) :
        TakeDamagePacket(from,
                         0,
                         result.damage,
                         result.mobid,
                         result.oid,
                         result.direction) {}
};

// Meso Explosion (4211006), parsed by the server separately from other
// close-range attacks: per mob oid, positions and damage lines, then the
// exploded meso drops
// Opcode: CLOSE_ATTACK(44)
class MesoExplosionPacket : public OutPacket {
public:
    struct Target {
        int32_t oid;
        Point<int16_t> pos;
        std::vector<int32_t> damage;
    };

    MesoExplosionPacket(const std::vector<Target> &targets,
                        const std::vector<int32_t> &mesos,
                        bool toleft,
                        uint8_t stance) :
        OutPacket(OutPacket::Opcode::CLOSE_ATTACK) {
        skip(1);
        uint8_t lines = 0;

        for (const auto &t : targets) {
            lines = std::max<uint8_t>(lines, static_cast<uint8_t>(t.damage.size()));
        }

        write_byte(static_cast<uint8_t>((targets.size() << 4) | (lines & 0xF)));
        write_int(4211006);
        skip(8);
        write_byte(0);  // display
        write_byte(toleft);
        write_byte(stance);
        skip(6);

        for (const auto &t : targets) {
            write_int(t.oid);
            skip(4);
            write_point(t.pos);
            write_point(t.pos);
            write_byte(static_cast<uint8_t>(t.damage.size()));

            for (int32_t d : t.damage) {
                write_int(d);
            }

            skip(4);
        }

        skip(4);
        write_byte(static_cast<uint8_t>(mesos.size()));

        for (int32_t oid : mesos) {
            write_int(oid);
            skip(1);
        }

        write_short(0);
    }
};

// Monster Magnet (1121001/1221001/1321001): pulled mobs and facing
// Opcode: USE_SKILL(91)
class MonsterMagnetPacket : public OutPacket {
public:
    MonsterMagnetPacket(int32_t skillid,
                        int32_t level,
                        const std::vector<int32_t> &oids,
                        bool left) :
        OutPacket(OutPacket::Opcode::USE_SKILL) {
        write_time();
        write_int(skillid);
        write_byte(static_cast<uint8_t>(level));
        write_int(static_cast<int32_t>(oids.size()));

        for (int32_t oid : oids) {
            write_int(oid);
            write_byte(1);
        }

        write_byte(left ? 1 : 0);
    }
};

// Ends a buff early (Monster Riding toggle, right-click on a buff icon).
// Opcode: CANCEL_BUFF(92)
class CancelBuffPacket : public OutPacket {
public:
    CancelBuffPacket(int32_t skillid) :
        OutPacket(OutPacket::Opcode::CANCEL_BUFF) {
        write_int(skillid);
    }
};

// Packet which notifies the server of a skill usage.
// Opcode: USE_SKILL(91)
class UseSkillPacket : public OutPacket {
public:
    UseSkillPacket(int32_t skillid, int32_t level) :
        OutPacket(OutPacket::Opcode::USE_SKILL) {
        write_time();
        write_int(skillid);
        write_byte(static_cast<uint8_t>(level));

        // If monster magnet : some more bytes

        if (skillid % 10000000 == 1004) {
            skip(2);  // TODO: No idea what this could be
        }

        // TODO: A point (4 bytes) could be added at the end
    }

    // maple-offline: summons and Mystic Door are placed where the player
    // stands; the server only spawns them when a position is sent
    UseSkillPacket(int32_t skillid, int32_t level, Point<int16_t> pos, bool left) :
        UseSkillPacket(skillid, level) {
        write_short(pos.x());
        write_short(pos.y());
        write_byte(left ? 1 : 0);
    }
};
}  // namespace ms