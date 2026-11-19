#include "ImeBridge.h"

#include <mutex>
#include <optional>

#include "UI.h"
#include "Window.h"
#include "Components/Textfield.h"

#ifdef __ANDROID__
#include <android/native_activity.h>
#include <jni.h>
#include "glfm.h"
#endif

namespace ms::ImeBridge {
namespace {
std::mutex lock_;
std::optional<std::string> pending_text_;
bool pending_return_ = false;
bool shown_ = false;
std::string synced_;   // text the Java EditText holds, as far as we know

#ifdef __ANDROID__
void call_activity(const char *method, const char *sig, const std::string *arg) {
    auto *activity = static_cast<ANativeActivity *>(
            glfmGetAndroidActivity(Window::get().get_display()));
    if (!activity) return;

    JNIEnv *env = nullptr;
    bool attached = false;
    if (activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }

    jclass cls = env->GetObjectClass(activity->clazz);
    jmethodID mid = env->GetMethodID(cls, method, sig);
    if (mid) {
        if (arg) {
            jstring js = env->NewStringUTF(arg->c_str());
            env->CallVoidMethod(activity->clazz, mid, js);
            env->DeleteLocalRef(js);
        } else {
            env->CallVoidMethod(activity->clazz, mid);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);

    if (attached) activity->vm->DetachCurrentThread();
}
#endif
}  // namespace

void show(const std::string &current_text) {
    shown_ = true;
    synced_ = current_text;
#ifdef __ANDROID__
    call_activity("showTextInput", "(Ljava/lang/String;)V", &current_text);
#endif
}

void hide() {
    if (!shown_) return;
    shown_ = false;
#ifdef __ANDROID__
    call_activity("hideTextInput", "()V", nullptr);
#endif
}

void poll() {
    std::optional<std::string> text;
    bool ret = false;
    {
        std::lock_guard<std::mutex> g(lock_);
        text.swap(pending_text_);
        ret = pending_return_;
        pending_return_ = false;
    }

    Textfield *tf = UI::get().get_focused_textfield();

    if (!tf) {
        hide();
        return;
    }

    if (text) {
        synced_ = *text;
        if (*text != tf->get_text()) {
            tf->change_text(*text);
        }
    } else if (shown_ && tf->get_text() != synced_) {
        // the field changed on the game side (hardware Backspace, a cleared
        // field, a different field focused): put the same text in the
        // EditText, or the next keyboard input brings the old text back
        synced_ = tf->get_text();
#ifdef __ANDROID__
        call_activity("syncTextInput", "(Ljava/lang/String;)V", &synced_);
#endif
    }

    if (ret) {
        tf->send_key(KeyType::Id::ACTION, KeyAction::Id::RETURN, true);
    }
}

void quit_app() {
#ifdef __ANDROID__
    call_activity("quitGame", "()V", nullptr);
#endif
}

void receive_text(const std::string &text) {
    std::lock_guard<std::mutex> g(lock_);
    pending_text_ = text;
}

void receive_return() {
    std::lock_guard<std::mutex> g(lock_);
    pending_return_ = true;
}
}  // namespace ms::ImeBridge

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL
Java_openMapleClient_MapleActivity_nativeSetText(JNIEnv *env, jobject, jstring text) {
    const char *s = env->GetStringUTFChars(text, nullptr);
    ms::ImeBridge::receive_text(s ? s : "");
    if (s) env->ReleaseStringUTFChars(text, s);
}

extern "C" JNIEXPORT void JNICALL
Java_openMapleClient_MapleActivity_nativeReturn(JNIEnv *, jobject) {
    ms::ImeBridge::receive_return();
}
#endif
