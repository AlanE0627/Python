// JNI bridge for first-run asset setup (called from com.mapleserver.AssetSetup).
#include "setup.h"

#include <jni.h>
#include <string>
#include <vector>

int wztonx_main(int argc, char **argv);

namespace {
std::string str(JNIEnv *env, jstring s) {
    const char *c = env->GetStringUTFChars(s, nullptr);
    std::string r(c);
    env->ReleaseStringUTFChars(s, c);
    return r;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_mapleserver_AssetSetup_nativeExtract(JNIEnv *env, jobject self, jstring setupExe, jstring outDir) {
    jclass cls = env->GetObjectClass(self);
    jmethodID onProgress = env->GetMethodID(cls, "onProgress", "(Ljava/lang/String;JJ)V");
    std::string err;
    bool ok = msetup::extract_installer(str(env, setupExe), str(env, outDir),
        [&](const char *stage, uint64_t done, uint64_t total) {
            jstring s = env->NewStringUTF(stage);
            env->CallVoidMethod(self, onProgress, s, (jlong) done, (jlong) total);
            env->DeleteLocalRef(s);
        }, err);
    return ok ? nullptr : env->NewStringUTF(err.c_str());
}

// Converts one .wz file to .nx (client format) next to it. Returns null on success.
extern "C" JNIEXPORT jstring JNICALL
Java_com_mapleserver_AssetSetup_nativeConvert(JNIEnv *env, jobject, jstring wzPath) {
    std::string path = str(env, wzPath);
    std::string a0 = "wztonx", a2 = "-c";
    std::vector<char *> argv = {a0.data(), path.data(), a2.data()};
    try {
        int r = wztonx_main((int) argv.size(), argv.data());
        if (r != 0) return env->NewStringUTF("converter returned error");
    } catch (const std::exception &e) {
        return env->NewStringUTF(e.what());
    }
    return nullptr;
}
