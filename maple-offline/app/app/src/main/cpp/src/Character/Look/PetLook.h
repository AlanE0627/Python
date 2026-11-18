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

#include "../../Gameplay/Physics/Physics.h"
#include "../../Graphics/Animation.h"
#include "../../Graphics/Text.h"
#include "../../IO/Components/ChatBalloon.h"
#include "../../Template/EnumMap.h"

namespace ms {
class PetLook {
public:
    enum Stance : uint8_t {
        MOVE,
        STAND,
        JUMP,
        ALERT,
        PRONE,
        FLY,
        HANG,
        WARP,
        LENGTH
    };

    static Stance stancebyvalue(uint8_t value) {
        uint8_t valueh = value / 2;

        return valueh >= LENGTH ? STAND : static_cast<Stance>(valueh);
    }

    PetLook(int32_t iid,
            std::string name,
            int32_t uniqueid,
            Point<int16_t> pos,
            uint8_t stance,
            int32_t fhid);

    PetLook();

    void draw(double viewx, double viewy, float alpha) const;

    void update(const Physics &physics, Point<int16_t> charpos);

    void set_position(int16_t xpos, int16_t ypos);

    void set_stance(Stance stance);

    void set_stance(uint8_t stancebyte);

    int32_t get_itemid() const;

    int32_t get_uniqueid() const { return unique_id_; }

    Stance get_stance() const;

    // Speech balloon above the pet (PET_CHAT / PET_COMMAND responses)
    void speak(const std::string &line);

    // Show a random line from String/PetDialog.img/<id>/<key><n><suffix>
    void speak_dialog(const std::string &key, const std::string &suffix);

    // Play Effect/PetEff.img/Basic/LevelUp over the pet
    void show_levelup();

private:
    int32_t item_id_;
    std::string name_;
    int32_t unique_id_;
    Stance stance_;
    bool flip_;

    EnumMap<Stance, Animation> animations_;
    PhysicsObject phobj_;
    Text name_label_;
    ChatBalloon chat_balloon_;
    Animation levelup_;
    bool levelup_active_ = false;
};
}  // namespace ms