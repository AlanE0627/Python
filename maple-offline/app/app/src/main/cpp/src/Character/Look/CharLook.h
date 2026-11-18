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

#include "../../Net/Login.h"
#include "../../Template/Interpolated.h"
#include "../../Util/Randomizer.h"
#include "../../Util/TimedBool.h"
#include "Body.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "../../Graphics/Texture.h"
#include "../../Graphics/Animation.h"
#include "CharEquips.h"
#include "Face.h"
#include "Hair.h"

namespace ms {
class CharLook {
public:
        // maple-offline: current action name (debugging)
        const std::string &get_actionstr() const { return actionstr_; }

    CharLook(const LookEntry &entry);

    CharLook();

    void reset();

    void draw(const DrawArgument &args, float alpha) const;

    void draw(Point<int16_t> pos,
              bool flip,
              Stance::Id stance,
              Expression::Id expression) const;

    bool update(uint16_t timestep);

    void set_hair(int32_t hairid);

    void set_body(int32_t skinid);

    void set_face(int32_t faceid);

    void add_equip(int32_t equipid);
    void remove_equip(EquipSlot::Id slot);

    void attack(bool degenerate);

    void attack(Stance::Id stance);

    void set_stance(Stance::Id stance);

    void set_expression(Expression::Id expression);

    void set_action(const std::string &action);

    void set_direction(bool mirrored);

    void set_alerted(int64_t millis);

    bool get_alerted() const;

    bool is_twohanded(Stance::Id stance) const;

    uint16_t get_attackdelay(size_t no, uint8_t first_frame) const;

    uint8_t get_frame() const;

    Stance::Id get_stance() const;

    const Body *get_body() const;

    const Hair *get_hair() const;

    const Face *get_face() const;

    const CharEquips &get_equips() const;

    // Monster Riding: draw TamingMob/0<mount>.img (+ saddle) under the
    // character, who sits on the mount's navel point
    void set_mount(int32_t mount_id, int32_t saddle_id);

    void clear_mount();

    int32_t get_mount() const { return mount_id_; }

    // Transformation (MORPH buff): Morph.nx/<id>.img replaces the character
    void set_morph(int32_t morph_id);

    void clear_morph();

    int32_t get_morph() const { return morph_id_; }

    // Initialize drawinfo
    static void init();

private:
    void updatetwohanded();

    void draw(const DrawArgument &args,
              Stance::Id interstance,
              Expression::Id interexp,
              uint8_t interframe,
              uint8_t interfcframe) const;

    uint16_t get_delay(Stance::Id stance, uint8_t frame) const;

    uint8_t getnextframe(Stance::Id stance, uint8_t frame) const;

    Stance::Id getattackstance(uint8_t attack, bool degenerate) const;

    Nominal<Stance::Id> stance_;
    Nominal<uint8_t> st_frame_;
    uint16_t st_elapsed_;

    Nominal<Expression::Id> expression_;
    Nominal<uint8_t> exp_frame_;
    uint16_t exp_elapsed_;

    bool flip_;

    const BodyAction *action_;
    std::string actionstr_;
    uint8_t actframe_;

    const Body *body_;
    const Hair *hair_;
    const Face *face_;
    CharEquips equips_;

    struct MountPart {
        Texture texture;
        bool front;
    };

    struct MountFrame {
        std::vector<MountPart> parts;
        Point<int16_t> navel;
        uint16_t delay;
    };

    const std::vector<MountFrame> *mount_frames(Stance::Id stance) const;

    void draw_mount(const DrawArgument &args,
                    Stance::Id stance,
                    bool front) const;

    std::string morph_stance_name(Stance::Id stance) const;

    int32_t morph_id_ = 0;
    std::unordered_map<std::string, Animation> morph_anims_;
    std::string morph_stance_;

    int32_t mount_id_ = 0;
    std::unordered_map<std::string, std::vector<MountFrame>> mount_anims_;
    std::string mount_stance_;
    uint8_t mount_frame_ = 0;
    uint16_t mount_elapsed_ = 0;

    Randomizer randomizer_;
    TimedBool alerted_;

    static BodyDrawInfo draw_info_;
    static std::unordered_map<int32_t, Hair> hair_styles_;
    static std::unordered_map<int32_t, Face> face_types_;
    static std::unordered_map<int32_t, Body> body_types_;
};
}  // namespace ms