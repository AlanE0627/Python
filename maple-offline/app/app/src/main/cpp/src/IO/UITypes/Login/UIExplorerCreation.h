// maple-offline: v83 Explorer creation — thin wrapper over the shared UINewChar.
#pragma once

#include "UINewChar.h"

namespace ms {
class UIExplorerCreation : public UINewChar {
public:
    UIExplorerCreation() : UINewChar(UINewChar::Race::EXPLORER) {}
};
}  // namespace ms
