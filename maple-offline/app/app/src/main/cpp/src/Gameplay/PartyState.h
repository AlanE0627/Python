// maple-offline: the player's party as last reported by PARTY_OPERATION.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ms {
struct PartyMember {
    int32_t id;
    std::string name;
    int32_t job;
    int32_t level;
    int32_t channel;
    int32_t map;
};

struct PartyState {
    int32_t party_id = 0;
    int32_t leader = 0;
    std::vector<PartyMember> members;

    bool in_party() const { return party_id != 0; }

    void clear() {
        party_id = 0;
        leader = 0;
        members.clear();
    }

    static PartyState &get() {
        static PartyState state;
        return state;
    }
};
}  // namespace ms
