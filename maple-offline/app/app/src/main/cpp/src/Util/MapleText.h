// maple-offline: resolves the inline tags used in quest and NPC strings
// (#p npc, #m map, #o mob, #t/#z item, #q skill, #h player, #c item count)
// and normalises colour codes to the ones the text renderer understands.
#pragma once

#include <string>

namespace ms::MapleText {
// keep_lists: leave #L..#/#l selection markers for the NPC dialog parser.
std::string format(const std::string &text, bool keep_lists = false);

// Removes every remaining # code (for plain one-line labels).
std::string strip(const std::string &text);
}  // namespace ms::MapleText
