// maple-offline: the v83 login screens are one scrolling map (UI.wz/MapLogin.img).
// Title, world select, character select and creation are camera stops on that map.
#pragma once

#include "../Gameplay/MapleMap/MapBackgrounds.h"
#include "../Gameplay/MapleMap/MapTilesObjs.h"
#include "../Template/Point.h"
#include "../Template/Singleton.h"

namespace ms {
class LoginScene : public Singleton<LoginScene> {
public:
    enum Step { TITLE = 0, WORLD_SELECT = 1, CHAR_SELECT = 2, NEW_CHAR = 3, RACE_SELECT = 4 };

    void init();
    void set_step(Step step, bool instant = false);
    void update();
    void draw(float alpha) const;

    // Screen position of a map point at the current camera (for UI placement).
    Point<int16_t> to_screen(Point<int16_t> mappos) const;
    // Top-left of the 800x600 v83 login frame on the current screen.
    Point<int16_t> frame_origin() const;

private:
    bool loaded_ = false;
    bool active_ = false;
    MapBackgrounds backgrounds_;
    MapTilesObjs tiles_objs_;
    double cam_y_ = 0.0, target_y_ = 0.0;
};
}  // namespace ms
