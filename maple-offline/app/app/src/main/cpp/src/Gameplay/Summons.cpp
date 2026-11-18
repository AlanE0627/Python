#include "Summons.h"

#include <algorithm>
#include <cmath>
#include <nlnx/nx.hpp>
#include <vector>

#include "../Constants.h"
#include "../Net/OutPacket.h"
#include "../Util/Randomizer.h"
#include "MapleMap/Mob.h"
#include "Stage.h"

namespace ms {
namespace {
nl::node skill_node(int32_t skill) {
    std::string sid = std::to_string(skill);

    if (sid.size() < 7) {
        sid.insert(0, 7 - sid.size(), '0');
    }

    return nl::nx::skill[sid.substr(0, sid.size() - 4) + ".img"]["skill"][sid];
}

// SUMMON_ATTACK(176): oid, int 0, direction, count, 8 bytes, then per
// target oid, 8 bytes, mob position twice, delay, damage
class SummonAttackPacket : public OutPacket {
public:
    struct Hit {
        int32_t oid;
        Point<int16_t> pos;
        int32_t damage;
    };

    SummonAttackPacket(int32_t summon_oid, bool toleft, Point<int16_t> summon_pos,
                       const std::vector<Hit> &hits) :
        OutPacket(OutPacket::Opcode::SUMMON_ATTACK) {
        write_int(summon_oid);
        write_int(0);
        write_byte(toleft ? 0 : 1);
        write_byte(static_cast<int8_t>(hits.size()));
        write_point(hits.empty() ? summon_pos : hits.front().pos);
        write_point(summon_pos);

        for (const Hit &hit : hits) {
            write_int(hit.oid);
            write_int(0);
            write_int(0);
            write_point(hit.pos);
            write_point(hit.pos);
            write_short(0);
            write_int(hit.damage);
        }
    }
};
}  // namespace

void Summons::add(int32_t owner, int32_t oid, int32_t skill, int8_t level, Point<int16_t> pos,
                  int8_t movetype, bool can_attack) {
    nl::node src = skill_node(skill)["summon"];

    Summon s{owner, oid, skill, level, pos, movetype, can_attack, false, false,
             1000 / Constants::TIMESTEP, Animation(), Animation(), nullptr};

    nl::node stand = src["stand"];

    if (stand.size() == 0) {
        stand = src["fly"];
    }

    s.stand = Animation(stand);
    s.attack = Animation(src["attack1"]);

    auto [it, inserted] = summons_.insert_or_assign(oid, std::move(s));
    it->second.current = &it->second.stand;
}

void Summons::remove(int32_t oid) {
    summons_.erase(oid);
}

void Summons::clear() {
    summons_.clear();
}

void Summons::try_attack(Summon &s) {
    nl::node info = skill_node(s.skill)["summon"]["attack1"]["info"];
    nl::node range = info["range"];
    int32_t r = range["r"];
    Point<int16_t> lt = range["lt"];
    Point<int16_t> rb = range["rb"];

    Rectangle<int16_t> area;

    if (r > 0) {
        area = Rectangle<int16_t>(static_cast<int16_t>(s.pos.x() - r), static_cast<int16_t>(s.pos.x() + r),
                                  static_cast<int16_t>(s.pos.y() - r), static_cast<int16_t>(s.pos.y() + r));
    } else if (lt != Point<int16_t>() || rb != Point<int16_t>()) {
        area = Rectangle<int16_t>(s.pos + lt, s.pos + rb);
    } else {
        area = Rectangle<int16_t>(static_cast<int16_t>(s.pos.x() - 200), static_cast<int16_t>(s.pos.x() + 200),
                                  static_cast<int16_t>(s.pos.y() - 120), static_cast<int16_t>(s.pos.y() + 60));
    }

    nl::node level = skill_node(s.skill)["level"][std::to_string(s.level)];
    int32_t mobcount = std::max<int32_t>(1, info["mobCount"].get_integer(level["mobCount"].get_integer(1)));
    int32_t pad = level["pad"];
    int32_t mad = level["mad"];

    Stage &stage = Stage::get();
    Player &player = stage.get_player();
    std::vector<std::pair<int32_t, Point<int16_t>>> targets;

    for (auto &[oid, obj] : *stage.get_mobs().get_mobs()) {
        auto *mob = static_cast<Mob *>(obj.get());

        if (mob && mob->is_alive() && mob->is_in_range(area)) {
            targets.emplace_back(oid, mob->get_position());
        }
    }

    if (targets.empty()) {
        return;
    }

    std::sort(targets.begin(), targets.end(), [&](const auto &a, const auto &b) {
        return std::abs(a.second.x() - s.pos.x()) < std::abs(b.second.x() - s.pos.x());
    });

    if (static_cast<int32_t>(targets.size()) > mobcount) {
        targets.resize(mobcount);
    }

    // mirrors the server's cap (SummonDamageHandler.calcMaxDamage)
    double base = player.get_stats().get_maxdamage();
    double maxdmg = pad > 0 ? base * ((base >= 438 ? 0.054 : 0.077) * pad) : base * 0.05 * mad;
    Randomizer random;
    std::vector<SummonAttackPacket::Hit> hits;

    for (const auto &[oid, pos] : targets) {
        auto dmg = static_cast<int32_t>(maxdmg * random.next_real(0.75, 1.0));
        hits.push_back({oid, pos, std::max(1, dmg)});
        stage.get_combat().show_summon_damage(oid, std::max(1, dmg));
    }

    s.flip = targets.front().second.x() > s.pos.x();
    SummonAttackPacket(s.oid, !s.flip, s.pos, hits).dispatch();

    s.attack.reset();
    s.current = &s.attack;
    s.attacking = true;
}

void Summons::update() {
    Player &player = Stage::get().get_player();
    Point<int16_t> ppos = player.get_position();

    for (auto &[oid, s] : summons_) {
        // following summons hover beside the player (birds above, others
        // at the side); stationary ones (puppet, octopus) stay put
        if (s.movetype != 0) {
            bool left = player.getflip();
            Point<int16_t> target = ppos + Point<int16_t>(left ? 45 : -45, s.movetype == 3 ? -60 : 0);
            s.pos = s.pos + Point<int16_t>(static_cast<int16_t>((target.x() - s.pos.x()) / 8),
                                           static_cast<int16_t>((target.y() - s.pos.y()) / 8));

            if (!s.attacking) {
                s.flip = !left;
            }
        }

        if (s.current && s.current->update() && s.attacking) {
            s.attacking = false;
            s.current = &s.stand;
        }

        if (s.can_attack && s.owner == player.get_oid() && --s.cooldown <= 0) {
            // about one attack every 1.5 s, like the original
            s.cooldown = 1500 / Constants::TIMESTEP;
            try_attack(s);
        }
    }
}

void Summons::draw(double viewx, double viewy, float alpha) const {
    for (const auto &[oid, s] : summons_) {
        if (!s.current) {
            continue;
        }

        Point<int16_t> pos(static_cast<int16_t>(std::round(s.pos.x() + viewx)),
                           static_cast<int16_t>(std::round(s.pos.y() + viewy)));
        s.current->draw(DrawArgument(pos, s.flip), alpha);
    }
}
}  // namespace ms
