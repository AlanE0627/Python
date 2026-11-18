// maple-offline: v83 Cygnus creation — thin wrapper over the shared UINewChar.
#pragma once

#include "UINewChar.h"

namespace ms {
class UICygnusCreation : public UINewChar {
public:
    UICygnusCreation() : UINewChar(UINewChar::Race::CYGNUS) {}
};
}  // namespace ms
