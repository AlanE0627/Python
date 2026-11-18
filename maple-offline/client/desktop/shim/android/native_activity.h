#pragma once
#include <android/asset_manager.h>
struct ANativeActivity {
    void *callbacks = nullptr; void *vm = nullptr; void *env = nullptr; void *clazz = nullptr;
    const char *internalDataPath = nullptr; const char *externalDataPath = nullptr;
    int sdkVersion = 31; void *instance = nullptr;
    AAssetManager *assetManager = nullptr; const char *obbPath = nullptr;
};
static inline void ANativeActivity_setWindowFlags(ANativeActivity *, unsigned, unsigned) {}
