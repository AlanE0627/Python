#pragma once
#include <cstdio>
#include <cstdarg>
enum { ANDROID_LOG_VERBOSE = 2, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL };
static inline int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); std::fprintf(stderr, "[%d/%s] ", prio, tag); int r = std::vfprintf(stderr, fmt, ap); std::fputc('\n', stderr); va_end(ap); return r;
}
static inline int __android_log_write(int prio, const char *tag, const char *text) { return __android_log_print(prio, tag, "%s", text); }
