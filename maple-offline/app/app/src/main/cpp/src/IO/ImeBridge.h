#pragma once

#include <string>

namespace ms::ImeBridge {
// maple-offline: Android soft keyboard through a hidden EditText in
// MapleActivity, so composed input (Hangul) reaches text fields.
void show(const std::string &current_text);
void hide();
// Apply text / Enter received from the Java side; call once per frame on the
// game thread.
void poll();
}  // namespace ms::ImeBridge
