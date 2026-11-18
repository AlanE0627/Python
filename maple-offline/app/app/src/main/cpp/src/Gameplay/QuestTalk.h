// maple-offline: the v83 client builds the quest part of an NPC
// conversation itself (quest list, Say.img pages, accept/decline) and only
// tells the server the final decision through QUEST_ACTION. Scripted
// quests hand the conversation to the server script instead.
#pragma once

#include <cstdint>

namespace ms::QuestTalk {
// Opens the quest menu for this NPC if it has quests to offer or take
// back. Returns false when there is nothing quest-related, so the caller
// talks to the NPC normally.
bool open(int32_t npcid, int32_t oid, bool scripted);
}  // namespace ms::QuestTalk
