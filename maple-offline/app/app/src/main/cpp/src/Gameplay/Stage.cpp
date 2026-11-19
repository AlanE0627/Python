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
#include "Stage.h"

#include <chrono>
#include "AutoPotion.h"
#include <cstdio>

#include <nlnx/nx.hpp>
#include <utility>

#include "../Configuration.h"
#include "../IO/UI.h"
#include "../IO/UITypes/UIStatusBar.h"
#include "../Net/Packets/AttackAndSkillPackets.h"
#include "../Net/Packets/GameplayPackets.h"
#include "../Net/Packets/PlayerInteractionPackets.h"
#include "../../IO/UITypes/UICharInfo.h"
#include "../Timer.h"

namespace ms {
namespace {
auto fn_take_damage = []<typename... T>(T && ...args) {
    TakeDamagePacket(std::forward<T>(args)...).dispatch();
};
auto fn_change_map = []<typename... T>(T && ...args) {
    ChangeMapPacket(std::forward<T>(args)...).dispatch();
};
auto fn_pickup_item = []<typename... T>(T && ...args) {
    PickupItemPacket(std::forward<T>(args)...).dispatch();
};
auto fn_player_map_transfer = []<typename... T>(T && ...args) {
    PlayerMapTransferPacket(std::forward<T>(args)...).dispatch();
};
auto fn_admin_enter_map = []<typename... T>(T && ...args) {
    AdminEnterMapPacket(std::forward<T>(args)...).dispatch();
};
auto fn_change_channel = []<typename... T>(T && ...args) {
    ChangeChannelPacket(std::forward<T>(args)...).dispatch();
};
auto fn_char_info_req = []<typename... T>(T && ...args) {
    CharInfoRequestPacket(std::forward<T>(args)...).dispatch();
};
}  // namespace

Stage::Stage() :
    state_(State::INACTIVE),
    map_id_(10000),
    level_before_(1),
    combat_(player_, chars_, mobs_, reactors_),
    mob_combat_(player_, chars_, mobs_),
    exp_before_(0) {}

void Stage::init() {
    drops_.init();
}

void Stage::load(int32_t mapid, int8_t portalid) {
    switch (state_) {
        case State::INACTIVE:
            load_map(mapid);
            respawn(portalid);
            break;
        case State::TRANSITION: respawn(portalid); break;
    }

    state_ = State::ACTIVE;
}

void Stage::load_player(const CharEntry &entry,
                        uint8_t wid,
                        uint8_t channel_id) {
    player_ = Player(entry, wid, channel_id);
    playable_ = player_;

    start_ = ContinuousTimer::get().start();

    CharStats stats = player_.get_stats();
    level_before_ = stats.get_stat(MapleStat::Id::LEVEL);
    exp_before_ = stats.get_exp();
}

void Stage::clear() {
    state_ = State::INACTIVE;

    chars_.clear();
    npcs_.clear();
    mobs_.clear();
    drops_.clear();
    reactors_.clear();
    summons_.clear();
    field_objects_.clear();
}

void Stage::load_map(int32_t mapid) {
    Stage::map_id_ = mapid;

    std::string strid = string_format::extend_id(mapid, 9);
    std::string prefix = std::to_string(mapid / 100000000);

    nl::node src = mapid == -1
                       ? nl::nx::ui_img("CashShopPreview.img")
                       : nl::nx::map["Map"]["Map" + prefix][strid + ".img"];

    tiles_objs_ = MapTilesObjs(src);
    backgrounds_ = MapBackgrounds(src["back"]);
    physics_ = Physics(src["foothold"]);
    map_info_ = MapInfo(src,
                        physics_.get_fht().get_walls(),
                        physics_.get_fht().get_borders());
    portals_ = MapPortals(src["portal"], mapid);
}

void Stage::respawn(int8_t portalid) {
    Music(map_info_.get_bgm()).play();

    Point<int16_t> spawnpoint = portals_.get_portal_by_id(portalid);

    field_objects_.place_town_door(get_town_door_position());

    if (has_spawn_override_) {
        spawnpoint = spawn_override_ - Point<int16_t>(0, 30);
        has_spawn_override_ = false;
    }
    Point<int16_t> startpos = physics_.get_y_below(spawnpoint);
    player_.respawn(startpos, map_info_.is_underwater());
    camera_.set_position(startpos);
    camera_.set_view(map_info_.get_walls(), map_info_.get_borders());
}

void Stage::draw(float alpha) const {
    if (state_ != State::ACTIVE) {
        return;
    }

    Point<int16_t> viewpos = camera_.position(alpha);
    Point<double> viewrpos = camera_.real_position(alpha);
    double viewx = viewrpos.x();
    double viewy = viewrpos.y();

    backgrounds_.drawbackgrounds(viewx, viewy, alpha);

    for (auto id : Layer::IDs) {
        tiles_objs_.draw(id, viewpos, alpha);

        // maple-offline: doors and mists in the player's layer, above the
        // map tiles there and behind characters
        if (id == player_.get_layer()) {
            field_objects_.draw(viewx, viewy, alpha);
        }
        reactors_.draw(id, viewx, viewy, alpha);
        npcs_.draw(id, viewx, viewy, alpha);
        mobs_.draw(id, viewx, viewy, alpha);
        chars_.draw(id, viewx, viewy, alpha);
        player_.draw(id, viewx, viewy, alpha);
        drops_.draw(id, viewx, viewy, alpha);
    }

    summons_.draw(viewx, viewy, alpha);
    combat_.draw(viewx, viewy, alpha);
    mob_combat_.draw(viewx, viewy, alpha);
    portals_.draw(viewpos, alpha);
    backgrounds_.drawforegrounds(viewx, viewy, alpha);
    effect_.draw();

    if (clock_active_) {
        auto left = std::chrono::duration_cast<std::chrono::seconds>(
                clock_end_ - std::chrono::steady_clock::now()).count();

        if (left < 0) {
            left = 0;
        }

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", static_cast<int>(left / 60), static_cast<int>(left % 60));
        // the text object is mutable state only for display
        const_cast<Text &>(clock_text_).change_text(buf);

        int16_t x = static_cast<int16_t>(Constants::Constants::get().get_viewwidth() / 2);
        clock_bg_.draw(DrawArgument(Point<int16_t>(static_cast<int16_t>(x - 129), 30)));
        clock_text_.draw(Point<int16_t>(x, 44));
    }
}

void Stage::update() {
    if (state_ != State::ACTIVE) {
        return;
    }

    combat_.update();

    if (held_skill_ != 0 && !player_.is_attacking()) {
        combat_.use_move(held_skill_);
    }

    // an attack pressed during the previous swing continues the chain
    if (!aran_queue_.empty() && !player_.is_attacking()) {
        std::vector<int32_t> dirs = aran_queue_.front();
        aran_queue_.erase(aran_queue_.begin());
        aran_attack(dirs);
    }

    mob_combat_.update();
    summons_.update();
    AutoPotion::update();
    field_objects_.update();
    backgrounds_.update();
    effect_.update();
    tiles_objs_.update();

    reactors_.update(physics_);
    npcs_.update(physics_);
    mobs_.update(physics_);
    chars_.update(physics_);
    drops_.update(physics_);
    player_.update(physics_);

    portals_.update(player_.get_position());
    camera_.update(player_.get_position());

    if (player_.is_invincible()) {
        return;
    }

    if (int32_t oid_id = mobs_.find_colliding(player_.get_phobj())) {
        if (MobAttack attack = mobs_.create_attack(oid_id)) {
            MobAttackResult result = player_.damage(attack);
            fn_take_damage(result, TakeDamagePacket::From::TOUCH);
        }
    }
}

void Stage::show_character_effect(int32_t cid, CharEffect::Id effect) {
    if (auto character = get_character(cid)) {
        character->get().show_effect_id(effect);
    }
}

void Stage::check_portals() {
    if (player_.is_attacking()) {
        return;
    }

    Point<int16_t> playerpos = player_.get_position();

    // maple-offline: Mystic Door before the map's own portals
    if (field_objects_.try_enter(playerpos, map_id_)) {
        return;
    }

    Portal::WarpInfo warpinfo = portals_.find_warp_at(playerpos);

    if (warpinfo.intramap) {
        Point<int16_t> spawnpoint =
            portals_.get_portal_by_name(warpinfo.toname);
        Point<int16_t> startpos = physics_.get_y_below(spawnpoint);

        player_.respawn(startpos, map_info_.is_underwater());
    } else if (warpinfo.valid) {
        fn_change_map(false, -1, warpinfo.name, false);

        CharStats &stats = Stage::get().get_player().get_stats();

        stats.set_mapid(warpinfo.mapid);

        Sound(Sound::Name::PORTAL).play();
    }
}

void Stage::check_seats() {
    if (player_.is_sitting() || player_.is_attacking()) {
        return;
    }

    auto seat = map_info_.findseat(player_.get_position());
    player_.set_seat(seat);
}

void Stage::check_ladders(bool up) {
    if (player_.is_climbing() || player_.is_attacking()) {
        return;
    }

    auto ladder = map_info_.findladder(player_.get_position(), up);
    player_.set_ladder(ladder);
}

void Stage::check_drops() {
    Point<int16_t> playerpos = player_.get_position();
    auto [drop_oid, drop_pos] = drops_.find_loot_at(playerpos);

    if (drop_oid) {
        fn_pickup_item(drop_oid, drop_pos);
    }
}

void Stage::send_key(KeyType::Id type, int32_t action, bool down) {
    if (state_ != State::ACTIVE || !playable_) {
        return;
    }

    switch (type) {
        case KeyType::Id::ACTION:
            if (down && (action == KeyAction::Id::UP || action == KeyAction::Id::DOWN
                         || action == KeyAction::Id::LEFT || action == KeyAction::Id::RIGHT)) {
                int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count();
                aran_dirs_.emplace_back(action, now);

                if (aran_dirs_.size() > 4) {
                    aran_dirs_.erase(aran_dirs_.begin());
                }
            }

            if (down) {
                switch (action) {
                    case KeyAction::Id::UP:
                        check_ladders(true);
                        check_portals();
                        break;
                    case KeyAction::Id::DOWN: check_ladders(false); break;
                    case KeyAction::Id::SIT: check_seats(); break;
                    case KeyAction::Id::ATTACK: {
                        int32_t job = player_.get_stats().get_stat(MapleStat::Id::JOB);

                        if (job / 100 == 21 && player_.get_weapontype() == Weapon::Type::POLEARM) {
                            aran_press();
                        } else {
                            combat_.use_move(0);
                        }

                        break;
                    }

                    case KeyAction::Id::PICKUP: check_drops(); break;
                    case KeyAction::Id::INTERACT_HARVEST:
                        npcs_.talk_nearest(player_.get_position());
                        break;
                }
            }

            playable_->get().send_action(KeyAction::get_action_by_id(action),
                                         down);
            break;
        case KeyType::Id::SKILL:
            if (down) {
                combat_.use_move(action);

                // Keydown skills keep firing while the key stays pressed
                std::string sid = std::to_string(action);

                if (sid.size() < 7) {
                    sid.insert(0, 7 - sid.size(), '0');
                }

                held_skill_ = nl::nx::skill[sid.substr(0, sid.size() - 4) + ".img"]
                                           ["skill"][sid]["keydown"]
                                  ? action
                                  : 0;
            } else if (action == held_skill_) {
                held_skill_ = 0;
            }

            break;
        case KeyType::Id::ITEM: player_.use_item(action); break;
        case KeyType::Id::FACE: player_.set_expression(action); break;
    }
}

bool Stage::aran_has(int32_t skill) {
    return player_.get_skills().get_level(skill) > 0;
}

// Aran attacks are key commands, not hotkeys (commands from the skill
// descriptions): attack x2 Double Swing, x3 Triple Swing; right after Triple
// Swing forward / up / down + attack: Final Charge / Final Cross / Final
// Blow; down, forward + attack: Combo Smash/Fenrir/Tempest by combo count;
// up, up + attack: Rolling Spin. Full/Over Swing replace the swings.
void Stage::aran_press() {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();

    std::vector<int32_t> dirs;

    for (const auto &[dir, time] : aran_dirs_) {
        if (now - time < 600) {
            dirs.push_back(dir);
        }
    }

    aran_dirs_.clear();

    if (player_.is_attacking() || !aran_queue_.empty()) {
        if (aran_queue_.size() < 3) {
            aran_queue_.push_back(dirs);
        }

        return;
    }

    aran_attack(dirs);
}

void Stage::aran_attack(const std::vector<int32_t> &dirs) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();

    int32_t forward = player_.getflip() ? KeyAction::Id::RIGHT : KeyAction::Id::LEFT;
    size_t n = dirs.size();
#ifdef MS_DESKTOP_DEBUG
    std::fprintf(stderr, "[dbg] aran dirs %zu last %d forward %d since_triple %lld chain %d combo %d\n", n,
                 n ? dirs[n - 1] : -1, forward, (long long) (now - aran_last_triple_), aran_chain_, aran_combo_);
#endif
    int32_t skill = 0;

    if (n >= 2 && dirs[n - 2] == KeyAction::Id::DOWN && dirs[n - 1] == forward) {
        if (aran_combo_ >= 200 && aran_has(21120006)) {
            skill = 21120006;
        } else if (aran_combo_ >= 100 && aran_has(21110004)) {
            skill = 21110004;
        } else if (aran_combo_ >= 30 && aran_has(21100004)) {
            skill = 21100004;
        }
    } else if (n >= 2 && dirs[n - 2] == KeyAction::Id::UP && dirs[n - 1] == KeyAction::Id::UP) {
        if (aran_has(21110006)) {
            skill = 21110006;
        }
    } else if (n >= 1 && now - aran_last_triple_ < 2000) {
        if (dirs[n - 1] == forward && aran_has(21100002)) {
            skill = 21100002;
        } else if (dirs[n - 1] == KeyAction::Id::UP && aran_has(21110003)) {
            skill = 21110003;
        } else if (dirs[n - 1] == KeyAction::Id::DOWN && aran_has(21120005)) {
            skill = 21120005;
        }
    }

    if (skill != 0) {
        aran_chain_ = 0;
        aran_last_triple_ = 0;
    } else {
        aran_chain_ = (now - aran_last_attack_ < 1000 && aran_chain_ < 2) ? aran_chain_ + 1 : 0;

        if (aran_chain_ == 1 && aran_has(21000002)) {
            skill = aran_has(21120009) ? 21120009 : aran_has(21110007) ? 21110007 : 21000002;
        } else if (aran_chain_ == 2 && aran_has(21100001)) {
            skill = aran_has(21120010) ? 21120010 : aran_has(21110008) ? 21110008 : 21100001;
            aran_last_triple_ = now;
        } else {
            aran_chain_ = 0;
        }
    }

    aran_last_attack_ = now;
    combat_.use_move(skill);
}

Cursor::State Stage::send_cursor(bool pressed, Point<int16_t> position) {
    auto statusbar = UI::get().get_element<UIStatusBar>();

    if (statusbar && statusbar->get().is_menu_active()) {
        if (pressed) {
            statusbar->get().remove_menus();
        }

        if (statusbar->get().is_in_range(position)) {
            return statusbar->get().send_cursor(pressed, position);
        }
    }

    return npcs_.send_cursor(pressed, position, camera_.position());
}

void Stage::doubleclick(Point<int16_t> pos) {
    auto chr = chars_.get_char(pos, camera_.position());

    if (chr) {
        fn_char_info_req(chr->get().get_oid());
    } else if (chars_.inrange(player_.get_position(),
                              pos,
                              camera_.position())) {
        fn_char_info_req(player_.get_oid());
    }
}

bool Stage::is_player(int32_t cid) const {
    return cid == player_.get_oid();
}

MapNpcs &Stage::get_npcs() {
    return npcs_;
}

MapChars &Stage::get_chars() {
    return chars_;
}

MapMobs &Stage::get_mobs() {
    return mobs_;
}

MapReactors &Stage::get_reactors() {
    return reactors_;
}

MapDrops &Stage::get_drops() {
    return drops_;
}

Player &Stage::get_player() {
    return player_;
}

FieldObjects &Stage::get_field_objects() {
    return field_objects_;
}

void Stage::set_clock(int32_t seconds) {
    clock_active_ = seconds > 0;

    if (clock_active_) {
        clock_end_ = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        clock_bg_ = nl::nx::map["Obj"]["etc.img"]["timer"]["backgrnd"];
        clock_text_ = Text(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::WHITE);
    }
}

Point<int16_t> Stage::get_town_door_position() const {
    return portals_.get_portal_by_id(0x80) + Point<int16_t>(0, 30);
}

Summons &Stage::get_summons() {
    return summons_;
}

Combat &Stage::get_combat() {
    return combat_;
}

MobCombat &Stage::get_mob_combat() {
    return mob_combat_;
}

std::optional<std::reference_wrapper<Char>> Stage::get_character(int32_t cid) {
    if (is_player(cid)) {
        return create_optional<Char>(&player_);
    }
    return create_optional<Char>(chars_.get_char(cid));
}

int Stage::get_mapid() {
    return map_id_;
}

void Stage::add_effect(std::string path) {
    effect_ = MapEffect(std::move(path));
}

int64_t Stage::get_uptime() {
    return ContinuousTimer::get().stop(start_);
}

uint16_t Stage::get_uplevel() {
    return level_before_;
}

int64_t Stage::get_upexp() {
    return exp_before_;
}

void Stage::transfer_player() {
    fn_player_map_transfer();

    if (Configuration::get().get_admin()) {
        fn_admin_enter_map(AdminEnterMapPacket::Operation::ALERT_ADMINS);
    }
}

void Stage::clear_channel_objects() {
    chars_.clear();
    mobs_.clear();
    drops_.clear();
    reactors_.clear();
}

void Stage::change_channel(uint8_t ch) {
    UI::get().disable();
    GraphicsGL::get().lock();
    fn_change_channel(ch);
    player_.set_channel_id(ch);
    state_ = State::TRANSITION;
}
}  // namespace ms