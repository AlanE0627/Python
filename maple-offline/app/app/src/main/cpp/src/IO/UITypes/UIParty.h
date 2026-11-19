// maple-offline: v83 party window (UIWindow.img/UserList, Party tab).
// Create / leave a party and list its members; needed to enter party quests
// even when playing alone.
#pragma once

#include "../UIDragElement.h"

namespace ms {
class UIParty : public UIDragElement<PosUSERLIST> {
public:
    static constexpr Type TYPE = UIElement::Type::USER_LIST;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = true;

    UIParty();

    void draw(float alpha) const override;
    void update() override;
    void send_key(int32_t keycode, bool pressed, bool escape) override;
    UIElement::Type get_type() const override;

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    enum Buttons : uint16_t { CLOSE, CREATE, LEAVE };

    std::vector<Texture> tabs_;
    Texture header_;
    Texture columns_;
    Texture row_;
};
}  // namespace ms
