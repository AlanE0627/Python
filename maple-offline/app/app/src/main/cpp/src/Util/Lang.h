// maple-offline: UI text written by the client itself (labels, notices).
// Game data text (item / map / NPC names ...) comes from String_ko.nx.
#pragma once

namespace ms::Lang {
// quest window / NPC quest menu
inline constexpr const char *NO_QUESTS = "퀘스트가 없습니다.";
inline constexpr const char *EXP = "경험치";
inline constexpr const char *MESOS = "메소";
inline constexpr const char *FAME = "인기도";
inline constexpr const char *RANDOM = "(랜덤)";
inline constexpr const char *QUEST_MENU = "무엇을 하시겠습니까?";
inline constexpr const char *QUEST_CAN_COMPLETE = "[완료 가능한 퀘스트]";
inline constexpr const char *QUEST_AVAILABLE = "[진행 가능한 퀘스트]";
inline constexpr const char *TALK = "대화하기";
inline constexpr const char *QUEST_TAG = "[퀘스트] ";
inline constexpr const char *QUEST_STARTED = " 퀘스트를 시작했습니다.";
inline constexpr const char *QUEST_COMPLETED = " 퀘스트를 완료했습니다.";
inline constexpr const char *QUEST_FORFEITED = " 퀘스트를 포기했습니다.";
inline constexpr const char *QUEST_EXPIRED = " 퀘스트의 제한 시간이 끝났습니다.";
inline constexpr const char *QUEST_TIME_LIMIT = "분 제한 퀘스트입니다.";
inline constexpr const char *NOT_ENOUGH_MESOS = "메소가 부족합니다.";
inline constexpr const char *UNEQUIP_FIRST = "착용 중인 아이템을 먼저 벗어 주세요.";
inline constexpr const char *INVENTORY_FULL = "인벤토리 공간이 부족합니다.";

// status messages (bottom right)
inline constexpr const char *CANT_GET_ITEMS = "더 이상 아이템을 얻을 수 없습니다.";
inline constexpr const char *ITEM_GAINED = "아이템을 얻었습니다";
inline constexpr const char *ITEM_LOST = "아이템을 잃었습니다";
inline constexpr const char *MESOS_GAINED = "메소를 얻었습니다";
inline constexpr const char *EXP_GAINED = "경험치를 얻었습니다";
inline constexpr const char *BONUS_EXP = "보너스 경험치";
inline constexpr const char *FAME_GAINED = "인기도가 올랐습니다";
inline constexpr const char *COUNT_UNIT = "개";
inline constexpr const char *TAB_EQUIP = "장비";
inline constexpr const char *TAB_USE = "소비";
inline constexpr const char *TAB_SETUP = "설치";
inline constexpr const char *TAB_ETC = "기타";
inline constexpr const char *TAB_CASH = "캐시";

// item / equip tooltip
inline constexpr const char *REQ_LEV = "요구 레벨";
inline constexpr const char *REQ_STR = "요구 STR";
inline constexpr const char *REQ_DEX = "요구 DEX";
inline constexpr const char *REQ_INT = "요구 INT";
inline constexpr const char *REQ_LUK = "요구 LUK";
inline constexpr const char *CATEGORY = "장비 분류";
inline constexpr const char *ATTACK_SPEED = "공격 속도";
inline constexpr const char *UPGRADES = "업그레이드 가능 횟수";
inline constexpr const char *MALE_ONLY = "(남)";
inline constexpr const char *FEMALE_ONLY = "(여)";
inline constexpr const char *UNTRADABLE = "교환 불가";
inline constexpr const char *ONE_OF_A_KIND = "고유 아이템";
inline constexpr const char *CASH_ITEM = "캐시 아이템";

// skill tooltip
inline constexpr const char *MASTER_LEVEL = "마스터 레벨";
inline constexpr const char *CURRENT_LEVEL = "현재 레벨";
inline constexpr const char *NEXT_LEVEL = "다음 레벨";

inline constexpr const char *JOBS[6] = {"초보자", "전사", "마법사", "궁수", "도적", "해적"};

inline const char *speed_name(int speed) {
    if (speed <= 2) return "매우 빠름";
    if (speed <= 4) return "빠름";
    if (speed <= 6) return "보통";
    if (speed <= 8) return "느림";
    return "매우 느림";
}

// EquipStat order: STR DEX INT LUK HP MP WATK MAGIC WDEF MDEF ACC AVOID
// HANDS SPEED JUMP
inline constexpr const char *EQUIP_STATS[15] = {
    "STR",      "DEX",      "INT",    "LUK",    "최대 HP",  "최대 MP",
    "공격력",   "마력",     "물리방어력", "마법방어력", "명중치",   "회피치",
    "손재주",   "이동속도", "점프력"
};
}  // namespace ms::Lang
