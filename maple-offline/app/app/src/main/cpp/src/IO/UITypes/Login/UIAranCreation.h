// maple-offline: v83 Aran creation — thin wrapper over the shared UINewChar.
#pragma once

#include "UINewChar.h"

namespace ms {
class UIAranCreation : public UINewChar {
public:
    UIAranCreation() : UINewChar(UINewChar::Race::ARAN) {}
};
}  // namespace ms
