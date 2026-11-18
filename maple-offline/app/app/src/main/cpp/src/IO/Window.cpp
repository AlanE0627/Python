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
#include "Window.h"

#include "../Util/StringHandling.h"
#include "../Configuration.h"
#include "../Constants.h"
#include "../Graphics/GraphicsGL.h"
#include "../Timer.h"
#include "UI.h"
#include "../Gameplay/Stage.h"
#include <cstdlib>
#include "stb/stb_image.h"
#include "GLES3/gl32.h"
#include "gl.h"
#include "Char.h"
#include "MapPortals.h"
#include "Singleton.h"
#include "Stage.h"

#include <utility>
#include <android/log.h>

namespace ms {
    Window::Window() :
            display(nullptr),
            fullscreen_(true),
            opacity_(1.0f),
            opc_step_(0.0f),
            ratio_x(0.0f),
            ratio_y(0.0f),
            width_(Constants::Constants::get().get_viewwidth()),
            height_(Constants::Constants::get().get_viewheight()) {
    }

    Window::~Window() {
        display = nullptr;
//        glfmTerminate();
    }

    void error_callback(int no, const char *description) {
        std::cout << "GLFW error [" << no << "]: " << description << std::endl;
    }

    bool key_callback(GLFMDisplay *display, GLFMKeyCode keyCode, GLFMKeyAction action,
                      int modifiers) {
        std::cout << "key_callback" << keyCode << " action " << action << std::endl;
#ifdef MS_DESKTOP_DEBUG
        // test hook: F11 casts the skill in MS_DEBUG_SKILL
        if (keyCode == GLFMKeyCodeF11 && action == GLFMKeyActionPressed) {
            if (const char *dbg = std::getenv("MS_DEBUG_SKILL")) {
                Stage::get().get_combat().use_move(std::atoi(dbg));
                return true;
            }
        }
#endif
        UI::get().send_key(keyCode, action != GLFMKeyActionReleased);
        return true;
    }

    void char_callback(GLFMDisplay *display, const char *string, int modifiers) {
        // maple-offline: decode UTF-8 ourselves; mbstowcs depends on the C
        // locale and drops Hangul from the Android keyboard
        for (uint32_t unicode_value: string_conversion::to_utf8_vector(string)) {
            UI::get().send_key(unicode_value);
        }
    }

    std::chrono::time_point<std::chrono::steady_clock> start =
            ContinuousTimer::get().start();

    std::chrono::time_point<std::chrono::steady_clock> click_start_time;

    bool sent = false;
    double touch_start_x = 0;
    double touch_start_y = 0;

    bool mousekey_callback(GLFMDisplay *display, int touch, GLFMTouchPhase phase,
                           double x, double y) {
        Point<double_t> relative_pos = Point<double_t>(x / Window::get().get_ratio_x(),
                                                       y / Window::get().get_ratio_y());
        UI::get().set_touch_phase(touch, TouchInfo(relative_pos, phase));
        if (UI::get().should_send_cursor()) {
            Window::get().move_cursor(x, y);
            switch (phase) {
                case GLFMTouchPhaseHover:
                    break;
                case GLFMTouchPhaseBegan: {
                    // maple-offline: a touch becomes a press either when it
                    // ends (tap) or once it moves / is held; the press is
                    // sent at the touch-down point so drags start on the
                    // icon that was touched
                    click_start_time = std::chrono::steady_clock::now();
                    touch_start_x = x;
                    touch_start_y = y;
                    sent = false;
                    UI::get().send_cursor(false);
                    break;
                }
                case GLFMTouchPhaseMoved: {
                    auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - click_start_time).count();
                    double dist = std::abs(x - touch_start_x) + std::abs(y - touch_start_y);

                    if (!sent && (dist > 12.0 || diff_ms > 300)) {
                        Window::get().move_cursor(touch_start_x, touch_start_y);
                        UI::get().send_cursor(true);
                        Window::get().move_cursor(x, y);
                        sent = true;
                    }

                    UI::get().send_cursor(sent);
                    break;
                }
                case GLFMTouchPhaseEnded: {
                    if (!sent) {
                        UI::get().send_cursor(true);
                    }

                    sent = false;
                    auto diff_ms = ContinuousTimer::get().stop(start) / 1000;
                    start = ContinuousTimer::get().start();

                    // two taps within 0.4 s (frames can batch both taps, so
                    // no lower bound)
                    if (diff_ms < 400) {
                        UI::get().doubleclick();
                    }
                    UI::get().send_cursor(false);
                    break;
                }
                case GLFMTouchPhaseCancelled:
                    break;
            }
        }

        return true;
    }

    void Window::move_cursor(double x, double y) {
        auto xpos = static_cast<int16_t>(x / Window::get().get_ratio_x());
        auto ypos = static_cast<int16_t>(y / Window::get().get_ratio_y());
        Point<int16_t> pos = Point<int16_t>(xpos, ypos);
        UI::get().send_cursor(pos);
    }

    void focus_callback(GLFMDisplay *display, bool focused) {
        UI::get().send_focus(focused);

        // maple-offline: Android may kill a backgrounded app without running
        // destructors; keep client settings on disk.
        if (!focused) {
            Configuration::get().save();
        }
    }

    void scroll_callback(GLFMDisplay *display, double xoffset, double yoffset) {
        UI::get().send_scroll(yoffset);
    }

    bool wheel_callback(GLFMDisplay *display, double x, double y,
                        GLFMMouseWheelDeltaType type,
                        double dx, double dy, double dz) {
        UI::get().send_scroll(dy);

        return true;
    }

    void close_callback(GLFMDisplay *display) {
        UI::get().send_close();
    }

    void onDraw(GLFMDisplay *display) {
        if (ms::program_ == 0) {
            program_ = glCreateProgram();
            if (Error gerr = GraphicsGL::get().init(display)) {
                std::cerr << "GraphicsGL init failed: " << gerr.get_message() << " / "
                          << gerr.get_details() << std::endl;
            }
            // maple-offline: v83 native logical height is 600; width follows the screen aspect.
            {
                int sw, sh;
                glfmGetDisplaySize(display, &sw, &sh);
                int16_t vw = static_cast<int16_t>(600.0 * sw / (sh > 0 ? sh : 1));
                Constants::Constants::get().set_viewwidth(vw);
                Constants::Constants::get().set_viewheight(600);
                Setting<Width>::get().save(vw);
                Setting<Height>::get().save(600);
            }
            GraphicsGL::get().reinit();
            Char::init();
            DamageNumber::init();
            MapPortals::init();
            Stage::get().init();
            UI::get().init();
            Timer::get().start();

            int width, height;
            glfmGetDisplaySize(display, &width, &height);
            Window::get().set_ratio(width / (double) Constants::Constants::get().get_viewwidth(), height / 600.0);
        }
        // maple-offline: viewport must always match the real drawable, not the 1366x768 default.
        {
            int dw, dh;
            glfmGetDisplaySize(display, &dw, &dh);
            static int last_w = -1, last_h = -1;
            static int16_t last_vw = -1, last_vh = -1;
            int16_t cvw = Constants::Constants::get().get_viewwidth();
            int16_t cvh = Constants::Constants::get().get_viewheight();
            bool view_changed = cvw != last_vw || cvh != last_vh;
            if (view_changed) {
                last_vw = cvw;
                last_vh = cvh;
                GraphicsGL::get().reinit();
            }
            if (view_changed || dw != last_w || dh != last_h) {
                last_w = dw;
                last_h = dh;
                Window::get().set_ratio(dw / (double) Constants::Constants::get().get_viewwidth(),
                                        dh / (double) Constants::Constants::get().get_viewheight());
            }
            glViewport(0, 0, dw, dh);
        }
        const int64_t timestep = Constants::TIMESTEP * 1000;
        int64_t accumulator = timestep;
        if (game.is_running()) {
            int64_t elapsed = Timer::get().stop();

            // Update game with constant timestep as many times as possible.
            for (accumulator += elapsed; accumulator >= timestep;
                 accumulator -= timestep) {
                game.update();
            }
            float alpha = static_cast<float>(accumulator) / timestep;
            game.draw(alpha);
        }
    }

    Error Window::init(GLFMDisplay *pApp) {
        display = pApp;
        glfmSetDisplayConfig(display,
                             GLFMRenderingAPIOpenGLES32,
                             GLFMColorFormatRGBA8888,
                             GLFMDepthFormatNone,
                             GLFMStencilFormatNone,
                             GLFMMultisampleNone);
        glfmSetMultitouchEnabled(display, true);
        return init_window();
    }

    Error Window::init_window() {
        glViewport(0, 0, width_, height_);

        glfmSetMouseCursor(display, GLFMMouseCursorDefault);
        glfmSetKeyFunc(display, key_callback);
        glfmSetCharFunc(display, char_callback);
        glfmSetRenderFunc(display, onDraw);
        glfmSetTouchFunc(display, mousekey_callback);
        glfmSetMouseWheelFunc(display, wheel_callback);
        glfmSetAppFocusFunc(display, focus_callback);
//        glfwSetScrollCallback(pApp, scroll_callback);
        glfmSetSurfaceDestroyedFunc(display, close_callback);

        return Error::Code::NONE;
    }

    bool Window::not_closed() const {
//        return glfwWindowShouldClose(glwnd_) == 0;
        return true;
    }

    void Window::update() {
        update_opc();
    }

    void Window::update_opc() {
        if (opc_step_ != 0.0f) {
            opacity_ += opc_step_;

            if (opacity_ >= 1.0f) {
                opacity_ = 1.0f;
                opc_step_ = 0.0f;
            } else if (opacity_ <= 0.0f) {
                opacity_ = 0.0f;
                opc_step_ = -opc_step_;

                fade_procedure_();
            }
        }
    }

    void Window::check_events() {
        int16_t max_width = Configuration::get().get_max_width();
        int16_t max_height = Configuration::get().get_max_height();
        int16_t new_width = Constants::Constants::get().get_viewwidth();
        int16_t new_height = Constants::Constants::get().get_viewheight();

        if (width_ != new_width || height_ != new_height) {
            width_ = new_width;
            height_ = new_height;

            if (new_width >= max_width || new_height >= max_height) {
                fullscreen_ = true;
            }

//            init_window();
        }

//        glfwPollEvents();
    }

    void Window::begin() const {
        GraphicsGL::get().clear_scene();
    }

    void Window::end() const {
        GraphicsGL::get().flush(opacity_);
        glfmSwapBuffers(display);
    }

    void Window::fadeout(float step, std::function<void()> fadeproc) {
        opc_step_ = -step;
        fade_procedure_ = std::move(fadeproc);
    }

    void Window::set_clipboard(const std::string &text) const {
//        glfwSetClipboardString(glwnd_, text.c_str());
    }

    std::string Window::get_clipboard() const {
//        const char *text = glfwGetClipboardString(glwnd_);

//        return text != nullptr ? text : "";
        return "";
    }

    void Window::toggle_fullscreen() {
        int16_t max_width = Configuration::get().get_max_width();
        int16_t max_height = Configuration::get().get_max_height();

        if (width_ < max_width && height_ < max_height) {
            fullscreen_ = !fullscreen_;
            Setting<Fullscreen>::get().save(fullscreen_);

//            init_window();
//            glfwPollEvents();
        }
    }

    double Window::get_ratio_x() {
        return ratio_x;
    }

    double Window::get_ratio_y() {
        return ratio_y;
    }

    void Window::set_ratio(double ratio_x, double ratio_y) {
        this->ratio_x = ratio_x;
        this->ratio_y = ratio_y;
    }

    GLFMDisplay *Window::get_display() {
        return display;
    }
}  // namespace ms