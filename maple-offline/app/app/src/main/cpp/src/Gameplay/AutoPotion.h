// maple-offline: drinks the first HP / MP potion in the Use tab when HP or
// MP falls below the percentage set in the system options. The v83 client
// needed a pet with a potion pouch for this; here it is built in.
#pragma once

namespace ms::AutoPotion {
void update();
}
