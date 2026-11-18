//	This file is part of MSClient.
//	Copyright (C) 2020 filoper
//
//	MSClient is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	MSClient is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with MSClient.  If not, see <https://www.gnu.org/licenses/>.

#include "StringHandling.h"

#include <sstream>

namespace ms::string_conversion {
std::string to_utf8_string(uint32_t value) {
    // maple-offline: the original shifted the 3/4-byte lead bits by the
    // wrong amount, which corrupted every Hangul syllable typed
    std::string out;

    if (value < 0x80) {
        out += static_cast<char>(value);
    } else if (value < 0x800) {
        out += static_cast<char>(0xC0 | (value >> 6));
        out += static_cast<char>(0x80 | (value & 0x3F));
    } else if (value < 0x10000) {
        out += static_cast<char>(0xE0 | (value >> 12));
        out += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (value & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | ((value >> 18) & 0x07));
        out += static_cast<char>(0x80 | ((value >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (value & 0x3F));
    }

    return out;
}

std::vector<uint32_t> to_utf8_vector(const std::string &text) {
    // maple-offline: bounds-checked decoder (truncated sequences at the end
    // of a string used to read past it)
    std::vector<uint32_t> vec;
    size_t n = text.size();

    for (size_t i = 0; i < n; i++) {
        auto b0 = static_cast<uint8_t>(text[i]);
        size_t extra = 0;
        uint32_t val = 0;

        if (b0 < 0x80) {
            vec.push_back(b0);
            continue;
        } else if ((b0 & 0xE0) == 0xC0) {
            extra = 1;
            val = b0 & 0x1F;
        } else if ((b0 & 0xF0) == 0xE0) {
            extra = 2;
            val = b0 & 0x0F;
        } else if ((b0 & 0xF8) == 0xF0) {
            extra = 3;
            val = b0 & 0x07;
        } else {
            // stray byte (e.g. Latin-1 text): keep it as that code point
            vec.push_back(b0);
            continue;
        }

        if (i + extra >= n) {
            break;
        }

        bool ok = true;

        for (size_t k = 1; k <= extra; k++) {
            auto bk = static_cast<uint8_t>(text[i + k]);

            if ((bk & 0xC0) != 0x80) {
                ok = false;
                break;
            }

            val = (val << 6) | (bk & 0x3F);
        }

        if (!ok) {
            vec.push_back(b0);
            continue;
        }

        vec.push_back(val);
        i += extra;
    }

    return vec;
}
}  // namespace ms::string_conversion