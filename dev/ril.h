#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define WRITE_TO_DEBUGGER ::OutputDebugStringA
#else
#define WRITE_TO_DEBUGGER(...) void(0)
#endif

#define RAPID_IMAGE_LOGE(...)                                                                              \
    do {                                                                                                   \
        auto message___ = ril::format("[ ERROR ] (#%d) %s\n", __LINE__, ril::format(__VA_ARGS__).c_str()); \
        fprintf(stderr, "%s", message___.c_str());                                                         \
        WRITE_TO_DEBUGGER(message___.c_str());                                                             \
    } while (false)

#define RAPID_IMAGE_LOGW(...)                                                                              \
    do {                                                                                                   \
        auto message___ = ril::format("[WARNING] (#%d) %s\n", __LINE__, ril::format(__VA_ARGS__).c_str()); \
        fprintf(stderr, "%s", message___.c_str());                                                         \
        WRITE_TO_DEBUGGER(message___.c_str());                                                             \
    } while (false)

#include <rapid-image/rapid-image.h>
