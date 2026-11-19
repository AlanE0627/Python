// maple-offline: v83 party window. See UIParty.h.
#include "UIParty.h"

#include <nlnx/nx.hpp>

#include "../../Character/Job.h"
#include "../../Gameplay/PartyState.h"
#include "../../Graphics/Text.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../Components/MapleButton.h"
#include "../KeyAction.h"

namespace ms {
UIParty::UIParty() : UIDragElement<PosUSERLIST>(Point<int16_t>(312, 20)) {
    nl::node src = nl::nx::ui_img("UIWindow.img")["UserList"];
    nl::node party = src["Party"];

    sprites_.emplace_back(src["backgrnd"]);

    // Buddy, Party, Guild, Guild Alliance, Blacklist (only Party is used)
    for (int i = 0; i < 5; i++) {
        tabs_.emplace_back(src["Tab"][i == 1 ? "enabled" : "disabled"][std::to_string(i)]);
    }

    header_ = party["party0"];
    columns_ = party["party5"];
    row_ = party["party1"];

    buttons_[CLOSE] = std::make_unique<MapleButton>(nl::nx::ui_img("Basic.img")["BtClose"],
                                                    Point<int16_t>(290, 6));
    buttons_[CREATE] = std::make_unique<MapleButton>(party["BtCreate"], Point<int16_t>(16, 345));
    buttons_[LEAVE] = std::make_unique<MapleButton>(party["BtWithdraw"], Point<int16_t>(66, 345));

    dimension_ = Point<int16_t>(312, 389);
}

void UIParty::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    int16_t x = 12;

    for (const auto &tab : tabs_) {
        tab.draw(position_ + Point<int16_t>(x, 30));
        x += static_cast<int16_t>(tab.width() + 14);
    }

    header_.draw(position_ + Point<int16_t>(15, 55));
    columns_.draw(position_ + Point<int16_t>(16, 86));

    const PartyState &party = PartyState::get();

    for (size_t i = 0; i < party.members.size(); i++) {
        const PartyMember &m = party.members[i];
        auto y = static_cast<int16_t>(104 + i * 18);
        row_.draw(position_ + Point<int16_t>(16, y));

        Point<int16_t> base = position_ + Point<int16_t>(0, y + 1);
        std::string name = m.name + (m.id == party.leader ? " *" : "");
        Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, name)
            .draw(base + Point<int16_t>(26, 0));
        Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, Job(m.job).get_name())
            .draw(base + Point<int16_t>(140, 0));
        Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, std::to_string(m.level))
            .draw(base + Point<int16_t>(232, 0));
    }

    UIElement::draw_buttons(alpha);
}

void UIParty::update() {
    UIElement::update();

    bool in_party = PartyState::get().in_party();
    buttons_[CREATE]->set_active(!in_party);
    buttons_[LEAVE]->set_active(in_party);
}

void UIParty::send_key(int32_t, bool pressed, bool escape) {
    if (pressed && escape) {
        deactivate();
    }
}

UIElement::Type UIParty::get_type() const {
    return TYPE;
}

Button::State UIParty::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case CLOSE: deactivate(); break;
        case CREATE: CreatePartyPacket().dispatch(); break;
        case LEAVE: LeavePartyPacket().dispatch(); break;
    }

    return Button::State::NORMAL;
}
}  // namespace ms
