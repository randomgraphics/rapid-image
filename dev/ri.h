#pragma once
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#define RAPID_IMAGE_LOG_ERROR(...)                                                                         \
    do {                                                                                                   \
        auto message___ = rapid_image::format("[  ERROR] %s\n", rapid_image::format(__VA_ARGS__).c_str()); \
        fprintf(stderr, message___.c_str());                                                               \
        ::OutputDebugStringA(message___.c_str());                                                          \
    } while (false)
#define RAPID_IMAGE_LOG_WARNING(...)                                                                       \
    do {                                                                                                   \
        auto message___ = rapid_image::format("[WARNING] %s\n", rapid_image::format(__VA_ARGS__).c_str()); \
        fprintf(stderr, message___.c_str());                                                               \
        ::OutputDebugStringA(message___.c_str());                                                          \
    } while (false)
#endif
#include <rapid-image/rapid-image.h>
