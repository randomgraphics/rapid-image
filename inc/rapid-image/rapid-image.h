/*
MIT License

Copyright (c) 2023 randomgraphics

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wc++98-compat"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#endif

#ifndef RAPID_IMAGE_H_
#define RAPID_IMAGE_H_

// ---------------------------------------------------------------------------------------------------------------------
// User configurable macros

/// A monotonically increasing number that uniquely identify the revision of the header.
#define RAPID_IMAGE_HEADER_REVISION 2

/// \def RAPID_IMAGE_NAMESPACE
/// Define the namespace of rapid-image library. Default value is ril, standing for Rapid Image Library
#ifndef RAPID_IMAGE_NAMESPACE
#define RAPID_IMAGE_NAMESPACE ril
#endif

/// \def RAPID_IMAGE_ENABLE_DEBUG_BUILD
/// Set to non-zero value to enable debug build. Disabled by default.
#ifndef RAPID_IMAGE_ENABLE_DEBUG_BUILD
#define RAPID_IMAGE_ENABLE_DEBUG_BUILD 0
#endif

/// \def RAPID_IMAGE_THROW
/// The macro to throw runtime exception.
#ifndef RAPID_IMAGE_THROW
#define RAPID_IMAGE_THROW(...) throw std::runtime_error(__VA_ARGS__)
#endif

/// \def RAPID_IMAGE_BACKTRACE
/// Define custom function to retrieve current call stack and store in std::string.
/// This macro is called when rapid-image encounters critical error, to help
/// quickly identify the source of the error. The default implementation does
/// nothing but return empty string.
#ifndef RAPID_IMAGE_BACKTRACE
#define RAPID_IMAGE_BACKTRACE() std::string("You have to define RAPID_IMAGE_BACKTRACE to retrieve current call stack.")
#endif

/// \def RAPID_IMAGE_LOGE
/// The macro to log error message. The default implementation prints to stderr.
#ifndef RAPID_IMAGE_LOGE
#define RAPID_IMAGE_LOGE(...)          \
    do {                               \
        fprintf(stderr, "[ ERROR ] "); \
        fprintf(stderr, __VA_ARGS__);  \
        fprintf(stderr, "\n");         \
    } while (false)
#endif

/// \def RAPID_IMAGE_LOGW
/// The macro to log warning message. The default implementation prints to stderr.
#ifndef RAPID_IMAGE_LOGW
#define RAPID_IMAGE_LOGW(...)          \
    do {                               \
        fprintf(stderr, "[WARNING] "); \
        fprintf(stderr, __VA_ARGS__);  \
        fprintf(stderr, "\n");         \
    } while (false)
#endif

/// \def RAPID_IMAGE_LOGI
/// The macro to log informational message. The default implementation prints to stdout.
#ifndef RAPID_IMAGE_LOGI
#define RAPID_IMAGE_LOGI(...)         \
    do {                              \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n");        \
    } while (false)
#endif

/// \def RAPID_IMAGE_ASSERT
/// The runtime assert macro for debug build only. This macro has no effect when
/// RAPID_IMAGE_ENABLE_DEBUG_BUILD is 0.
#ifndef RAPID_IMAGE_ASSERT
#define RAPID_IMAGE_ASSERT(expression, ...)                                                          \
    if (!(expression)) {                                                                             \
        auto rapidImageAssertMessage_ = RAPID_IMAGE_NAMESPACE::rii_details::format(__VA_ARGS__);     \
        RAPID_IMAGE_LOGE("Condition %s not met. %s", #expression, rapidImageAssertMessage_.c_str()); \
        assert(false);                                                                               \
    } else                                                                                           \
        void(0)
#endif

// ---------------------------------------------------------------------------------------------------------------------
// include system headers.

#include <cassert>
#include <cstdint>
#include <cmath>
#include <cstdarg>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <exception>
#include <memory>

// ---------------------------------------------------------------------------------------------------------------------
// internal macros

#if defined(_PPC_) || defined(__BIG_ENDIAN__)
#define RII_LITTLE_ENDIAN 0
#define RII_BIG_ENDIAN    1
#else
#define RII_LITTLE_ENDIAN 1
#define RII_BIG_ENDIAN    0
#endif

#define RII_NO_COPY(T)                 \
    T(const T &)             = delete; \
    T & operator=(const T &) = delete

#define RII_NO_MOVE(T)            \
    T(T &&)             = delete; \
    T & operator=(T &&) = delete

#define RII_NO_COPY_NO_MOVE(T) RII_NO_COPY(T) RII_NO_MOVE(T)

#define RII_STR(x) RII_STR_HELPER(x)

#define RII_STR_HELPER(x) #x

#define RII_THROW(...)                                                                                            \
    do {                                                                                                          \
        std::stringstream riiThrowStrStream_;                                                                     \
        riiThrowStrStream_ << __FILE__ << "(" << __LINE__ << "): " << RAPID_IMAGE_NAMESPACE::rii_details::format(__VA_ARGS__); \
        RAPID_IMAGE_LOGE("%s", riiThrowStrStream_.str().data());                                                  \
        RAPID_IMAGE_THROW(riiThrowStrStream_.str());                                                              \
    } while (false)

#if RAPID_IMAGE_ENABLE_DEBUG_BUILD
// assert is enabled only in debug build
#define RII_ASSERT RAPID_IMAGE_ASSERT
#else
#define RII_ASSERT(...) ((void) 0)
#endif

#define RII_REQUIRE(condition, ...)                                                             \
    do {                                                                                        \
        if (!(condition)) {                                                                     \
            auto riiRequireErrorMessage_ = RAPID_IMAGE_NAMESPACE::rii_details::format(__VA_ARGS__); \
            RII_THROW("Condition " #condition " not met: %s", riiRequireErrorMessage_.c_str()); \
        }                                                                                       \
    } while (false)

// ---------------------------------------------------------------------------------------------------------------------
// beginning of rapid-image namespace

namespace RAPID_IMAGE_NAMESPACE {

using namespace std::string_literals;


namespace rii_details {

// ---------------------------------------------------------------------------------------------------------------------
/// \def format
/// \brief A printf like string formatting function.
#if __clang__
__attribute__((format(printf, 1, 2)))
#endif
std::string format(const char * format, ...);

/// Overload of format() function for empty parameter list.
inline std::string format() { return ""s; }

// ---------------------------------------------------------------------------------------------------------------------
/// \brief Allocate aligned memory. The memory must be freed with afree(). Calling free() on the returned pointer
/// will cause undefined behavior.
void * aalloc(size_t a, size_t s);

// ---------------------------------------------------------------------------------------------------------------------
// \brief Free memory allocated by aalloc().
void afree(void * p);

}

// ---------------------------------------------------------------------------------------------------------------------
/// @brief A convenience class that we use to hold one uncompressed pixel of any format
union OnePixel {
    struct {
        uint64_t lo;
        uint64_t hi;
    };
    uint8_t  u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
    float    f32[4];

    static inline OnePixel make(const void * data, size_t size) {
        RII_ASSERT(size <= 16);
        OnePixel r   = {};
        auto     src = (const uint8_t *) data;
        auto     end = src + size;
        for (auto dst = r.u8; src < end; ++src, ++dst) { *dst = *src; }
        return r;
    }

    uint32_t segment(uint32_t offset, uint32_t count) const {
        uint64_t mask = (count < 64) ? ((((uint64_t) 1) << count) - 1) : (uint64_t) -1;
        if (offset + count <= 64) {
            return (uint32_t) ((lo >> offset) & mask);
        } else if (offset >= 64) {
            return (uint32_t) (hi >> (offset - 64) & mask);
        } else {
            // This means the segment is crossing the low and hi
            RII_THROW("unsupported yet.");
        }
    }
    void set(uint32_t value, uint32_t offset, uint32_t count) {
        uint64_t mask = (count < 64) ? ((((uint64_t) 1) << count) - 1) : (uint64_t) -1;
        if (offset + count <= 64) {
            lo |= (value & mask) << offset;
        } else if (offset >= 64) {
            hi |= (value & mask) << (offset - 64);
        } else {
            // This means the segment is crossing the low and hi
            RII_THROW("unsupported yet.");
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------

/// @brief Represents one RGBA8 pixel
union RGBA8 {
    struct {
        uint8_t x, y, z, w;
    };
    struct {
        uint8_t r, g, b, a;
    };
    uint32_t u32;
    int32_t  i32;
    uint8_t  u8[4];
    int8_t   i8[4];
    char     c8[4];

    static RGBA8 make(uint8_t r, uint8_t g = 0, uint8_t b = 0, uint8_t a = 0) { return {{r, g, b, a}}; }

    static RGBA8 make(float r, float g, float b, float a) {
        return {{static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.0f), static_cast<uint8_t>(std::clamp(g, 0.f, 1.f) * 255.0f),
                 static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.0f), static_cast<uint8_t>(std::clamp(a, 0.f, 1.f) * 255.0f)}};
    }

    static RGBA8 make(const uint8_t * p) { return {{p[0], p[1], p[2], p[3]}}; }

    static RGBA8 make(uint32_t u) {
        RGBA8 result;
        result.u32 = u;
        return result;
    }

    void set(uint8_t r_, uint8_t g_ = 0, uint8_t b_ = 0, uint8_t a_ = 0) {
        r = r_;
        g = g_;
        b = b_;
        a = a_;
    }
};
static_assert(sizeof(RGBA8) == 4, "");

/// @brief Represents one RGBA16F pixel
union Half4 {
    struct {
        uint16_t x, y, z, w;
    };
    struct {
        uint16_t r, g, b, a;
    };
    uint16_t u16[4];
    int16_t  i16[4];
    uint32_t u32[2];
    int32_t  i32[2];
    uint64_t u64;
    int64_t  i64;

    static Half4 make(uint16_t r, uint16_t g, uint16_t b, uint16_t a) { return {{r, g, b, a}}; }

    static Half4 make(const uint16_t * p) { return {{p[0], p[1], p[2], p[3]}}; }
};

/// @brief Represents one RGBA32F pixel
union Float4 {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
    float    f32[4];
    uint32_t u32[4];
    int32_t  i32[4];
    uint64_t u64[2];
    int64_t  i64[2];
    OnePixel u128;

    static Float4 make(float r, float g, float b, float a) { return {{r, g, b, a}}; }

    static Float4 make(const float * p) { return {{p[0], p[1], p[2], p[3]}}; }

    Float4 & operator+=(const Float4 & v) {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }

    Float4 & operator-=(const Float4 & v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }

    Float4 & operator*=(const Float4 & v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        w *= v.w;
        return *this;
    }

    Float4 & operator/=(const Float4 & v) {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        w /= v.w;
        return *this;
    }

    Float4 & operator*=(float v) {
        x *= v;
        y *= v;
        z *= v;
        w *= v;
        return *this;
    }
};
static_assert(sizeof(Float4) == 128 / 8, "");

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Pixel format structure
union PixelFormat {
    struct {
#if RII_LITTLE_ENDIAN
        /// @brief Defines pixel layout. See \ref Layout for details.
        /// When this field is empty, the whole pixel forma is considered empty/invalid.
        uint32_t layout    : 7;
        uint32_t reserved0 : 1; ///< reserved, must be zero.
        uint32_t sign0     : 4; ///< sign for channel 0. See \ref Sign.
        uint32_t sign12    : 4; ///< sign for channel 1 and 2. See \ref Sign.
        uint32_t sign3     : 4; ///< sign for channel 3. See \ref Sign.
        uint32_t swizzle0  : 3; ///< Swizzle for channel 0. See \ref Swizzle.
        uint32_t swizzle1  : 3; ///< Swizzle for channel 1. See \ref Swizzle.
        uint32_t swizzle2  : 3; ///< Swizzle for channel 2. See \ref Swizzle.
        uint32_t swizzle3  : 3; ///< Swizzle for channel 3. See \ref Swizzle.
#else
        uint32_t swizzle3  : 3;
        uint32_t swizzle2  : 3;
        uint32_t swizzle1  : 3;
        uint32_t swizzle0  : 3;
        uint32_t sign3     : 4;
        uint32_t sign12    : 4;
        uint32_t sign0     : 4;
        uint32_t reserved0 : 1;
        uint32_t layout    : 7;
#endif
    };

    /// @brief Pixel format represented as a single 32-bit unsigned integer
    uint32_t u32;

    /// @brief Pixel layout. Defines number of channels in the layout, and number of bits in each channel.
    ///
    /// The order of the channels is always started from the last significant bit. For example, for LAYOUT_5_5_5_1,
    /// the least significant 5 bits are for channel 0, the highest 1 bit is for channel 3. Note that the actual
    /// in-memory layout of the bits are determined by the endianness of the system.
    ///
    /// The size of each pixel could be smaller than a byte, in which case the pixel is considered packed or compressed.
    /// The examples of that are LAYOUT_1 and those DXT formats.
    enum Layout {
        LAYOUT_UNKNOWN = 0,
        LAYOUT_1,
        LAYOUT_2_2_2_2,
        LAYOUT_3_3_2,
        LAYOUT_4_4,
        LAYOUT_4_4_4_4,
        LAYOUT_5_5_5_1,
        LAYOUT_5_6_5,
        LAYOUT_8,
        LAYOUT_8_8,
        LAYOUT_8_8_8,
        LAYOUT_8_8_8_8,
        LAYOUT_10_11_11,
        LAYOUT_11_11_10,
        LAYOUT_10_10_10_2,
        LAYOUT_16,
        LAYOUT_16_16,
        LAYOUT_16_16_16_16,
        LAYOUT_32,
        LAYOUT_32_32,
        LAYOUT_32_32_32,
        LAYOUT_32_32_32_32,
        LAYOUT_24,
        LAYOUT_8_24,
        LAYOUT_24_8,
        LAYOUT_4_4_24,
        LAYOUT_32_8_24,
        LAYOUT_DXT1,
        LAYOUT_DXT2,
        LAYOUT_DXT3,
        LAYOUT_DXT3A,
        LAYOUT_DXT4,
        LAYOUT_DXT5,
        LAYOUT_DXT5A,
        LAYOUT_DXN,
        LAYOUT_CTX1,
        LAYOUT_DXT3A_AS_1_1_1_1,
        LAYOUT_GRGB,
        LAYOUT_RGBG,

        // all ASTC layout are having 4 channels: RGB + A
        FIRST_ASTC_LAYOUT,
        LAYOUT_ASTC_4x4 = FIRST_ASTC_LAYOUT,
        LAYOUT_ASTC_5x4,
        LAYOUT_ASTC_5x5,
        LAYOUT_ASTC_6x5,
        LAYOUT_ASTC_6x6,
        LAYOUT_ASTC_8x5,
        LAYOUT_ASTC_8x6,
        LAYOUT_ASTC_8x8,
        LAYOUT_ASTC_10x5,
        LAYOUT_ASTC_10x6,
        LAYOUT_ASTC_10x8,
        LAYOUT_ASTC_10x10,
        LAYOUT_ASTC_12x10,
        LAYOUT_ASTC_12x12,

        FIRST_ASTC_SFLOAT_LAYOUT,
        LAYOUT_ASTC_4x4_SFLOAT = FIRST_ASTC_SFLOAT_LAYOUT,
        LAYOUT_ASTC_5x4_SFLOAT,
        LAYOUT_ASTC_5x5_SFLOAT,
        LAYOUT_ASTC_6x5_SFLOAT,
        LAYOUT_ASTC_6x6_SFLOAT,
        LAYOUT_ASTC_8x5_SFLOAT,
        LAYOUT_ASTC_8x6_SFLOAT,
        LAYOUT_ASTC_8x8_SFLOAT,
        LAYOUT_ASTC_10x5_SFLOAT,
        LAYOUT_ASTC_10x6_SFLOAT,
        LAYOUT_ASTC_10x8_SFLOAT,
        LAYOUT_ASTC_10x10_SFLOAT,
        LAYOUT_ASTC_12x10_SFLOAT,
        LAYOUT_ASTC_12x12_SFLOAT,
        LAST_ASTC_LAYOUT = LAYOUT_ASTC_12x12_SFLOAT,

        NUM_COLOR_LAYOUTS,
    };

    /// @brief Description of one channel in a pixel layout.
    struct ChannelDesc {
        uint8_t shift; ///< number of bits to shift to the right to get the channel value.
        uint8_t bits;  ///< number of bits in the channel.
    };

    /// @brief Description of a pixel layout.
    struct LayoutDesc {
        uint8_t     blockWidth  : 4; ///< width of one pixel block. 1 means the pixel is not compressed/packed.
        uint8_t     blockHeight : 4; ///< height of one pixel block. 1 means the pixel is not compressed/packed.
        uint8_t     blockBytes;      ///< bytes of one pixel block. this should always be equal to (blockWidth * blockHeight * bitsPerPixel + 7) / 8
        uint8_t     numChannels;     ///< number of channels in the pixel layout.
        ChannelDesc channels[4];     ///< description of each channel in the pixel layout, up to 4 channels.
    };

    ///
    /// Layout descriptors. Indexed by the layout enum value.
    ///
    static constexpr LayoutDesc LAYOUTS[] = {
        // clang-format off
        //BW  BH  BB   CH      CH0        CH1          CH2          CH3
        { 0 , 0 , 0  , 0 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_UNKNOWN,
        { 8 , 1 , 1  , 1 , { { 0 , 1  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_1,
        { 1 , 1 , 1  , 4 , { { 0 , 2  }, { 2  , 2  }, { 4  , 2  }, { 6  , 2  } } }, //LAYOUT_2_2_2_2,
        { 1 , 1 , 1  , 3 , { { 0 , 3  }, { 3  , 3  }, { 8  , 2  }, { 0  , 0  } } }, //LAYOUT_3_3_2,
        { 1 , 1 , 1  , 2 , { { 0 , 4  }, { 4  , 4  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_4_4,
        { 1 , 1 , 2  , 4 , { { 0 , 4  }, { 4  , 4  }, { 8  , 4  }, { 12 , 4  } } }, //LAYOUT_4_4_4_4,
        { 1 , 1 , 2  , 4 , { { 0 , 5  }, { 10 , 5  }, { 15 , 5  }, { 15 , 1  } } }, //LAYOUT_5_5_5_1,
        { 1 , 1 , 2  , 3 , { { 0 , 5  }, { 5  , 6  }, { 11 , 5  }, { 0  , 0  } } }, //LAYOUT_5_6_5,
        { 1 , 1 , 1  , 1 , { { 0 , 8  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_8,
        { 1 , 1 , 2  , 2 , { { 0 , 8  }, { 8  , 8  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_8_8,
        { 1 , 1 , 3  , 3 , { { 0 , 8  }, { 8  , 8  }, { 16 , 8  }, { 0  , 0  } } }, //LAYOUT_8_8_8,
        { 1 , 1 , 4  , 4 , { { 0 , 8  }, { 8  , 8  }, { 16 , 8  }, { 24 , 8  } } }, //LAYOUT_8_8_8_8,
        { 1 , 1 , 4  , 3 , { { 0 , 10 }, { 10 , 11 }, { 21 , 11 }, { 0  , 0  } } }, //LAYOUT_10_11_11,
        { 1 , 1 , 4  , 3 , { { 0 , 11 }, { 11 , 11 }, { 22 , 10 }, { 0  , 0  } } }, //LAYOUT_11_11_10,
        { 1 , 1 , 4  , 4 , { { 0 , 10 }, { 10 , 10 }, { 20 , 10 }, { 30 , 2  } } }, //LAYOUT_10_10_10_2,
        { 1 , 1 , 2  , 1 , { { 0 , 16 }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_16,
        { 1 , 1 , 4  , 2 , { { 0 , 16 }, { 16 , 16 }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_16_16,
        { 1 , 1 , 8  , 4 , { { 0 , 16 }, { 16 , 16 }, { 32 , 16 }, { 48 , 1  } } }, //LAYOUT_16_16_16_16,
        { 1 , 1 , 4  , 1 , { { 0 , 32 }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_32,
        { 1 , 1 , 8  , 2 , { { 0 , 32 }, { 32 , 32 }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_32_32,
        { 1 , 1 , 12 , 3 , { { 0 , 32 }, { 32 , 32 }, { 64 , 32 }, { 0  , 0  } } }, //LAYOUT_32_32_32,
        { 1 , 1 , 16 , 4 , { { 0 , 32 }, { 32 , 32 }, { 64 , 32 }, { 96 , 32 } } }, //LAYOUT_32_32_32_32,
        { 1 , 1 , 3  , 1 , { { 0 , 24 }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_24,
        { 1 , 1 , 4  , 2 , { { 0 , 8  }, { 8  , 24 }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_8_24,
        { 1 , 1 , 4  , 2 , { { 0 , 24 }, { 24 , 8  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_24_8,
        { 1 , 1 , 4  , 4 , { { 0 , 4  }, { 4  , 4  }, { 8  , 24 }, { 0  , 0  } } }, //LAYOUT_4_4_24,
        { 1 , 1 , 8  , 3 , { { 0 , 32 }, { 32 , 8  }, { 40 , 24 }, { 0  , 0  } } }, //LAYOUT_32_8_24,
        { 4 , 4 , 8  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT1,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT2,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT3,
        { 4 , 4 , 8  , 1 , { { 0 , 4  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT3A,
        { 4 , 4 , 8  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT4,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT5,
        { 4 , 4 , 8  , 1 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT5A,
        { 4 , 4 , 16 , 2 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXN,
        { 4 , 4 , 8  , 2 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_CTX1,
        { 4 , 4 , 8  , 4 , { { 0 , 1  }, { 1  , 1  }, { 2  , 1  }, { 3  , 1  } } }, //LAYOUT_DXT3A_AS_1_1_1_1,
        { 2 , 1 , 4  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_GRGB,
        { 2 , 1 , 4  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_RGBG,

        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_4x4,
        { 5 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x4,
        { 5 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x5,
        { 6 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x5,
        { 6 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x6,
        { 8 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x5,
        { 8 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x6,
        { 8 , 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x8,
        { 10, 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x5,
        { 10, 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x6,
        { 10, 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x8,
        { 10, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x10,
        { 12, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x10,
        { 12, 12, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x12,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_4x4_SFLOAT,
        { 5 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x4_SFLOAT,
        { 5 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x5_SFLOAT,
        { 6 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x5_SFLOAT,
        { 6 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x6_SFLOAT,
        { 8 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x5_SFLOAT,
        { 8 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x6_SFLOAT,
        { 8 , 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x8_SFLOAT,
        { 10, 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x5_SFLOAT,
        { 10, 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x6_SFLOAT,
        { 10, 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x8_SFLOAT,
        { 10, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x10_SFLOAT,
        { 12, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x10_SFLOAT,
        { 12, 12, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x12_SFLOAT,
        // clang-format on
    };
    static_assert(std::size(LAYOUTS) == NUM_COLOR_LAYOUTS, "LAYOUTs array size is wrong.");
    static_assert(LAYOUTS[LAYOUT_UNKNOWN].blockWidth == 0, "LAYOUT_UNKNOWN's blockWidth must be 0.");

    /// @brief Data sign for one channel.
    enum Sign {
        SIGN_UNORM, ///< normalized unsigned integer
        SIGN_SNORM, ///< normalized signed integer
        SIGN_BNORM, ///< normalized bias integer
        SIGN_GNORM, ///< normalized gamma integer (sRGB)
        SIGN_SRGB = SIGN_GNORM,
        SIGN_UINT,  ///< unsigned integer
        SIGN_SINT,  ///< signed integer
        SIGN_BINT,  ///< bias integer
        SIGN_GINT,  ///< gamma integer
        SIGN_FLOAT, ///< signed floating pointer
    };

    /// @brief Swizzle for 1 channel.
    enum Swizzle {
        SWIZZLE_X = 0,
        SWIZZLE_Y = 1,
        SWIZZLE_Z = 2,
        SWIZZLE_W = 3,
        SWIZZLE_0 = 4,
        SWIZZLE_1 = 5,
    };

    /// @brief Swizzle for all 4 channels.
    enum Swizzle4 {
        SWIZZLE_XYZW = (0 << 0) | (1 << 3) | (2 << 6) | (3 << 9),
        SWIZZLE_ZYXW = (2 << 0) | (1 << 3) | (0 << 6) | (3 << 9),
        SWIZZLE_XYZ1 = (0 << 0) | (1 << 3) | (2 << 6) | (5 << 9),
        SWIZZLE_ZYX1 = (2 << 0) | (1 << 3) | (0 << 6) | (5 << 9),
        SWIZZLE_XXXY = (0 << 0) | (0 << 3) | (0 << 6) | (1 << 9),
        SWIZZLE_XY00 = (0 << 0) | (1 << 3) | (4 << 6) | (4 << 9),
        SWIZZLE_XY01 = (0 << 0) | (1 << 3) | (4 << 6) | (5 << 9),
        SWIZZLE_X000 = (0 << 0) | (4 << 3) | (4 << 6) | (4 << 9),
        SWIZZLE_X001 = (0 << 0) | (4 << 3) | (4 << 6) | (5 << 9),
        SWIZZLE_XXX1 = (0 << 0) | (0 << 3) | (0 << 6) | (5 << 9),
        SWIZZLE_111X = (5 << 0) | (5 << 3) | (5 << 6) | (0 << 9),
        SWIZZLE_0Y00 = (0 << 0) | (1 << 3) | (4 << 6) | (4 << 9),
        SWIZZLE_0Y01 = (0 << 0) | (1 << 3) | (4 << 6) | (5 << 9),
    };

    /// @brief Construct pixel format from individual properties.
    static constexpr PixelFormat make(Layout l, Sign si0, Sign si12, Sign si3, Swizzle sw0, Swizzle sw1, Swizzle sw2, Swizzle sw3) {
        // clang-format off
        return {{
            (uint32_t)l     & 0x7f,
            0,
            (uint32_t)si0   & 0xf,
            (uint32_t)si12  & 0xf,
            (uint32_t)si3   & 0xf,
            (uint32_t)sw0   & 0x7,
            (uint32_t)sw1   & 0x7,
            (uint32_t)sw2   & 0x7,
            (uint32_t)sw3   & 0x7,
        }};
        // clang-format on
    }

    /// @brief Construct pixel format from individual properties.
    static constexpr PixelFormat make(Layout l, Sign si0, Sign si12, Sign si3, Swizzle4 sw0123) {
        // clang-format off
        return make(
            l,
            si0,
            si12,
            si3,
            (Swizzle)(((int)sw0123>>0)&3),
            (Swizzle)(((int)sw0123>>3)&3),
            (Swizzle)(((int)sw0123>>6)&3),
            (Swizzle)(((int)sw0123>>9)&3));
        // clang-format on
    }

    /// @brief Construct pixel format from individual properties.
    static constexpr PixelFormat make(Layout l, Sign si012, Sign si3, Swizzle sw0, Swizzle sw1, Swizzle sw2, Swizzle sw3) {
        return make(l, si012, si012, si3, sw0, sw1, sw2, sw3);
    }

    /// @brief Construct pixel format from individual properties.
    static constexpr PixelFormat make(Layout l, Sign si012, Sign si3, Swizzle4 sw0123) { return make(l, si012, si012, si3, sw0123); }

    /// @brief Construct pixel format from individual properties.
    static constexpr PixelFormat make(Layout l, Sign si0123, Swizzle4 sw0123) { return make(l, si0123, si0123, si0123, sw0123); }

    /// @brief Construct pixel format from DXGI_FORMAT enum
    static PixelFormat makeFromDXGIFormat(uint32_t);

    /// @brief Check if the pixel format is empty.
    constexpr bool empty() const { return 0 == layout; }

    /// @brief Check if the pixel format is valid.
    constexpr bool valid() const {
        // clang-format off
        return
            0 < layout && layout < NUM_COLOR_LAYOUTS &&
            sign0    <= SIGN_FLOAT &&
            sign12   <= SIGN_FLOAT &&
            sign3    <= SIGN_FLOAT &&
            swizzle0 <= SWIZZLE_1 &&
            swizzle1 <= SWIZZLE_1 &&
            swizzle2 <= SWIZZLE_1 &&
            swizzle3 <= SWIZZLE_1 &&
            0 == reserved0;
        // clang-format on
    }

    /// @brief Get numeric value of a channel in a pixel.
    /// @param pixel Pointer to the pixel data
    /// @param channel Index of the channel we want the value for. Must be in range of [0, 3].
    float getPixelChannelFloat(const void * pixel, size_t channel);

    ///
    /// get layout descriptor
    ///
    constexpr const LayoutDesc & layoutDesc() const { return LAYOUTS[layout]; }

    ///
    /// Get bytes-per-pixel-block
    ///
    constexpr uint8_t bytesPerBlock() const { return LAYOUTS[layout].blockBytes; }

    /// @brief Load uncompressed pixel value from float4. Do not support compressed format.
    OnePixel loadFromFloat4(const Float4 &) const;

    /// @brief Store uncompressed pixel value to float4.
    Float4 storeToFloat4(const void *) const;

    /// convert to string
    std::string toString() const;

    // /// @brief Convert PixelFormat to DXGI_FORMAT
    // uint32_t toDXGIFormat() const;

    ///
    /// convert to bool
    ///
    constexpr operator bool() const { return !empty(); }

    ///
    /// equality check
    ///
    constexpr bool operator==(const PixelFormat & c) const { return u32 == c.u32; }

    ///
    /// equality check
    ///
    constexpr bool operator!=(const PixelFormat & c) const { return u32 != c.u32; }

    ///
    /// less operator
    ///
    constexpr bool operator<(const PixelFormat & c) const { return u32 < c.u32; }

    // clang-format off

    /// \name aliases of commonly used color formats
    /// @{

    static constexpr PixelFormat UNKNOWN()                     { return {}; }

    // 8 bits
    static constexpr PixelFormat R_8_UNORM()                   { return make(LAYOUT_8, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr PixelFormat R_8_SNORM()                   { return make(LAYOUT_8, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr PixelFormat R_8_UINT()                    { return make(LAYOUT_8, SIGN_UINT,  SWIZZLE_X001); }
    static constexpr PixelFormat R_8_SINT()                    { return make(LAYOUT_8, SIGN_SINT,  SWIZZLE_X001); }
    static constexpr PixelFormat L_8_UNORM()                   { return make(LAYOUT_8, SIGN_UNORM, SWIZZLE_XXX1); }
    static constexpr PixelFormat A_8_UNORM()                   { return make(LAYOUT_8, SIGN_UNORM, SWIZZLE_111X); }
    static constexpr PixelFormat RGB_3_3_2_UNORM()             { return make(LAYOUT_3_3_2, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat BGR_3_3_2_UNORM()             { return make(LAYOUT_3_3_2, SIGN_UNORM, SWIZZLE_ZYX1); }

    // 16 bits
    static constexpr PixelFormat BGRA_4_4_4_4_UNORM()          { return make(LAYOUT_4_4_4_4, SIGN_UNORM, SWIZZLE_ZYXW); }
    static constexpr PixelFormat BGRX_4_4_4_4_UNORM()          { return make(LAYOUT_4_4_4_4, SIGN_UNORM, SWIZZLE_ZYX1); }
    static constexpr PixelFormat BGR_5_6_5_UNORM()             { return make(LAYOUT_5_6_5, SIGN_UNORM, SWIZZLE_ZYX1); }
    static constexpr PixelFormat BGRA_5_5_5_1_UNORM()          { return make(LAYOUT_5_5_5_1, SIGN_UNORM, SWIZZLE_ZYXW); }
    static constexpr PixelFormat BGRX_5_5_5_1_UNORM()          { return make(LAYOUT_5_5_5_1, SIGN_UNORM, SWIZZLE_ZYX1); }

    static constexpr PixelFormat RG_8_8_UNORM()                { return make(LAYOUT_8_8, SIGN_UNORM, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_8_8_SNORM()                { return make(LAYOUT_8_8, SIGN_SNORM, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_8_8_UINT()                 { return make(LAYOUT_8_8, SIGN_UINT,  SWIZZLE_XY01); }
    static constexpr PixelFormat RG_8_8_SINT()                 { return make(LAYOUT_8_8, SIGN_SINT,  SWIZZLE_XY01); }
    static constexpr PixelFormat LA_8_8_UNORM()                { return make(LAYOUT_8_8, SIGN_UNORM, SWIZZLE_XXXY); }

    static constexpr PixelFormat R_16_UNORM()                  { return make(LAYOUT_16, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr PixelFormat R_16_SNORM()                  { return make(LAYOUT_16, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr PixelFormat R_16_UINT()                   { return make(LAYOUT_16, SIGN_UINT , SWIZZLE_X001); }
    static constexpr PixelFormat R_16_SINT()                   { return make(LAYOUT_16, SIGN_SINT , SWIZZLE_X001); }
    static constexpr PixelFormat R_16_FLOAT()                  { return make(LAYOUT_16, SIGN_FLOAT, SWIZZLE_X001); }

    static constexpr PixelFormat L_16_UNORM()                  { return make(LAYOUT_16, SIGN_UNORM, SWIZZLE_XXX1); }

    // 24 bits
    static constexpr PixelFormat RGB_8_8_8_UNORM()             { return make(LAYOUT_8_8_8, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat RGB_8_8_8_SNORM()             { return make(LAYOUT_8_8_8, SIGN_SNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat BGR_8_8_8_UNORM()             { return make(LAYOUT_8_8_8, SIGN_UNORM, SWIZZLE_ZYX1); }
    static constexpr PixelFormat BGR_8_8_8_SNORM()             { return make(LAYOUT_8_8_8, SIGN_SNORM, SWIZZLE_ZYX1); }
    static constexpr PixelFormat R_24_FLOAT()                  { return make(LAYOUT_24, SIGN_FLOAT, SWIZZLE_X001); }

    // 32 bits
    static constexpr PixelFormat RGBA_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_8_8_8_8_SRGB()           { return make(LAYOUT_8_8_8_8, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_8_8_8_8_SNORM()          { return make(LAYOUT_8_8_8_8, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_8_8_8_8_UINT()           { return make(LAYOUT_8_8_8_8, SIGN_UINT,  SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_8_8_8_8_SINT()           { return make(LAYOUT_8_8_8_8, SIGN_SINT,  SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA8()                       { return RGBA_8_8_8_8_UNORM(); }
    static constexpr PixelFormat UBYTE4N()                     { return RGBA_8_8_8_8_UNORM(); }

    static constexpr PixelFormat RGBX_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_XYZ1); }

    static constexpr PixelFormat BGRA_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_ZYXW); }
    static constexpr PixelFormat BGRA_8_8_8_8_UINT()           { return make(LAYOUT_8_8_8_8, SIGN_UINT,  SWIZZLE_ZYXW); }
    static constexpr PixelFormat BGRA8()                       { return BGRA_8_8_8_8_UNORM(); }

    static constexpr PixelFormat BGRX_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_ZYX1); }

    static constexpr PixelFormat RGBA_10_10_10_2_UNORM()       { return make(LAYOUT_10_10_10_2, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_10_10_10_2_UINT()        { return make(LAYOUT_10_10_10_2, SIGN_UINT , SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_10_10_10_SNORM_2_UNORM() { return make(LAYOUT_10_10_10_2, SIGN_SNORM, SIGN_UNORM, SWIZZLE_XYZW); }

    static constexpr PixelFormat BGRA_10_10_10_2_UNORM()       { return make(LAYOUT_10_10_10_2, SIGN_UNORM, SWIZZLE_ZYXW); }

    static constexpr PixelFormat RGB_11_11_10_FLOAT()          { return make(LAYOUT_11_11_10, SIGN_FLOAT, SWIZZLE_XYZ1); }

    static constexpr PixelFormat RG_16_16_UNORM()              { return make(LAYOUT_16_16, SIGN_UNORM, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_16_16_SNORM()              { return make(LAYOUT_16_16, SIGN_SNORM, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_16_16_UINT()               { return make(LAYOUT_16_16, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_16_16_SINT()               { return make(LAYOUT_16_16, SIGN_SINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_16_16_FLOAT()              { return make(LAYOUT_16_16, SIGN_FLOAT, SWIZZLE_XY01); }
    static constexpr PixelFormat USHORT2N()                    { return RG_16_16_UNORM(); }
    static constexpr PixelFormat SHORT2N()                     { return RG_16_16_SNORM(); }
    static constexpr PixelFormat USHORT2()                     { return RG_16_16_UINT(); }
    static constexpr PixelFormat SHORT2()                      { return RG_16_16_SINT(); }
    static constexpr PixelFormat HALF2()                       { return RG_16_16_FLOAT(); }

    static constexpr PixelFormat LA_16_16_UNORM()              { return make(LAYOUT_16_16, SIGN_UNORM, SWIZZLE_XXXY); }

    static constexpr PixelFormat R_32_UNORM()                  { return make(LAYOUT_32, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr PixelFormat R_32_SNORM()                  { return make(LAYOUT_32, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr PixelFormat R_32_UINT()                   { return make(LAYOUT_32, SIGN_UINT, SWIZZLE_X001); }
    static constexpr PixelFormat R_32_SINT()                   { return make(LAYOUT_32, SIGN_SINT, SWIZZLE_X001); }
    static constexpr PixelFormat R_32_FLOAT()                  { return make(LAYOUT_32, SIGN_FLOAT, SWIZZLE_X001); }
    static constexpr PixelFormat UINT1N()                      { return R_32_UNORM(); }
    static constexpr PixelFormat INT1N()                       { return R_32_SNORM(); }
    static constexpr PixelFormat UINT1()                       { return R_32_UINT(); }
    static constexpr PixelFormat INT1()                        { return R_32_SINT(); }
    static constexpr PixelFormat FLOAT1()                      { return R_32_FLOAT(); }

    static constexpr PixelFormat GR_8_UINT_24_UNORM()          { return make(LAYOUT_8_24, SIGN_UINT, SIGN_UNORM, SIGN_UINT, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_0, SWIZZLE_1); }
    static constexpr PixelFormat GX_8_24_UNORM()               { return make(LAYOUT_8_24, SIGN_UINT, SIGN_UNORM, SIGN_UINT, SWIZZLE_Y, SWIZZLE_0, SWIZZLE_0, SWIZZLE_1); }

    static constexpr PixelFormat RG_24_UNORM_8_UINT()          { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RX_24_8_UNORM()               { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_24_8_UINT()                { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat XG_24_8_UINT()                { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SIGN_UINT, SWIZZLE_0Y01); }

    static constexpr PixelFormat RG_24_FLOAT_8_UINT()          { return make(LAYOUT_24_8, SIGN_FLOAT, SIGN_UINT, SIGN_UINT, SWIZZLE_XY01); }

    static constexpr PixelFormat GRGB_UNORM()                  { return make(LAYOUT_GRGB, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat RGBG_UNORM()                  { return make(LAYOUT_RGBG, SIGN_UNORM, SWIZZLE_XYZ1); }

    // 64 bits
    static constexpr PixelFormat RGBA_16_16_16_16_UNORM()      { return make(LAYOUT_16_16_16_16, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_16_16_16_16_SNORM()      { return make(LAYOUT_16_16_16_16, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_16_16_16_16_UINT()       { return make(LAYOUT_16_16_16_16, SIGN_UINT , SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_16_16_16_16_SINT()       { return make(LAYOUT_16_16_16_16, SIGN_SINT , SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_16_16_16_16_FLOAT()      { return make(LAYOUT_16_16_16_16, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat USHORT4N()                    { return RGBA_16_16_16_16_UNORM(); }
    static constexpr PixelFormat SHORT4N()                     { return RGBA_16_16_16_16_SNORM(); }
    static constexpr PixelFormat USHORT4()                     { return RGBA_16_16_16_16_UINT(); }
    static constexpr PixelFormat SHORT4()                      { return RGBA_16_16_16_16_SINT(); }
    static constexpr PixelFormat HALF4()                       { return RGBA_16_16_16_16_FLOAT(); }

    static constexpr PixelFormat RGBX_16_16_16_16_UNORM()      { return make(LAYOUT_16_16_16_16, SIGN_UNORM, SWIZZLE_XYZ1); }

    static constexpr PixelFormat RG_32_32_UNORM()              { return make(LAYOUT_32_32, SIGN_UNORM, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_32_32_SNORM()              { return make(LAYOUT_32_32, SIGN_SNORM, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_32_32_UINT()               { return make(LAYOUT_32_32, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_32_32_SINT()               { return make(LAYOUT_32_32, SIGN_SINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RG_32_32_FLOAT()              { return make(LAYOUT_32_32, SIGN_FLOAT, SWIZZLE_XY01); }
    static constexpr PixelFormat FLOAT2()                      { return RG_32_32_FLOAT(); }

    static constexpr PixelFormat RGX_32_FLOAT_8_UINT_24()      { return make(LAYOUT_32_8_24, SIGN_FLOAT, SIGN_UINT, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat RXX_32_8_24_FLOAT()           { return make(LAYOUT_32_8_24, SIGN_FLOAT, SIGN_UINT, SIGN_UINT, SWIZZLE_X001); }
    static constexpr PixelFormat RGX_32_8_24_UINT()            { return make(LAYOUT_32_8_24, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr PixelFormat XGX_32_8_24_UINT()            { return make(LAYOUT_32_8_24, SIGN_UINT, SWIZZLE_0Y01); }

    // 96 bits
    static constexpr PixelFormat RGB_32_32_32_UNORM()          { return make(LAYOUT_32_32_32, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat RGB_32_32_32_SNORM()          { return make(LAYOUT_32_32_32, SIGN_SNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat RGB_32_32_32_UINT()           { return make(LAYOUT_32_32_32, SIGN_UINT , SWIZZLE_XYZ1); }
    static constexpr PixelFormat RGB_32_32_32_SINT()           { return make(LAYOUT_32_32_32, SIGN_SINT , SWIZZLE_XYZ1); }
    static constexpr PixelFormat RGB_32_32_32_FLOAT()          { return make(LAYOUT_32_32_32, SIGN_FLOAT, SWIZZLE_XYZ1); }
    static constexpr PixelFormat FLOAT3()                      { return RGB_32_32_32_FLOAT(); }

    // 128 bits
    static constexpr PixelFormat RGBA_32_32_32_32_UNORM()      { return make(LAYOUT_32_32_32_32, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_32_32_32_32_SNORM()      { return make(LAYOUT_32_32_32_32, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_32_32_32_32_UINT()       { return make(LAYOUT_32_32_32_32, SIGN_UINT , SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_32_32_32_32_SINT()       { return make(LAYOUT_32_32_32_32, SIGN_SINT , SWIZZLE_XYZW); }
    static constexpr PixelFormat RGBA_32_32_32_32_FLOAT()      { return make(LAYOUT_32_32_32_32, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat UINT4N()                      { return RGBA_32_32_32_32_UNORM(); }
    static constexpr PixelFormat SINT4N()                      { return RGBA_32_32_32_32_SNORM(); }
    static constexpr PixelFormat UINT4()                       { return RGBA_32_32_32_32_UINT(); }
    static constexpr PixelFormat SINT4()                       { return RGBA_32_32_32_32_SINT(); }
    static constexpr PixelFormat FLOAT4()                      { return RGBA_32_32_32_32_FLOAT(); }

    // compressed
    static constexpr PixelFormat DXT1_UNORM()                  { return make(LAYOUT_DXT1, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat DXT1_SRGB()                   { return make(LAYOUT_DXT1, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat DXT1_UINT()                   { return make(LAYOUT_DXT1, SIGN_UINT,  SWIZZLE_XYZ1); }

    static constexpr PixelFormat DXT2_UNORM()                  { return make(LAYOUT_DXT2, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat DXT2_SRGB()                   { return make(LAYOUT_DXT2, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat DXT2_UINT()                   { return make(LAYOUT_DXT2, SIGN_UINT,  SWIZZLE_XYZW); }

    static constexpr PixelFormat DXT3_UNORM()                  { return make(LAYOUT_DXT3, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat DXT3_SRGB()                   { return make(LAYOUT_DXT3, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr PixelFormat DXT3_UINT()                   { return make(LAYOUT_DXT3, SIGN_UINT,  SWIZZLE_XYZ1); }

    static constexpr PixelFormat DXT4_UNORM()                  { return make(LAYOUT_DXT4, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr PixelFormat DXT4_SNORM()                  { return make(LAYOUT_DXT4, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr PixelFormat DXT4_UINT()                   { return make(LAYOUT_DXT4, SIGN_UINT,  SWIZZLE_X001); }

    static constexpr PixelFormat DXT5_UNORM()                  { return make(LAYOUT_DXT5, SIGN_UNORM, SWIZZLE_XY00); }
    static constexpr PixelFormat DXT5_SNORM()                  { return make(LAYOUT_DXT5, SIGN_SNORM, SWIZZLE_XY00); }
    static constexpr PixelFormat DXT5_UINT()                   { return make(LAYOUT_DXT5, SIGN_UINT,  SWIZZLE_XY00); }

    static constexpr PixelFormat DXT5A_UNORM()                 { return make(LAYOUT_DXT5A, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat DXT5A_SNORM()                 { return make(LAYOUT_DXT5A, SIGN_SNORM, SWIZZLE_XYZW); }

    static constexpr PixelFormat DXN_UNORM()                   { return make(LAYOUT_DXN, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat DXN_SNORM()                   { return make(LAYOUT_DXN, SIGN_SNORM, SWIZZLE_XYZW); }

    static constexpr PixelFormat ASTC_4x4_UNORM()              { return make(LAYOUT_ASTC_4x4, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_4x4_SRGB()               { return make(LAYOUT_ASTC_4x4, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_5x4_UNORM()              { return make(LAYOUT_ASTC_5x4, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_5x4_SRGB()               { return make(LAYOUT_ASTC_5x4, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_5x5_UNORM()              { return make(LAYOUT_ASTC_5x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_5x5_SRGB()               { return make(LAYOUT_ASTC_5x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_6x5_UNORM()              { return make(LAYOUT_ASTC_6x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_6x5_SRGB()               { return make(LAYOUT_ASTC_6x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_6x6_UNORM()              { return make(LAYOUT_ASTC_6x6, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_6x6_SRGB()               { return make(LAYOUT_ASTC_6x6, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x5_UNORM()              { return make(LAYOUT_ASTC_8x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x5_SRGB()               { return make(LAYOUT_ASTC_8x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x6_UNORM()              { return make(LAYOUT_ASTC_8x6, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x6_SRGB()               { return make(LAYOUT_ASTC_8x6, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x8_UNORM()              { return make(LAYOUT_ASTC_8x8, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x8_SRGB()               { return make(LAYOUT_ASTC_8x8, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x5_UNORM()             { return make(LAYOUT_ASTC_10x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x5_SRGB()              { return make(LAYOUT_ASTC_10x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x6_UNORM()             { return make(LAYOUT_ASTC_10x6, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x6_SRGB()              { return make(LAYOUT_ASTC_10x6, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x8_UNORM()             { return make(LAYOUT_ASTC_10x8, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x8_SRGB()              { return make(LAYOUT_ASTC_10x8, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x10_UNORM()            { return make(LAYOUT_ASTC_10x10, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x10_SRGB()             { return make(LAYOUT_ASTC_10x10, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_12x10_UNORM()            { return make(LAYOUT_ASTC_12x10, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_12x10_SRGB()             { return make(LAYOUT_ASTC_12x10, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_12x12_UNORM()            { return make(LAYOUT_ASTC_12x12, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_12x12_SRGB()             { return make(LAYOUT_ASTC_12x12, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_4x4_SFLOAT()             { return make(LAYOUT_ASTC_4x4_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_5x4_SFLOAT()             { return make(LAYOUT_ASTC_5x4_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_5x5_SFLOAT()             { return make(LAYOUT_ASTC_5x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_6x5_SFLOAT()             { return make(LAYOUT_ASTC_6x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_6x6_SFLOAT()             { return make(LAYOUT_ASTC_6x6_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x5_SFLOAT()             { return make(LAYOUT_ASTC_8x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x6_SFLOAT()             { return make(LAYOUT_ASTC_8x6_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_8x8_SFLOAT()             { return make(LAYOUT_ASTC_8x8_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x5_SFLOAT()            { return make(LAYOUT_ASTC_10x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x6_SFLOAT()            { return make(LAYOUT_ASTC_10x6_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x8_SFLOAT()            { return make(LAYOUT_ASTC_10x8_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_10x10_SFLOAT()           { return make(LAYOUT_ASTC_10x10_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_12x10_SFLOAT()           { return make(LAYOUT_ASTC_12x10_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr PixelFormat ASTC_12x12_SFLOAT()           { return make(LAYOUT_ASTC_12x12_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }

    //@}

    // clang-format on
};
static_assert(4 == sizeof(PixelFormat), "size of PixelFormat must be 4");
static_assert(PixelFormat::UNKNOWN().layoutDesc().blockWidth == 0, "UNKNOWN format's block width must be 0");
static_assert(!PixelFormat::UNKNOWN().valid(), "UNKNOWN format must be invalid.");
static_assert(PixelFormat::UNKNOWN().empty(), "UNKNOWN format must be empty.");
static_assert(PixelFormat::RGBA8().valid(), "RGBA8 format must be valid.");
static_assert(!PixelFormat::RGBA8().empty(), "RGBA8 format must _NOT_ be empty.");
static_assert(PixelFormat::ASTC_12x12_SFLOAT().valid(), "ASTC_12x12_SFLOAT must be a valid format.");

// Helper functions and structs to count bits at compile time.
namespace BitFieldDetails {
template<typename T>
constexpr auto getBitsCount(T data, size_t startBit = 0) -> typename std::enable_if<std::is_unsigned<T>::value, size_t>::type {
    return (startBit == sizeof(T) * 8) ? 0 : getBitsCount(data, startBit + 1) + ((data & (1ull << startBit)) ? 1 : 0);
}

// We should support unsigned enums too
template<typename T>
constexpr auto getBitsCount(T data, size_t startBit = 0) -> typename std::enable_if<std::is_enum<T>::value, size_t>::type {
    return (startBit == sizeof(T) * 8)
               ? 0
               : getBitsCount(data, startBit + 1) + ((static_cast<typename std::underlying_type<T>::type>(data) & (1ull << startBit)) ? 1 : 0);
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbitfield-constant-conversion"
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4463) // overflow
#endif
template<typename StructType, typename StructFieldsType, StructFieldsType... initVals>
constexpr StructType getFilledStructImpl(StructType s, ...) {
    return s;
}

template<typename StructType, typename StructFieldsType, StructFieldsType... initVals>
constexpr StructType getFilledStructImpl(StructType, decltype(StructType {initVals...}, int()) = 0) {
    // can you numeric_limits::max() here instead of -1, but it will require casting to and from underlying type for enum
    return getFilledStructImpl<StructType, StructFieldsType, initVals..., static_cast<StructFieldsType>(-1)>(StructType {initVals...}, 0);
}

template<typename StructType, typename StructFieldsType>
constexpr StructType getFilledStruct() {
    using UnderlyingStructType =
        typename std::conditional<std::is_enum<StructFieldsType>::value, std::underlying_type<StructFieldsType>, std::remove_cv<StructFieldsType>>::type::type;
    static_assert(std::is_unsigned<UnderlyingStructType>::value, "Bit field calculation only works with unsigned values");
    return getFilledStructImpl<StructType, StructFieldsType>(StructType {}, 0);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
} // namespace BitFieldDetails

// Get bit field size at compile time
#define RII_GET_BIT_FIELD_WIDTH(structType, fieldName) \
    BitFieldDetails::getBitsCount(BitFieldDetails::getFilledStruct<structType, decltype(structType {}.fieldName)>().fieldName)

// Make sure that all layouts can fit into the PixelFormat::layout field. If this assert fails, then we have defined more
// layouts then the PixelFormat::layout field can hold. In this case, you either need to delete some layouts, or increase
// size of PixelFormat::layout field.
static_assert(PixelFormat::NUM_COLOR_LAYOUTS < (1u << RII_GET_BIT_FIELD_WIDTH(PixelFormat, layout)), "");

/// @brief compose RGBA8 color constant
template<typename T>
inline constexpr uint32_t makeRGBA8(T r, T g, T b, T a) {
    // clang-format off
    return (
        ( ((uint32_t)(r)&0xFF) <<  0 ) |
        ( ((uint32_t)(g)&0xFF) <<  8 ) |
        ( ((uint32_t)(b)&0xFF) << 16 ) |
        ( ((uint32_t)(a)&0xFF) << 24 ) );
    // clang-format on
}

/// @brief compose BGRA8 color constant
template<typename T>
inline constexpr uint32_t makeBGRA8(T r, T g, T b, T a) {
    // clang-format off
    return (
        ( ((uint32_t)(b)&0xFF) <<  0 ) |
        ( ((uint32_t)(g)&0xFF) <<  8 ) |
        ( ((uint32_t)(r)&0xFF) << 16 ) |
        ( ((uint32_t)(a)&0xFF) << 24 ) );
    // clang-format on
}

class Image;

struct Extent3D {
    uint32_t w = 0; ///< width
    uint32_t h = 0; ///< height
    uint32_t d = 0; ///< depth;

    Extent3D & set(uint32_t w_, uint32_t h_ = 1, uint32_t d_ = 1) {
        w = w_;
        h = h_;
        d = d_;
        return *this;
    }

    bool empty() const { return 0 == w || 0 == h || 0 == d; }

    bool operator==(const Extent3D & rhs) const { return w == rhs.w && h == rhs.h && d == rhs.d; }

    bool operator!=(const Extent3D & rhs) const { return w != rhs.w || h != rhs.h || d != rhs.d; }

    bool operator<(const Extent3D & rhs) const {
        if (w != rhs.w) return w < rhs.w;
        if (h != rhs.h) return h < rhs.h;
        return d < rhs.d;
    }

    static Extent3D make(uint32_t w_, uint32_t h_ = 1, uint32_t d_ = 1) { return {w_, h_, d_}; }
};

/// This represents a single 1D/2D/3D image plan. This is the building block of more complex image structures like
/// cube/array images, mipmap chains and etc.
struct PlaneDesc {
    /// pixel format
    PixelFormat format = PixelFormat::UNKNOWN();

    /// Extent of the plane
    Extent3D extent {};

    /// bytes (not BITS) from one pixel or pixel block to the next. Minimal valid value is pixel or pixel block size.
    uint32_t step = 0;

    /// Bytes from one row to next. Minimal valid value is (width * step) and aligned to pixel boundary.
    /// For compressed format, this is the number of bytes in one row of pixel blocks.
    uint32_t pitch = 0;

    /// Bytes from one slice to next. Minimal valid value is pitch * ((height + blockHeight - 1) / blockHeight)
    uint32_t slice = 0;

    /// Bytes of the whole plane. Minimal valid value is (slice * depth).
    uint32_t size = 0;

    /// Bytes between first pixel of the plane to the first pixel of the whole image.
    uint32_t offset = 0;

    /// Create a new image plane descriptor
    static PlaneDesc make(PixelFormat format, const Extent3D & extent, size_t step = 0, size_t pitch = 0, size_t slice = 0);

    PlaneDesc & setFormat(PixelFormat format_) {
        format = format_;
        return *this;
    }

    PlaneDesc & setExtent(const Extent3D & e) {
        extent = e;
        return *this;
    }

    PlaneDesc & setExtent(size_t width_, size_t height_ = 1, size_t depth_ = 1) {
        extent.set((uint32_t) width_, (uint32_t) height_, (uint32_t) depth_);
        return *this;
    }

    PlaneDesc & setSpacing(size_t step_, size_t pitch_ = 0, size_t slice_ = 0) {
        RII_REQUIRE(format.valid(), "must call this after setting the pixel format.");
        const auto & fd = format.layoutDesc();
        RII_REQUIRE(!extent.empty(), "must call this after setting the plane extent.");
        auto numBlocksPerRow = (uint32_t) ((extent.w + fd.blockWidth - 1) / fd.blockWidth);
        auto numBlocksPerCol = (uint32_t) ((extent.h + fd.blockHeight - 1) / fd.blockHeight);
        step                 = std::max((uint32_t) step_, (uint32_t) (fd.blockBytes));
        pitch                = std::max(step * numBlocksPerRow, (uint32_t) pitch_);
        slice                = std::max(pitch * numBlocksPerCol, (uint32_t) slice_);
        return *this;
    }

    /// returns offset from start of data buffer for a particular pixel within the plane
    size_t pixel(size_t x, size_t y, size_t z = 0) const {
        RII_ASSERT(x < extent.w && y < extent.h && z < extent.d);
        auto & ld = format.layoutDesc();
        RII_ASSERT((x % ld.blockWidth) == 0 && (y % ld.blockHeight) == 0);
        size_t r = z * slice + y / ld.blockHeight * pitch + x / ld.blockWidth * step;
        RII_ASSERT(r < size);
        return r + offset;
    }

    /// check if this is a valid image plane descriptor. Note that valid descriptor is never empty.
    bool valid() const;

    /// check if this is an empty descriptor. Note that empty descriptor is never valid.
    bool empty() const { return PixelFormat::UNKNOWN() == format; }

    /// Convert the image plane to float4 format.
    /// \return Pixel data in float4 format.
    std::vector<Float4> toFloat4(const void * src) const;

    /// Convert image plane to rgba8 format.
    /// \return Pixel data in rgba8 format.
    std::vector<RGBA8> toRGBA8(const void * src) const;

    /// Load data to specific slice of image plane from float4 data.
    void fromFloat4(void * dst, size_t dstSize, size_t dstZ, const void * src) const;

    /// @brief Generate full mipmap chain from this image plane.
    /// @param pixels The pixel data. The layout of the data must match the plane descriptor.
    /// @param maxLevels The maximum number of mipmap levels to generate. Set t0 0 to generate full mipmap chain.
    Image generateMipmaps(const void * pixels, size_t maxLevels = 0) const;

    bool operator==(const PlaneDesc & rhs) const {
        // clang-format off
        return format == rhs.format
            && extent == rhs.extent
            && step   == rhs.step
            && pitch  == rhs.pitch
            && slice  == rhs.slice
            && size   == rhs.size
            && offset == rhs.offset;
        // clang-format on
    }

    bool operator!=(const PlaneDesc & rhs) const { return !operator==(rhs); }

    bool operator<(const PlaneDesc & rhs) const {
        // clang-format off
        if (format != rhs.format) return format < rhs.format;
        if (extent != rhs.extent) return extent < rhs.extent;
        if (step   != rhs.step)   return step   < rhs.step;
        if (pitch  != rhs.pitch)  return pitch  < rhs.pitch;
        if (slice  != rhs.slice)  return slice  < rhs.slice;
        if (size   != rhs.size)   return size   < rhs.size;
        return offset < rhs.offset;
        // clang-format on
    }
};

///
/// Represent a complex image with optional mipmap chain
///
struct ImageDesc {
    /// Default alignment requirement for each plane. We set it to 16 bytes to be compatible with SSE instructions.
    inline static constexpr uint32_t DEFAULT_ALIGNMENT = 16;

    // ****************************
    /// \name member data
    // ****************************

    //@{

    /// @brief Image plane array.
    /// Length of the array should always be (layers * levels), in mip level major fashion. Index, layer and level
    /// can be calculated as follows:
    ///  - index = level * layers + layer
    ///  - level = index / layers
    ///  - layer = index % layers
    std::vector<PlaneDesc> planes;
    uint32_t               layers    = 0;                 ///< number of layers
    uint32_t               levels    = 0;                 ///< number of mipmap levels
    uint64_t               size      = 0;                 ///< total size in bytes of the whole image.
    uint32_t               alignment = DEFAULT_ALIGNMENT; ///< memory alignment requirement for each plane. Default is 4 bytes.
    uint32_t : 32;                                        // pading;

    //@}

    // ****************************
    /// \name ctor/dtor/copy/move
    // ****************************

    //@{

    /// @brief  Default constructor
    ImageDesc() = default;

    /// Defines how pixels packed in memory.
    /// Note that this only affects how plane offset are calculated. The 'planes' data member in this structure
    /// is always indexed in mip level major fashion.
    enum ConstructionOrder {
        /// In this mode, pixels from same face are packed together. For example, given a cubemap with 6 faces
        /// and 3 mipmap levels, the offsets are calculated in this order:
        ///
        ///     face 0, mip 0
        ///     face 0, mip 1
        ///     face 0, mip 2
        ///
        ///     face 1, mip 0
        ///     face 1, mip 1
        ///     face 1, mip 2
        ///     ...
        ///     face 5, mip 0
        ///     face 5, mip 1
        ///     face 5, mip 2
        ///
        /// Note: this is the order used by DDS file.
        FACE_MAJOR,

        /// In this mode, pixels from same mipmap level are packed together. For example, given a cubemap with
        /// 6 faces and 3 mipmap levels, the pixels are packed in memory in this order:
        ///
        ///     mip 0, face 0
        ///     mip 0, face 1
        ///     mip 0, ...
        ///     mip 0, face 5
        ///
        ///     mip 1, face 0
        ///     mip 1, face 1
        ///     mip 1, ...
        ///     mip 1, face 5
        ///
        ///     mip 2, face 0
        ///     mip 2, face 1
        ///     mip 2, ...
        ///     mip 2, face 5
        MIP_MAJOR,
    };

    /// Copy constructor
    ImageDesc(const ImageDesc &) = default;

    /// Copy operator
    ImageDesc & operator=(const ImageDesc &) = default;

    /// move constructor
    ImageDesc(ImageDesc &&);

    /// move operator
    ImageDesc & operator=(ImageDesc &&);

    /// Reset to empty image
    ImageDesc & clear();

    /// Reset the descriptor
    /// \param baseMap Base map of the image. This is the first plane of the image.
    /// \param layers  Number of layers in the image. Default is 1.
    /// \param levels  Number of mipmap levels. Default is 1. Set to 0 to automatically generate the full mipmap chain.
    /// \param order   Specify the order of image planes in memory. Default is FACE_MAJOR.
    /// \param planeOffsetAlignment Alignment of the plane offset. Default is 4.
    ImageDesc & reset(const PlaneDesc & baseMap, size_t layers = 1, size_t levels = 1, ConstructionOrder order = FACE_MAJOR,
                      size_t planeOffsetAlignment = DEFAULT_ALIGNMENT);

    /// @brief Set to simple 2D Image
    /// @param format   Pixel format
    /// @param width    Width of the image in pixels
    /// @param height   Height of the image in pixels
    /// @param levels   Mipmap count. Set to 0 to automatically generate full mipmap chain.
    ImageDesc & set2D(PixelFormat format, size_t width, size_t height = 1, size_t levels = 1, ConstructionOrder order = MIP_MAJOR,
                      size_t planeOffsetAlignment = DEFAULT_ALIGNMENT);

    /// @brief Set to simple 2D Image
    /// @param format   Pixel format
    /// @param width    Width and height of the image in pixels
    /// @param levels   Mipmap count. Set to 0 to automatically generate full mipmap chain.
    ImageDesc & setCube(PixelFormat format, size_t width, size_t levels = 1, ConstructionOrder order = MIP_MAJOR,
                        size_t planeOffsetAlignment = DEFAULT_ALIGNMENT);

    /// @brief Make a new image descriptor.
    /// This method takes exactly same parameters as ImageDesc::reset() method.
    static ImageDesc make(const PlaneDesc & baseMap, size_t layers = 1, size_t levels = 1, ConstructionOrder order = FACE_MAJOR,
                          size_t planeOffsetAlignment = DEFAULT_ALIGNMENT) {
        ImageDesc d;
        d.reset(baseMap, layers, levels, order, planeOffsetAlignment);
        return d;
    }

    //@}

    // ****************************
    /// \name public methods
    // ****************************

    //@{

    ///
    /// check if the image is empty or not
    ///
    bool empty() const { return planes.empty(); }

    ///
    /// make sure this is a meaningful image descriptor
    ///
    bool valid() const;

    /// methods to return properties of the specific plane.
    //@{
    // clang-format off
    const PlaneDesc & plane (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)]; }
    PlaneDesc       & plane (size_t layer = 0, size_t level = 0)       { return planes[index(layer, level)]; }
    PixelFormat       format(size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].format; }
    const Extent3D &  extent(size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].extent; }
    uint32_t          width (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].extent.w; }
    uint32_t          height(size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].extent.h; }
    uint32_t          depth (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].extent.d; }
    uint32_t          step  (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].step; }
    uint32_t          pitch (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].pitch; }
    uint32_t          slice (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].slice; }
    // clang-format on
    //@}

    ///
    /// returns offset of particular pixel
    ///
    size_t pixel(size_t layer, size_t level, size_t x = 0, size_t y = 0, size_t z = 0) const {
        const auto & d = planes[index(layer, level)];
        auto         r = d.pixel(x, y, z);
        RII_ASSERT(r < size);
        return r;
    }

    /// @brief Calculate plane index based on layer and level.
    size_t index(size_t layer, size_t level) const {
        RII_ASSERT(layer < layers);
        RII_ASSERT(level < levels);
        return (level * layers) + layer;
    }

    struct AlignedDeleter {
        void operator()(void * p) const { rii_details::afree(p); }
    };
    using AlignedUniquePtr = std::unique_ptr<uint8_t, AlignedDeleter>;

    /// @brief Load the image from input stream.
    /// This method support .RIL and .DDS formats by default. It can also support loading from other common image formats if
    /// stb_image.h is included before this header.
    AlignedUniquePtr load(std::istream & stream);

    /// @brief Load the image from memory buffer
    /// This method support .RIL and .DDS formats by default. It can also support loading from other common image formats if
    /// stb_image.h is included before this header.
    AlignedUniquePtr load(const void * data, size_t size);

    enum FileFormat {
        RIL, ///< Rapid Image Library format.
        DDS, ///< Direct Draw Surface format.
        JPG, ///< Joint Photographic Experts Group format. Requires stb_image_write.h to be included before this header.
        PNG, ///< Portable Network Graphics format. Requires stb_image_write.h to be included before this header.
        BMP, ///< Windows Bitmap format. Requires stb_image_write.h to be included before this header.
    };

    struct SaveToStreamParameters {
        /// @brief File format to save the image as.
        FileFormat format = RIL;

        /// @brief Quality of the compression. Only used for JPG.
        int quality = 85;

        SaveToStreamParameters & setFormat(FileFormat f) {
            format = f;
            return *this;
        }

        SaveToStreamParameters & setQuality(int q) {
            quality = q;
            return *this;
        }
    };

    /// @brief Save the image to output stream.
    void save(const SaveToStreamParameters & params, std::ostream & stream, const void * pixels) const;

    /// @brief Save image to file.
    void save(const SaveToStreamParameters & params, const std::string & filename, const void * pixels) const;

    /// @brief Save image to file. Use extension to determine file format. Use default value for other options.
    void save(const std::string & filename, const void * pixels) const;

    /// @brief equality operator
    bool operator==(const ImageDesc & rhs) const {
        return planes == rhs.planes && layers == rhs.layers && levels == rhs.levels && size == rhs.size && alignment == rhs.alignment;
    }

    /// @brief inequality operator
    bool operator!=(const ImageDesc & rhs) const { return !operator==(rhs); }

    /// @brief less than operator
    bool operator<(const ImageDesc & rhs) const {
        if (layers != rhs.layers) return layers < rhs.layers;
        if (levels != rhs.levels) return levels < rhs.levels;
        if (size != rhs.size) return size < rhs.size;
        if (alignment != rhs.alignment) return alignment < rhs.alignment;
        if (planes.size() != rhs.planes.size()) return planes.size() < rhs.planes.size();
        for (size_t i = 0; i < planes.size(); ++i) {
            const auto & a = planes[i];
            const auto & b = rhs.planes[i];
            if (a != b) return a < b;
        }
        return false;
    }

private:
    AlignedUniquePtr loadFromRIL(std::istream & stream);
    AlignedUniquePtr loadFromDDS(std::istream & stream);
};

/// Image descriptor combined with a pointer to pixel array. This is a convenient helper class for passing image
/// data around w/o actually copying pixel data array.
struct ImageProxy {
    ImageDesc desc;           ///< the image descriptor
    uint8_t * data = nullptr; ///< the image data (pixel array)

    /// return size of the whole image in bytes.
    uint64_t size() const { return desc.size; }

    /// check if the image is empty or not.
    bool empty() const { return desc.empty(); }

    /// \name query properties of the specific plane.
    //@{
    // clang-format off
    PixelFormat      format(size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).format; }
    const Extent3D & extent(size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).extent; }
    uint32_t         width (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).extent.w; }
    uint32_t         height(size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).extent.h; }
    uint32_t         depth (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).extent.d; }
    uint32_t         step  (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).step; }
    uint32_t         pitch (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).pitch; }
    uint32_t         slice (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).slice; }
    // clang-format on
    //@}

    /// return pointer to particular pixel
    const uint8_t * pixel(size_t layer, size_t level, size_t x = 0, size_t y = 0, size_t z = 0) const { return data + desc.pixel(layer, level, x, y, z); }

    /// return pointer to particular pixel
    uint8_t * pixel(size_t layer, size_t level, size_t x = 0, size_t y = 0, size_t z = 0) { return data + desc.pixel(layer, level, x, y, z); }

    /// Save image to output stream
    void save(const ImageDesc::SaveToStreamParameters & params, std::ostream & stream) const { return desc.save(params, stream, data); }

    /// Save image to file
    void save(const std::string & filename) const { return desc.save(filename, data); }
};

/// @brief The image class of rapid-image library.
class Image {
public:
    /// \name ctor/dtor/copy/move
    //@{
    RII_NO_COPY(Image);
    Image() = default;
    Image(ImageDesc && desc, const void * initialContent = nullptr, size_t initialContentSizeInbytes = 0);
    Image(const ImageDesc & desc, const void * initialContent = nullptr, size_t initialContentSizeInbytes = 0);
    Image(Image && rhs) {
        _proxy.desc = std::move(rhs._proxy.desc);
        RII_ASSERT(rhs._proxy.desc.empty());
        _proxy.data     = rhs._proxy.data;
        rhs._proxy.data = nullptr;
    }
    ~Image() { clear(); }
    Image & operator=(Image && rhs) {
        if (this != &rhs) {
            _proxy.desc = std::move(rhs._proxy.desc);
            RII_ASSERT(rhs._proxy.desc.empty());
            _proxy.data     = rhs._proxy.data;
            rhs._proxy.data = nullptr;
        }
        return *this;
    }
    //@}

    /// \name basic property query
    //@{

    /// return proxy of the image.
    const ImageProxy & proxy() const { return _proxy; }

    /// return descriptor of the whole image
    const ImageDesc & desc() const { return _proxy.desc; }

    /// return descriptor of a image plane
    const PlaneDesc & plane(size_t layer, size_t level) const { return _proxy.desc.plane(layer, level); }

    /// return pointer to pixel buffer.
    const uint8_t * data() const { return _proxy.data; }

    /// return pointer to pixel buffer.
    uint8_t * data() { return _proxy.data; }

    /// return size of the whole image in bytes.
    uint64_t size() const { return _proxy.desc.size; }

    /// check if the image is empty or not.
    bool empty() const { return _proxy.desc.empty(); }

    //@}

    /// \name query properties of the specific plane.
    //@{
    // clang-format off
    PixelFormat      format(size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).format; }
    const Extent3D & extent(size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).extent; }
    uint32_t         width (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).extent.w; }
    uint32_t         height(size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).extent.h; }
    uint32_t         depth (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).extent.d; }
    uint32_t         step  (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).step; }
    uint32_t         pitch (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).pitch; }
    uint32_t         slice (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).slice; }
    // clang-format on
    //@}

    /// reset to an empty image
    void clear();

    /// @brief Make a clone of the current image.
    /// Cloning image is a very costly operation, involving memory allocation and data copy. This is why the
    /// copy constructor and copy operator are disabled. If you want to make a copy of the image, use this method and be
    /// aware of the memory and performance cost of it.
    Image clone() const { return Image(desc(), data()); }

    /// Save image to stream
    void save(const ImageDesc::SaveToStreamParameters & params, std::ostream & stream) const { return _proxy.save(params, stream); }

    /// Save image to file
    void save(const std::string & filename) const { return _proxy.save(filename); }

    /// Load image from binary input stream.
    static Image load(std::istream &);

    /// Load image from memory buffer.
    static Image load(const void * data, size_t size);

    /// Load image from file.
    static Image load(const std::string & filename);

private:
    ImageProxy _proxy;

private:
    void construct(const void * initialContent, size_t initialContentSizeInbytes);
};

} // namespace RAPID_IMAGE_NAMESPACE

namespace std {

/// A functor that allows for hashing image image plane desc.
template<>
struct hash<RAPID_IMAGE_NAMESPACE::PlaneDesc> {
    size_t operator()(const RAPID_IMAGE_NAMESPACE::PlaneDesc & key) const {
        // Records the calculated hash of the texture handle.
        size_t h = 7;

        // Hash the members.
        h = 79 * h + (size_t) key.format;
        h = 79 * h + (size_t) key.extent.w;
        h = 79 * h + (size_t) key.extent.h;
        h = 79 * h + (size_t) key.extent.d;
        h = 79 * h + (size_t) key.step;
        h = 79 * h + (size_t) key.pitch;
        h = 79 * h + (size_t) key.slice;
        h = 79 * h + (size_t) key.size;
        h = 79 * h + (size_t) key.offset;

        return h;
    }
};

/// A functor that allows for hashing image descriptions.
template<>
struct hash<RAPID_IMAGE_NAMESPACE::ImageDesc> {
    size_t operator()(const RAPID_IMAGE_NAMESPACE::ImageDesc & key) const {
        // Records the calculated hash of the texture handle.
        size_t h = 7;

        // Hash the members.
        h = 79 * h + (size_t) key.layers;
        h = 79 * h + (size_t) key.levels;
        h = 79 * h + (size_t) key.size;

        /// Allows us to hash the planes.
        std::hash<RAPID_IMAGE_NAMESPACE::PlaneDesc> planeHasher;

        // Calculate the hash of the planes.
        for (const RAPID_IMAGE_NAMESPACE::PlaneDesc & plane : key.planes) { h = 79 * h + planeHasher(plane); }

        return h;
    }
};

} // namespace std

#endif // RAPID_IMAGE_H_

#ifdef RAPID_IMAGE_IMPLEMENTATION
#include "rapid-image.cpp"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
