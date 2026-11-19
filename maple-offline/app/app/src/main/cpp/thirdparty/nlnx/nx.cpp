//////////////////////////////////////////////////////////////////////////////
// NoLifeNx - Part of the NoLifeStory project                               //
// Copyright © 2013 Peter Atashian                                          //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "nx.hpp"
#include "file.hpp"
#include "node.hpp"

#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace nl
{
namespace nx
{
std::vector<std::unique_ptr<file>> files;
std::string filePath;
bool exists(std::string name, GLFMDisplay *pApp)
{
    filePath = std::string(((ANativeActivity*)glfmGetAndroidActivity(pApp))->externalDataPath);
    FILE* file = fopen((filePath + "/" + name).c_str(), "rb");

    if (file != nullptr) {
        fclose(file);
        return true;
    }

    return false;
}
node add_file(std::string name)
{
    files.emplace_back(new file(filePath + "/" + name));
    return *files.back();
}
node base, character, effect, etc, item, map, mapPretty, mapLatest, map001, mob, morph, npc, quest, reactor, skill, sound, string, tamingmob, ui, uiko;
void load_all(GLFMDisplay *pApp)
{
    if (exists("Base.nx", pApp)) {
        base = add_file("Base.nx");
        character = add_file("Character.nx");
        effect = add_file("Effect.nx");
        etc = add_file("Etc.nx");
        item = add_file("Item.nx");
        map = add_file("Map.nx");
        // maple-offline: v83 has a single Map.wz; post-v83 map files alias to it.
        mapPretty = map;
        mapLatest = map;
        map001 = map;
        mob = add_file("Mob.nx");
        morph = add_file("Morph.nx");
        npc = add_file("Npc.nx");
        quest = add_file(exists("Quest_ko.nx", pApp) ? "Quest_ko.nx" : "Quest.nx");
        reactor = add_file("Reactor.nx");
        skill = add_file("Skill.nx");
        sound = add_file("Sound.nx");
        // maple-offline: Korean names (built by tools/nx_ko_patch.py) when present
        string = add_file(exists("String_ko.nx", pApp) ? "String_ko.nx" : "String.nx");
        tamingmob = add_file("TamingMob.nx");
        ui = add_file("UI.nx");
        // maple-offline: Korean UI overlay (tools/nx_merge_ui.py --overlay) when present
        if (exists("UI_ko.nx", pApp)) {
            uiko = add_file("UI_ko.nx");
        }
    } else if (exists("Data.nx", pApp)) {
        base = add_file("Data.nx");
        character = base["Character"];
        effect = base["Effect"];
        etc = base["Etc"];
        item = base["Item"];
        map = base["Map"];
        mob = base["Mob"];
        morph = base["Morph"];
        npc = base["Npc"];
        quest = base["Quest"];
        reactor = base["Reactor"];
        skill = base["Skill"];
        sound = base["Sound"];
        string = base["String"];
        tamingmob = base["TamingMob"];
        ui = base["UI"];
    } else {
        throw std::runtime_error("Failed to locate nx files.");
    }
}
node ui_img(const std::string &name) {
    node k = uiko[name];
    return k ? k : ui[name];
}
} // namespace nx
} // namespace nl
