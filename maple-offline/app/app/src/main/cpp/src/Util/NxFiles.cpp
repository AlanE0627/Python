//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "NxFiles.h"

#include <fstream>
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>

namespace ms::NxFiles {
Error init(GLFMDisplay *pApp) {
    try {
        nl::nx::load_all(pApp);
    } catch (const std::exception &ex) {
        static const std::string message = ex.what();
        return Error(Error::Code::NLNX, message.c_str());
    }

    // maple-offline targets the original GMS v83 UI.wz (UIWindow.img / StatusBar.img layout).
    if (nl::nx::ui_img("UIWindow.img").size() == 0 || nl::nx::ui_img("StatusBar.img").size() == 0) {
        return Error::Code::WRONG_UI_FILE;
    }

    return Error::Code::NONE;
}
}  // namespace ms::NxFiles