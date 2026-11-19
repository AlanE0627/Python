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
#include "Job.h"

namespace ms {
Job::Job(uint16_t i) {
    change_job(i);
}

Job::Job() {
    change_job(0);
}

void Job::change_job(uint16_t i) {
    id_ = i;
    name_ = get_name(id_);

    if (id_ == 0) {
        level_ = Level::BEGINNER;
    } else if (id_ % 100 == 0) {
        level_ = Level::FIRST;
    } else if (id_ % 10 == 0) {
        level_ = Level::SECOND;
    } else if (id_ % 10 == 1) {
        level_ = Level::THIRD;
    } else {
        level_ = Level::FOURTH;
    }
}

bool Job::is_sub_job(uint16_t subid) const {
    for (int lvit = BEGINNER; lvit <= FOURTH; lvit++) {
        Level lv = static_cast<Level>(lvit);

        if (subid == get_subjob(lv)) {
            return true;
        }
    }

    return false;
}

bool Job::can_use(int32_t skill_id) const {
    uint16_t required = static_cast<uint16_t>(skill_id / 10000);
    return is_sub_job(required);
}

uint16_t Job::get_id() const {
    return id_;
}

uint16_t Job::get_subjob(Level lv) const {
    if (lv <= level_) {
        switch (lv) {
            case Level::BEGINNER: return 0;
            case Level::FIRST: return (id_ / 100) * 100;
            case Level::SECOND: return (id_ / 10) * 10;
            case Level::THIRD: return (level_ == Level::FOURTH) ? id_ - 1 : id_;
            case Level::FOURTH: return id_;
        }
    }

    return 0;
}

const std::string &Job::get_name() const {
    return name_;
}

Job::Level Job::get_level() const {
    return level_;
}

std::string Job::get_name(uint16_t jid) const {
    switch (jid) {
        case 0: return "초보자";
        case 100: return "검사";
        case 110: return "파이터";
        case 111: return "크루세이더";
        case 112: return "히어로";
        case 120: return "페이지";
        case 121: return "나이트";
        case 122: return "팔라딘";
        case 130: return "스피어맨";
        case 131: return "용기사";
        case 132: return "다크나이트";
        case 200: return "매지션";
        case 210: return "위자드(불,독)";
        case 211: return "메이지(불,독)";
        case 212: return "아크메이지(불,독)";
        case 220: return "위자드(썬,콜)";
        case 221: return "메이지(썬,콜)";
        case 222: return "아크메이지(썬,콜)";
        case 230: return "클레릭";
        case 231: return "프리스트";
        case 232: return "비숍";
        case 300: return "아처";
        case 310: return "헌터";
        case 311: return "레인저";
        case 312: return "보우마스터";
        case 320: return "사수";
        case 321: return "저격수";
        case 322: return "신궁";
        case 400: return "로그";
        case 410: return "어쌔신";
        case 411: return "허밋";
        case 412: return "나이트로드";
        case 420: return "시프";
        case 421: return "시프마스터";
        case 422: return "섀도어";
        case 500: return "해적";
        case 510: return "인파이터";
        case 511: return "버커니어";
        case 512: return "바이퍼";
        case 520: return "건슬링거";
        case 521: return "발키리";
        case 522: return "캡틴";
        case 1000: return "노블레스";
        case 900: return "운영자";
        case 910: return "슈퍼 운영자";
        case 1100:
        case 1110:
        case 1111: return "소울마스터";
        case 1200:
        case 1210:
        case 1211: return "플레임위자드";
        case 1300:
        case 1310:
        case 1311: return "윈드브레이커";
        case 1400:
        case 1410:
        case 1411: return "나이트워커";
        case 1500:
        case 1510:
        case 1511: return "스트라이커";
        case 2000: return "레전드";
        case 2100:
        case 2110:
        case 2111:
        case 2112: return "아란";
        default: return "";
    }
}

EquipStat::Id Job::get_primary(Weapon::Type weapontype) const {
    switch (id_ / 100) {
        case 2: return EquipStat::Id::INT;
        case 3: return EquipStat::Id::DEX;
        case 4: return EquipStat::Id::LUK;
        case 5:
            return (weapontype == Weapon::Type::GUN) ? EquipStat::Id::DEX
                                                     : EquipStat::Id::STR;
        default: return EquipStat::Id::STR;
    }
}

EquipStat::Id Job::get_secondary(Weapon::Type weapontype) const {
    switch (id_ / 100) {
        case 2: return EquipStat::Id::LUK;
        case 3: return EquipStat::Id::STR;
        case 4: return EquipStat::Id::DEX;
        case 5:
            return (weapontype == Weapon::Type::GUN) ? EquipStat::Id::STR
                                                     : EquipStat::Id::DEX;
        default: return EquipStat::Id::DEX;
    }
}
}  // namespace ms