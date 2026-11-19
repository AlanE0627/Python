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
#include "Combat.h"

#include <nlnx/nx.hpp>
#include "../Stage.h"

#include "../../Data/SkillData.h"

#include <cstdio>
#include <cstdlib>

#include "../../Character/SkillId.h"
#include "../../IO/Messages.h"
#include "../../Net/Packets/AttackAndSkillPackets.h"
#include "../../Net/Packets/GameplayPackets.h"

namespace ms {
namespace {
auto fn_attack = []<typename... T>(T && ...args) {
    AttackPacket(std::forward<T>(args)...).dispatch();
};
auto fn_damage_reactor = []<typename... T>(T && ...args) {
    DamageReactorPacket(std::forward<T>(args)...).dispatch();
};
auto fn_use_skill = []<typename... T>(T && ...args) {
    UseSkillPacket(std::forward<T>(args)...).dispatch();
};
}  // namespace

Combat::Combat(Player &player,
               MapChars &chars,
               MapMobs &mobs,
               MapReactors &reactors) :
    player_(player),
    chars_(chars),
    mobs_(mobs),
    reactors_(reactors),
    attack_results_([&](const AttackResult &attack) { apply_attack(attack); }),
    bullet_effects_(
        [&](const BulletEffect &effect) { apply_bullet_effect(effect); }),
    damage_effects_(
        [&](const DamageEffect &effect) { apply_damage_effect(effect); }) {}

void Combat::draw(double viewx, double viewy, float alpha) const {
    for (const auto &bullet_eff : bullets_) {
        bullet_eff.bullet.draw(viewx, viewy, alpha);
    }

    for (const auto &dmg_eff : damage_numbers_) {
        dmg_eff.draw(viewx, viewy, alpha);
    }
}

void Combat::update() {
    attack_results_.update();
    bullet_effects_.update();
    damage_effects_.update();

    bullets_.remove_if([&](BulletEffect &mb) {
        int32_t target_oid = mb.damageeffect.target_oid;

        if (mobs_.contains(target_oid)) {
            mb.target = mobs_.get_mob_head_position(target_oid);
            bool apply = mb.bullet.update(mb.target);

            if (apply) {
                apply_damage_effect(mb.damageeffect);
            }

            return apply;
        }
        return mb.bullet.update(mb.target);
    });

    damage_numbers_.remove_if([](DamageNumber &dn) { return dn.update(); });
}

void Combat::use_move(int32_t move_id) {
    // Using the riding skill again while mounted gets off the mount
    if (player_.get_look().get_mount() != 0
        && player_.get_buff(Buffstat::Id::MONSTER_RIDING).skillid == move_id) {
        CancelBuffPacket(move_id).dispatch();
        return;
    }

    if (!player_.can_attack()) {
#ifdef MS_DESKTOP_DEBUG
        fprintf(stderr, "[dbg] use_move %d: cannot attack now (attacking %d climb %d sit %d weapon %d)\n", move_id,
                (int) player_.is_attacking(), (int) player_.is_climbing(), (int) player_.is_sitting(),
                (int) player_.get_look().get_equips().has_weapon());
#endif
        return;
    }

    const SpecialMove &move = get_move(move_id);
    SpecialMove::ForbidReason reason = player_.can_use(move);
    Weapon::Type weapontype = player_.get_stats().get_weapontype();
#ifdef MS_DESKTOP_DEBUG
    fprintf(stderr, "[dbg] use_move %d reason %d\n", move_id, (int) reason);
#endif

    if (reason == SpecialMove::ForbidReason::FBR_NONE) {
        apply_move(move);
    } else {
        ForbidSkillMessage(reason, weapontype).drop();
    }
}

void Combat::apply_move(const SpecialMove &move) {
    if (move.is_attack()) {
        Attack attack = player_.prepare_attack(move.is_skill());

        // maple-offline: spells are magic attacks with any weapon; decide it
        // before the action is chosen so the cast animation is used
        if (move.is_skill()) {
            int32_t level = player_.get_skilllevel(move.get_id());

            if (level > 0 && SkillData::get(move.get_id()).get_stats(level).matk > 0) {
                attack.type = Attack::Type::MAGIC;
            }
        }

        // Hit through the ranged-attack path on the server even with melee
        // weapons: Energy Orb, Spark, Shark Wave, Taunt, Aran combo skills
        switch (move.get_id()) {
            case 5121002:
            case 15111007:
            case 4121003:
            case 4221003:
            case 21100004:
            case 21110004:
            case 21120006: attack.type = Attack::Type::RANGED; break;
        }

        move.apply_useeffects(player_);
        move.apply_actions(player_, attack.type);

        player_.set_afterimage(move.get_id());

        move.apply_stats(player_, attack);

        Point<int16_t> origin = attack.origin;
        Rectangle<int16_t> range = attack.range;
        auto hrange = static_cast<int16_t>(range.left() * attack.hrange);

        if (attack.toleft) {
            range = { static_cast<int16_t>(origin.x() + hrange),
                      static_cast<int16_t>(origin.x() + range.right()),
                              static_cast<int16_t>(origin.y() + range.top()),
                                      static_cast<int16_t>(origin.y() + range.bottom()) };
        } else {
            range = { static_cast<int16_t>(origin.x() - range.right()),
                    static_cast<int16_t>(origin.x() - hrange),
                            static_cast<int16_t>(origin.y() + range.top()),
                                    static_cast<int16_t>(origin.y() + range.bottom()) };
        }

        // This approach should also make it easier to implement PvP
        uint8_t mobcount = attack.mobcount;
        AttackResult result = attack;

        if (move.get_id() == 4211006) {
            meso_explosion(attack, range, origin);
            return;
        }

        MapObjects *mob_objs = mobs_.get_mobs();
        MapObjects *reactor_objs = reactors_.get_reactors();

        std::vector<int32_t> mob_targets =
            find_closest(mob_objs, range, origin, mobcount, true);
        std::vector<int32_t> reactor_targets =
            find_closest(reactor_objs, range, origin, mobcount, false);

        mobs_.send_attack(result, attack, mob_targets, mobcount);
        if (std::getenv("MS_DEBUG_ATK")) {
            std::cout << "[atkdmg] min " << attack.mindamage << " max " << attack.maxdamage << " lines";
            for (auto &dl : result.damage_lines) for (auto &d : dl.second) std::cout << " " << d.first;
            std::cout << std::endl;
        }
        result.attacker = player_.get_oid();
        result.position = mob_targets.empty()
                              ? player_.get_position()
                              : mobs_.get_mob_head_position(mob_targets.front());

        // Aran: every mob hit adds one to the server-side combo count
        if (player_.get_stats().get_stat(MapleStat::Id::JOB) / 100 == 21) {
            for (size_t i = 0; i < result.damage_lines.size(); i++) {
                OutPacket(OutPacket::Opcode::ARAN_COMBO_COUNTER).dispatch();
            }
        }
        extract_effects(player_, move, result);

        apply_use_movement(move);
        apply_result_movement(move, result);

        fn_attack(result);

        if (!reactor_targets.empty()) {
            if (auto reactor =
                    reactor_objs->get<Reactor>(reactor_targets.at(0))) {
                fn_damage_reactor(reactor->get().get_oid(),
                                  player_.get_position(),
                                  0,
                                  0);
            }
        }
    } else {
        move.apply_useeffects(player_);
        move.apply_actions(player_, Attack::Type::MAGIC);

        int32_t moveid = move.get_id();
        int32_t level = player_.get_skills().get_level(moveid);

        // Monster Magnet pulls the closest mobs in front of the player
        if (moveid == 1121001 || moveid == 1221001 || moveid == 1321001) {
            const SkillData::Stats &st = SkillData::get(moveid).get_stats(level);
            Point<int16_t> origin = player_.get_position();
            bool left = !player_.getflip();
            int16_t reach = st.range.empty() ? 350 : st.range.width();
            Rectangle<int16_t> range =
                left ? Rectangle<int16_t>(origin.x() - reach, origin.x(),
                                          origin.y() - 150, origin.y() + 30)
                     : Rectangle<int16_t>(origin.x(), origin.x() + reach,
                                          origin.y() - 150, origin.y() + 30);
            uint8_t count = st.mobcount ? st.mobcount : 5;

            MonsterMagnetPacket(moveid,
                                level,
                                find_closest(mobs_.get_mobs(), range, origin, count, true),
                                left)
                .dispatch();
            return;
        }

        // maple-offline: summons and Mystic Door need the cast position
        std::string sid = std::to_string(moveid);

        if (sid.size() < 7) {
            sid.insert(0, 7 - sid.size(), '0');
        }

        nl::node src = nl::nx::skill[sid.substr(0, sid.size() - 4) + ".img"]["skill"][sid];
        bool placed = src["summon"].size() > 0 || moveid == 2311002;
#ifdef MS_DESKTOP_DEBUG
        fprintf(stderr, "[dbg] skill %d placed %d\n", moveid, (int) placed);
#endif

        if (placed) {
            fn_use_skill(moveid, level, player_.get_position(), player_.getflip());
        } else {
            fn_use_skill(moveid, level);
        }
    }
}

// Meso Explosion: every meso on the ground in range explodes and hits one
// of the mobs in range (assigned in turn). Per-drop damage is an
// interpretation: no verified v83 formula was found, so it scales the
// skill's x with the drop's meso amount: high = x * (0.5 + 0.5 * m / (m + 250)),
// low = high / 2.
void Combat::meso_explosion(const Attack &attack,
                            Rectangle<int16_t> range,
                            Point<int16_t> origin) {
    int32_t level = player_.get_skilllevel(4211006);
    int32_t x = nl::nx::skill["421.img"]["skill"]["4211006"]["level"]
                             [std::to_string(level)]["x"]
                                 .get_integer(1000);
    size_t maxmesos = attack.hitcount > 0 ? attack.hitcount : 15;

    auto mesos = Stage::get().get_drops().find_mesos(range, origin, maxmesos);
    std::vector<int32_t> mobs =
        find_closest(mobs_.get_mobs(), range, origin, attack.mobcount, true);

    std::vector<MesoExplosionPacket::Target> targets;

    for (int32_t oid : mobs) {
        targets.push_back({ oid, mobs_.get_mob_head_position(oid), {} });
    }

    std::vector<int32_t> meso_oids;

    for (size_t i = 0; i < mesos.size(); i++) {
        meso_oids.push_back(mesos[i].first);

        if (targets.empty()) {
            continue;
        }

        double m = mesos[i].second;
        double high = x * (0.5 + 0.5 * m / (m + 250.0));
        double low = high * 0.5;
        auto damage = static_cast<int32_t>(
            low + (high - low) * (std::rand() % 1000) / 1000.0);

        targets[i % targets.size()].damage.push_back(damage);
    }

    MesoExplosionPacket(targets, meso_oids, attack.toleft, attack.stance)
        .dispatch();

    for (const auto &target : targets) {
        for (int32_t damage : target.damage) {
            show_summon_damage(target.oid, damage);
        }
    }
}

std::vector<int32_t> Combat::find_closest(MapObjects *objs,
                                          Rectangle<int16_t> range,
                                          Point<int16_t> origin,
                                          uint8_t objcount,
                                          bool use_mobs) const {
    std::multimap<uint16_t, int32_t> distances;

    for (auto &mmo : *objs) {
        if (use_mobs) {
            const Mob *mob = static_cast<const Mob *>(mmo.second.get());

            if (mob && std::getenv("MS_DEBUG_ATK")) {
                auto mp = mob->get_position();
                std::cout << "[atk] range " << range.left() << "," << range.right() << "," << range.top() << "," << range.bottom()
                          << " origin " << origin.x() << "," << origin.y() << " mob " << mob->get_oid() << " pos " << mp.x() << "," << mp.y()
                          << " alive " << mob->is_alive() << " cx " << mob->debug_center_x() << " inrange " << mob->is_in_range(range) << std::endl;
            }
            if (mob && mob->is_alive() && mob->is_in_range(range)) {
                int32_t oid = mob->get_oid();
                uint16_t distance = mob->get_position().distance(origin);
                distances.emplace(distance, oid);
            }
        } else {
            // Assume Reactor
            const Reactor *reactor =
                static_cast<const Reactor *>(mmo.second.get());

            if (reactor && reactor->is_hittable()
                && reactor->is_in_range(range)) {
                int32_t oid = reactor->get_oid();
                uint16_t distance = reactor->get_position().distance(origin);
                distances.emplace(distance, oid);
            }
        }
    }

    std::vector<int32_t> targets;

    for (auto &iter : distances) {
        if (targets.size() >= objcount) {
            break;
        }

        targets.push_back(iter.second);
    }

    return targets;
}

void Combat::apply_use_movement(const SpecialMove &move) {
    switch (move.get_id()) {
        case SkillId::Id::TELEPORT_FP:
        case SkillId::Id::IL_TELEPORT:
        case SkillId::Id::PRIEST_TELEPORT:
        case SkillId::Id::FLASH_JUMP:
        default: break;
    }
}

void Combat::apply_result_movement(const SpecialMove &move,
                                   const AttackResult &result) {
    switch (move.get_id()) {
        case SkillId::Id::RUSH_HERO:
        case SkillId::Id::RUSH_PALADIN:
        case SkillId::Id::RUSH_DK: apply_rush(result); break;
        default: break;
    }
}

void Combat::apply_rush(const AttackResult &result) {
    if (result.mobcount == 0) {
        return;
    }

    Point<int16_t> mob_position = mobs_.get_mob_position(result.last_oid);
    int16_t targetx = mob_position.x();
    player_.rush(targetx);
}

void Combat::apply_bullet_effect(const BulletEffect &effect) {
    bullets_.push_back(effect);

    if (bullets_.back().bullet.settarget(effect.target)) {
        apply_damage_effect(effect.damageeffect);
        bullets_.pop_back();
    }
}

void Combat::apply_damage_effect(const DamageEffect &effect) {
    Point<int16_t> head_position =
        mobs_.get_mob_head_position(effect.target_oid);
    damage_numbers_.push_back(effect.number);
    damage_numbers_.back().set_x(head_position.x());

    const SpecialMove &move = get_move(effect.move_id);
    mobs_.apply_damage(effect.target_oid,
                       effect.damage,
                       effect.toleft,
                       effect.user,
                       move);
}

void Combat::show_summon_damage(int32_t mob_oid, int32_t damage) {
    if (!mobs_.contains(mob_oid)) {
        return;
    }

    Point<int16_t> head = mobs_.get_mob_head_position(mob_oid);
    damage_numbers_.emplace_back(DamageNumber::Type::NORMAL, damage, head.y());
    damage_numbers_.back().set_x(head.x());
}

void Combat::push_attack(const AttackResult &attack) {
    attack_results_.push(400, attack);
}

void Combat::apply_attack(const AttackResult &attack) {
    if (auto ouser = chars_.get_char(attack.attacker)) {
        OtherChar &user = *ouser;
        user.update_skill(attack.skill, attack.level);
        user.update_speed(attack.speed);

        const SpecialMove &move = get_move(attack.skill);
        move.apply_useeffects(user);

        if (Stance::Id stance = Stance::by_id(attack.stance)) {
            user.attack(stance);
        } else {
            move.apply_actions(user, attack.type);
        }

        user.set_afterimage(attack.skill);

        extract_effects(user, move, attack);
    }
}

void Combat::extract_effects(const Char &user,
                             const SpecialMove &move,
                             const AttackResult &result) {
    AttackUser attack_user = { user.get_skilllevel(move.get_id()),
                               user.get_level(),
                               user.is_twohanded(),
                               !result.toleft };

    if (result.bullet) {
        Bullet bullet { move.get_bullet(user, result.bullet),
                        user.get_position(),
                        result.toleft };

        for (const auto &[oid, vec] : result.damage_lines) {
            if (mobs_.contains(oid)) {
                std::vector<DamageNumber> numbers = place_numbers(oid, vec);
                Point<int16_t> head = mobs_.get_mob_head_position(oid);

                size_t i = 0;

                for (auto &number : numbers) {
                    DamageEffect effect {
                        attack_user,   number, vec[i].first,
                        result.toleft, oid,    move.get_id()
                    };

                    bullet_effects_.emplace(user.get_attackdelay(i),
                                            std::move(effect),
                                            bullet,
                                            head);
                    i++;
                }
            }
        }

        if (result.damage_lines.empty()) {
            int16_t xshift = result.toleft ? -400 : 400;
            Point<int16_t> target =
                user.get_position() + Point<int16_t>(xshift, -26);

            for (int i = 0; i < result.hitcount; i++) {
                DamageEffect effect { attack_user, {}, 0, false, 0, 0 };
                bullet_effects_.emplace(user.get_attackdelay(i),
                                        std::move(effect),
                                        bullet,
                                        target);
            }
        }
    } else {
        for (const auto &[oid, vec] : result.damage_lines) {
            if (mobs_.contains(oid)) {
                std::vector<DamageNumber> numbers = place_numbers(oid, vec);

                size_t i = 0;

                for (auto &number : numbers) {
                    damage_effects_.emplace(user.get_attackdelay(i),
                                            attack_user,
                                            number,
                                            vec[i].first,
                                            result.toleft,
                                            oid,
                                            move.get_id());

                    i++;
                }
            }
        }
    }
}

std::vector<DamageNumber> Combat::place_numbers(
    int32_t oid,
    const std::vector<std::pair<int32_t, bool>> &damage_lines) {
    std::vector<DamageNumber> numbers;
    int16_t head = mobs_.get_mob_head_position(oid).y();

    for (const auto &[amount, is_critical] : damage_lines) {
        DamageNumber::Type type = is_critical ? DamageNumber::Type::CRITICAL
                                              : DamageNumber::Type::NORMAL;
        numbers.emplace_back(type, amount, head);

        head -= DamageNumber::row_height(is_critical);
    }

    return numbers;
}

void Combat::show_buff(int32_t cid, int32_t skillid, int8_t level) {
    if (auto ouser = chars_.get_char(cid)) {
        OtherChar &user = *ouser;
        user.update_skill(skillid, level);

        const SpecialMove &move = get_move(skillid);
        move.apply_useeffects(user);
        move.apply_actions(user, Attack::Type::MAGIC);
    }
}

void Combat::show_player_buff(int32_t skillid) {
    get_move(skillid).apply_useeffects(player_);
}

void Combat::show_affected_by_buff(int32_t cid, int32_t skillid, int8_t level) {
    if (auto ouser = chars_.get_char(cid)) {
        OtherChar &user = *ouser;
        user.update_skill(skillid, level);

        const SpecialMove &move = get_move(skillid);
        move.apply_affected_effects(user);
    }
}

void Combat::show_player_affected_by_buff(int32_t skillid) {
    get_move(skillid).apply_affected_effects(player_);
}

void Combat::give_foreign_buff(int32_t cid, int32_t skillid, int8_t level) {
    if (auto ouser = chars_.get_char(cid)) {
        OtherChar &user = *ouser;
        user.update_skill(skillid, level);

        const SpecialMove &move = get_move(skillid);
        user.add_recurring_effect(
            1,
            nl::nx::skill["MobSkill.img"][skillid]["level"][level]["affected"],
            1);
    }
}

void Combat::show_player_disease(int32_t skillid, int8_t level) {
    player_.add_recurring_effect(
        1,
        nl::nx::skill["MobSkill.img"][skillid]["level"][level]["affected"],
        1);
}

const SpecialMove &Combat::get_move(int32_t move_id) {
    if (move_id == 0) {
        return regular_attack_;
    }

    auto iter = skills_.find(move_id);

    if (iter == skills_.end()) {
        iter = skills_.emplace(move_id, move_id).first;
    }

    return iter->second;
}
}  // namespace ms