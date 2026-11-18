// maple-offline: client-side reading of Quest.nx Check.img requirements.
// The server stays authoritative; this only drives what the UI shows
// (available list, NPC overhead icons).
#pragma once

#include <cstdint>

namespace ms::QuestCheck {
// Startable now: not taken yet, has a start NPC, not a dated event, and
// level / job / prerequisite quest conditions are met.
bool can_start(int16_t qid);

// In progress and the end conditions the client can see (items held,
// mob kill counts) are met.
bool can_complete(int16_t qid);

// Overhead icon for an NPC: -1 none, 0 has a quest to give,
// 2 a quest can be turned in. Cached until the quest log, level or job
// changes.
int8_t npc_icon(int32_t npcid);
}  // namespace ms::QuestCheck
