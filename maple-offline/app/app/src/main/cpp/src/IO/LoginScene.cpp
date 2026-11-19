#include "LoginScene.h"
#include <cstdlib>
#include <cstdio>

#include "../Constants.h"

#include <nlnx/nx.hpp>

namespace ms {
namespace {
// Map-space y of each camera stop (centre of the 800x600 view), from MapLogin.img layout.
// Camera stops verified by sweeping the map: race select is the open sky at -1800,
// Cygnus/Explorer/Aran creation scenes are at -2400/-3000/-3600.
constexpr double STOP_Y[] = {0.0, -600.0, -1200.0, -1800.0, -2400.0, -3000.0, -3600.0};
}

void LoginScene::init() {
    if (loaded_) return;
    nl::node src = nl::nx::ui_img("MapLogin.img");
    if (!src) return;
    backgrounds_ = MapBackgrounds(src["back"]);
    tiles_objs_ = MapTilesObjs(src);
    loaded_ = true;
}

void LoginScene::set_step(Step step, bool instant) {
    init();
    active_ = true;
    target_y_ = STOP_Y[step];
#ifdef MS_DESKTOP_DEBUG
    if (const char *cy = std::getenv("MS_CAM_Y")) target_y_ = std::atof(cy);
#endif
    if (instant) cam_y_ = target_y_;
}

void LoginScene::update() {
    if (!loaded_) return;
#ifdef MS_DESKTOP_DEBUG
    if (std::getenv("MS_CAM_SWEEP")) {
        static int ticks = 0;
        if (++ticks % 300 == 0) { target_y_ -= 300.0; cam_y_ = target_y_; std::fprintf(stderr, "[cam] %.0f\n", cam_y_); }
    }
#endif
    double d = target_y_ - cam_y_;
    if (d > 1.0 || d < -1.0) cam_y_ += d * 0.12;
    else cam_y_ = target_y_;
    backgrounds_.update();
    tiles_objs_.update();
}

Point<int16_t> LoginScene::to_screen(Point<int16_t> mappos) const {
    int16_t vw = Constants::Constants::get().get_viewwidth();
    int16_t vh = Constants::Constants::get().get_viewheight();
    return Point<int16_t>(static_cast<int16_t>(vw / 2 + mappos.x()),
                          static_cast<int16_t>(vh / 2 + mappos.y() - cam_y_));
}

Point<int16_t> LoginScene::frame_origin() const {
    int16_t vw = Constants::Constants::get().get_viewwidth();
    int16_t vh = Constants::Constants::get().get_viewheight();
    return Point<int16_t>(static_cast<int16_t>((vw - 800) / 2), static_cast<int16_t>((vh - 600) / 2));
}

void LoginScene::draw(float alpha) const {
    if (!loaded_ || !active_) return;
    int16_t vw = Constants::Constants::get().get_viewwidth();
    int16_t vh = Constants::Constants::get().get_viewheight();
    double viewx = vw / 2.0;
    double viewy = vh / 2.0 - cam_y_;
    backgrounds_.drawbackgrounds(viewx, viewy, alpha);
    Point<int16_t> viewpos(static_cast<int16_t>(viewx), static_cast<int16_t>(viewy));
    for (auto id : Layer::IDs) {
        tiles_objs_.draw(id, viewpos, alpha);
    }
    backgrounds_.drawforegrounds(viewx, viewy, alpha);
}
}  // namespace ms
