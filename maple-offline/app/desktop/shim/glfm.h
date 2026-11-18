// Desktop wrapper: present the Android flavour of glfm.h on Linux.
#pragma once
#include <android/native_activity.h>
#define __ANDROID__ 1
#include "../../app/src/main/cpp/thirdparty/glfm/include/glfm.h"
#undef __ANDROID__
