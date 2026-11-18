#include "MapleText.h"

#include <cctype>
#include <nlnx/nx.hpp>

#include "../Data/ItemData.h"
#include "../Data/SkillData.h"
#include "../Gameplay/Stage.h"
#include "GameInfo.h"

namespace ms::MapleText {
namespace {
bool read_id(const std::string &s, size_t from, size_t &end, int32_t &id) {
    end = from;

    while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end]))) {
        end++;
    }

    if (end == from || end - from > 10) {
        return false;
    }

    id = static_cast<int32_t>(std::stoll(s.substr(from, end - from)));

    // tags close with '#'; tolerate a missing terminator at the end
    if (end < s.size() && s[end] == '#') {
        end++;
    }

    return true;
}

std::string item_name(int32_t id) {
    const ItemData &data = ItemData::get(id);

    if (data.is_valid()) {
        return data.get_name();
    }

    return "";
}
}  // namespace

std::string format(const std::string &text, bool keep_lists) {
    std::string out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];

        if (c != '#' || i + 1 >= text.size()) {
            out += c;
            continue;
        }

        char tag = text[i + 1];
        size_t end = 0;
        int32_t id = 0;

        switch (tag) {
            case 'b':
            case 'k':
            case 'r':
                out += '#';
                out += tag;
                i++;
                break;
            case 'd':
            case 'g':
                out += "#b";
                i++;
                break;
            case 'e':
            case 'n':
            case 'E':
            case 'I':
                i++;
                break;
            case 'h':
                // #h # or #h0#
                end = text.find('#', i + 2);

                if (end != std::string::npos && end - i <= 4) {
                    out += Stage::get().get_player().get_name();
                    i = end;
                } else {
                    i++;
                }

                break;
            case 'p':
            case 'o':
            case 'm':
            case 't':
            case 'z':
            case 'q':
            case 'c':
            case 'a':
            case 'R':
            case 'x':
                if (!read_id(text, i + 2, end, id)) {
                    i++;
                    break;
                }

                if (tag == 'p') {
                    out += static_cast<std::string>(
                            nl::nx::string["Npc.img"][std::to_string(id)]["name"]);
                } else if (tag == 'o') {
                    out += static_cast<std::string>(
                            nl::nx::string["Mob.img"][std::to_string(id)]["name"]);
                } else if (tag == 'm') {
                    out += NxHelper::Map::get_map_info_by_id(id).name;
                } else if (tag == 't' || tag == 'z') {
                    out += item_name(id);
                } else if (tag == 'q') {
                    out += SkillData::get(id).get_name();
                } else if (tag == 'c') {
                    out += std::to_string(
                            Stage::get().get_player().get_inventory().get_total_item_count(id));
                } else {
                    // mob kill counters and the like: the server keeps them
                    out += "0";
                }

                i = end - 1;
                break;
            case 'i':
            case 'v':
            case 's':
            case 'f':
            case 'F':
            case 'B':
                // pictures / gauges: no inline images in this renderer
                end = text.find('#', i + 2);
                i = (end == std::string::npos) ? text.size() : end;
                break;
            case 'L':
                if (keep_lists) {
                    out += c;
                } else {
                    end = text.find('#', i + 2);
                    i = (end == std::string::npos) ? text.size() : end;
                    out += "\n- ";
                }

                break;
            case 'l':
                if (keep_lists) {
                    out += c;
                } else {
                    i++;
                }

                break;
            default:
                out += c;
                break;
        }
    }

    return out;
}

std::string strip(const std::string &text) {
    std::string in = format(text);
    std::string out;

    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '#' && i + 1 < in.size()) {
            i++;
            continue;
        }

        out += in[i];
    }

    return out;
}
}  // namespace ms::MapleText
