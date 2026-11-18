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
#include "Messages.h"

#include "UI.h"
#include "UITypes/UIChatBar.h"

namespace ms {
const EnumMap<Messages::Type, const char *> Messages::messages = {
    "",
    "이 무기로는 사용할 수 없는 스킬입니다.",
    "HP가 부족하여 스킬을 사용할 수 없습니다.",
    "MP가 부족하여 스킬을 사용할 수 없습니다.",
    "화살이 부족합니다.",
    "탄환이 부족합니다.",
    "표창이 부족합니다.",
    "아직 다시 사용할 수 없는 스킬입니다.",
    "주문서의 신비한 힘이 아이템에 깃들었습니다.",
    "주문서가 빛났지만 아이템에는 아무 변화가 없습니다.",
    "주문서의 강한 힘을 이기지 못하고 아이템이 파괴되었습니다."
};

InChatMessage::InChatMessage(Messages::Type t) {
    type = t;
}

void InChatMessage::drop() const {
    if (type == Messages::Type::NONE) {
        return;
    }

    if (auto chatbar = UI::get().get_element<UIChatBar>()) {
        chatbar->get().display_message(type, UIChatBar::RED);
    }
}

ForbidSkillMessage::ForbidSkillMessage(SpecialMove::ForbidReason reason,
                                       Weapon::Type weapon) :
    InChatMessage(message_by_reason(reason, weapon)) {}

Messages::Type ForbidSkillMessage::message_by_reason(
    SpecialMove::ForbidReason reason,
    Weapon::Type weapon) {
    switch (reason) {
        case SpecialMove::FBR_WEAPONTYPE: return Messages::SKILL_WEAPONTYPE;
        case SpecialMove::FBR_HPCOST: return Messages::SKILL_HPCOST;
        case SpecialMove::FBR_MPCOST: return Messages::SKILL_MPCOST;
        case SpecialMove::FBR_COOLDOWN: return Messages::SKILL_COOLDOWN;
        case SpecialMove::FBR_BULLETCOST: return message_by_weapon(weapon);
        default: return Messages::NONE;
    }
}

Messages::Type ForbidSkillMessage::message_by_weapon(Weapon::Type weapon) {
    switch (weapon) {
        case Weapon::BOW:
        case Weapon::CROSSBOW: return Messages::SKILL_NOARROWS;
        case Weapon::CLAW: return Messages::SKILL_NOSTARS;
        default: return Messages::SKILL_NOBULLETS;
    }
}
}  // namespace ms