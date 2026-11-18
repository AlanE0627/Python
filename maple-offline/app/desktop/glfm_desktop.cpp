// Desktop (Linux/X11) implementation of the subset of the GLFM API used by the client.
// Purpose: run the Android client code unchanged on a desktop for automated testing
// (Xvfb + Mesa llvmpipe, OpenGL ES 3.2 through EGL).
#define GLFW_INCLUDE_NONE
extern "C" { int maple_gamepad_active = 0; }
#include <GLFW/glfw3.h>
#include <GLES3/gl32.h>
#include "glfm.h"
#include <android/native_activity.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct GLFMDisplay {
    GLFWwindow *win = nullptr;
    GLFMRenderFunc render = nullptr;
    GLFMTouchFunc touch = nullptr;
    GLFMKeyFunc key = nullptr;
    GLFMCharFunc chr = nullptr;
    GLFMMouseWheelFunc wheel = nullptr;
    GLFMAppFocusFunc focus = nullptr;
    GLFMSurfaceDestroyedFunc destroyed = nullptr;
    bool mouseDown = false;
    bool swapped = false;
    ANativeActivity activity;
    std::string internalPath, externalPath;
};

// ---------- AAssetManager shim: assets are plain files under MS_ASSET_DIR ----------
struct AAssetManager { std::string root; };
struct AAsset { std::vector<char> data; };
static AAssetManager g_assets;

AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int) {
    std::string path = mgr->root + "/" + filename;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) { std::fprintf(stderr, "[asset] missing %s\n", path.c_str()); return nullptr; }
    auto *a = new AAsset();
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    a->data.resize(n > 0 ? n : 0);
    if (n > 0 && std::fread(a->data.data(), 1, n, f) != (size_t) n) a->data.clear();
    std::fclose(f);
    return a;
}
off_t AAsset_getLength(AAsset *a) { return a ? (off_t) a->data.size() : 0; }
int AAsset_read(AAsset *a, void *buf, size_t count) {
    if (!a) return -1;
    size_t n = count < a->data.size() ? count : a->data.size();
    std::memcpy(buf, a->data.data(), n);
    return (int) n;
}
void AAsset_close(AAsset *a) { delete a; }

// ---------- key mapping GLFW -> GLFMKeyCode ----------
static GLFMKeyCode map_key(int k) {
    if (k >= GLFW_KEY_A && k <= GLFW_KEY_Z) return (GLFMKeyCode) (GLFMKeyCodeA + (k - GLFW_KEY_A));
    if (k >= GLFW_KEY_0 && k <= GLFW_KEY_9) return (GLFMKeyCode) (GLFMKeyCode0 + (k - GLFW_KEY_0));
    if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F9) return (GLFMKeyCode) (GLFMKeyCodeF1 + (k - GLFW_KEY_F1));
    if (k >= GLFW_KEY_F10 && k <= GLFW_KEY_F12) return (GLFMKeyCode) (GLFMKeyCodeF10 + (k - GLFW_KEY_F10));
    switch (k) {
        case GLFW_KEY_BACKSPACE: return GLFMKeyCodeBackspace;
        case GLFW_KEY_TAB: return GLFMKeyCodeTab;
        case GLFW_KEY_ENTER: return GLFMKeyCodeEnter;
        case GLFW_KEY_ESCAPE: return GLFMKeyCodeEscape;
        case GLFW_KEY_SPACE: return GLFMKeyCodeSpace;
        case GLFW_KEY_APOSTROPHE: return GLFMKeyCodeQuote;
        case GLFW_KEY_COMMA: return GLFMKeyCodeComma;
        case GLFW_KEY_MINUS: return GLFMKeyCodeMinus;
        case GLFW_KEY_PERIOD: return GLFMKeyCodePeriod;
        case GLFW_KEY_SLASH: return GLFMKeyCodeSlash;
        case GLFW_KEY_SEMICOLON: return GLFMKeyCodeSemicolon;
        case GLFW_KEY_EQUAL: return GLFMKeyCodeEqual;
        case GLFW_KEY_LEFT_BRACKET: return GLFMKeyCodeBracketLeft;
        case GLFW_KEY_BACKSLASH: return GLFMKeyCodeBackslash;
        case GLFW_KEY_RIGHT_BRACKET: return GLFMKeyCodeBracketRight;
        case GLFW_KEY_GRAVE_ACCENT: return GLFMKeyCodeBackquote;
        case GLFW_KEY_DELETE: return GLFMKeyCodeDelete;
        case GLFW_KEY_CAPS_LOCK: return GLFMKeyCodeCapsLock;
        case GLFW_KEY_LEFT_SHIFT: return GLFMKeyCodeShiftLeft;
        case GLFW_KEY_RIGHT_SHIFT: return GLFMKeyCodeShiftRight;
        case GLFW_KEY_LEFT_CONTROL: return GLFMKeyCodeControlLeft;
        case GLFW_KEY_RIGHT_CONTROL: return GLFMKeyCodeControlRight;
        case GLFW_KEY_LEFT_ALT: return GLFMKeyCodeAltLeft;
        case GLFW_KEY_RIGHT_ALT: return GLFMKeyCodeAltRight;
        case GLFW_KEY_INSERT: return GLFMKeyCodeInsert;
        case GLFW_KEY_PAGE_UP: return GLFMKeyCodePageUp;
        case GLFW_KEY_PAGE_DOWN: return GLFMKeyCodePageDown;
        case GLFW_KEY_END: return GLFMKeyCodeEnd;
        case GLFW_KEY_HOME: return GLFMKeyCodeHome;
        case GLFW_KEY_LEFT: return GLFMKeyCodeArrowLeft;
        case GLFW_KEY_UP: return GLFMKeyCodeArrowUp;
        case GLFW_KEY_RIGHT: return GLFMKeyCodeArrowRight;
        case GLFW_KEY_DOWN: return GLFMKeyCodeArrowDown;
        default: return GLFMKeyCodeUnknown;
    }
}
static int map_mods(int m) {
    int r = 0;
    if (m & GLFW_MOD_SHIFT) r |= GLFMKeyModifierShift;
    if (m & GLFW_MOD_CONTROL) r |= GLFMKeyModifierControl;
    if (m & GLFW_MOD_ALT) r |= GLFMKeyModifierAlt;
    return r;
}

static GLFMDisplay *D(GLFWwindow *w) { return (GLFMDisplay *) glfwGetWindowUserPointer(w); }

static void cb_key(GLFWwindow *w, int key, int, int action, int mods) {
    GLFMDisplay *d = D(w);
    if (!d->key) return;
    GLFMKeyAction a = action == GLFW_PRESS ? GLFMKeyActionPressed
                    : action == GLFW_REPEAT ? GLFMKeyActionRepeated : GLFMKeyActionReleased;
    d->key(d, map_key(key), a, map_mods(mods));
}
static void cb_char(GLFWwindow *w, unsigned int cp) {
    GLFMDisplay *d = D(w);
    if (!d->chr) return;
    char s[5] = {0};
    if (cp < 0x80) s[0] = (char) cp;
    else if (cp < 0x800) { s[0] = (char) (0xC0 | (cp >> 6)); s[1] = (char) (0x80 | (cp & 0x3F)); }
    else { s[0] = (char) (0xE0 | (cp >> 12)); s[1] = (char) (0x80 | ((cp >> 6) & 0x3F)); s[2] = (char) (0x80 | (cp & 0x3F)); }
    d->chr(d, s, 0);
}
static void cb_button(GLFWwindow *w, int button, int action, int) {
    GLFMDisplay *d = D(w);
    if (button != GLFW_MOUSE_BUTTON_LEFT || !d->touch) return;
    double x, y; glfwGetCursorPos(w, &x, &y);
    d->mouseDown = action == GLFW_PRESS;
    d->touch(d, 0, d->mouseDown ? GLFMTouchPhaseBegan : GLFMTouchPhaseEnded, x, y);
}
static void cb_cursor(GLFWwindow *w, double x, double y) {
    GLFMDisplay *d = D(w);
    if (d->touch) d->touch(d, 0, d->mouseDown ? GLFMTouchPhaseMoved : GLFMTouchPhaseHover, x, y);
}
static void cb_scroll(GLFWwindow *w, double dx, double dy) {
    GLFMDisplay *d = D(w);
    double x, y; glfwGetCursorPos(w, &x, &y);
    if (d->wheel) d->wheel(d, x, y, GLFMMouseWheelDeltaPixel, dx, dy, 0);
}
static void cb_focus(GLFWwindow *w, int f) { GLFMDisplay *d = D(w); if (d->focus) d->focus(d, f != 0); }

// ---------- GLFM API subset ----------
void *glfmGetAndroidActivity(const GLFMDisplay *d) { return (void *) &d->activity; }
void glfmGetDisplaySize(const GLFMDisplay *d, int *w, int *h) { glfwGetFramebufferSize(d->win, w, h); }
GLFMAppFocusFunc glfmSetAppFocusFunc(GLFMDisplay *d, GLFMAppFocusFunc f) { auto o = d->focus; d->focus = f; return o; }
GLFMCharFunc glfmSetCharFunc(GLFMDisplay *d, GLFMCharFunc f) { auto o = d->chr; d->chr = f; return o; }
void glfmSetDisplayConfig(GLFMDisplay *, GLFMRenderingAPI, GLFMColorFormat, GLFMDepthFormat, GLFMStencilFormat, GLFMMultisample) {}
GLFMKeyFunc glfmSetKeyFunc(GLFMDisplay *d, GLFMKeyFunc f) { auto o = d->key; d->key = f; return o; }
void glfmSetKeyboardVisible(GLFMDisplay *, bool) {}
void glfmSetMouseCursor(GLFMDisplay *, GLFMMouseCursor) {}
GLFMMouseWheelFunc glfmSetMouseWheelFunc(GLFMDisplay *d, GLFMMouseWheelFunc f) { auto o = d->wheel; d->wheel = f; return o; }
void glfmSetMultitouchEnabled(GLFMDisplay *, bool) {}
GLFMRenderFunc glfmSetRenderFunc(GLFMDisplay *d, GLFMRenderFunc f) { auto o = d->render; d->render = f; return o; }
GLFMSurfaceDestroyedFunc glfmSetSurfaceDestroyedFunc(GLFMDisplay *d, GLFMSurfaceDestroyedFunc f) { auto o = d->destroyed; d->destroyed = f; return o; }
GLFMTouchFunc glfmSetTouchFunc(GLFMDisplay *d, GLFMTouchFunc f) { auto o = d->touch; d->touch = f; return o; }
void glfmSwapBuffers(GLFMDisplay *d) { glfwSwapBuffers(d->win); d->swapped = true; }

extern void glfmMain(GLFMDisplay *display);

int main() {
    const char *data = std::getenv("MS_DATA_DIR");
    const char *assets = std::getenv("MS_ASSET_DIR");
    if (std::getenv("MS_HIDE_TOUCH")) maple_gamepad_active = 1;
    const char *ws = std::getenv("MS_WIDTH");
    const char *hs = std::getenv("MS_HEIGHT");
    int width = ws ? std::atoi(ws) : 1280, height = hs ? std::atoi(hs) : 720;

    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    GLFMDisplay display;
    display.win = glfwCreateWindow(width, height, "maple-offline desktop", nullptr, nullptr);
    if (!display.win) { std::fprintf(stderr, "glfwCreateWindow failed\n"); return 1; }
    glfwMakeContextCurrent(display.win);
    glfwSwapInterval(0);
    glfwSetWindowUserPointer(display.win, &display);
    glfwSetKeyCallback(display.win, cb_key);
    glfwSetCharCallback(display.win, cb_char);
    glfwSetMouseButtonCallback(display.win, cb_button);
    glfwSetCursorPosCallback(display.win, cb_cursor);
    glfwSetScrollCallback(display.win, cb_scroll);
    glfwSetWindowFocusCallback(display.win, cb_focus);

    display.internalPath = data ? data : ".";
    display.externalPath = display.internalPath;
    display.activity.internalDataPath = display.internalPath.c_str();
    display.activity.externalDataPath = display.externalPath.c_str();
    g_assets.root = assets ? assets : ".";
    display.activity.assetManager = &g_assets;

    std::fprintf(stderr, "GL_RENDERER=%s GL_VERSION=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glfmMain(&display);

    while (!glfwWindowShouldClose(display.win)) {
        glfwPollEvents();
        display.swapped = false;
        if (display.render) display.render(&display);
        if (!display.swapped) glfwSwapBuffers(display.win);
    }
    if (display.destroyed) display.destroyed(&display);
    glfwTerminate();
    return 0;
}
