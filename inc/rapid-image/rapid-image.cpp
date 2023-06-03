#undef RAPID_IMAGE_IMPLEMENTATION
#include "rapid-image.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include <numeric>
#include <cstring>
#include <filesystem>

namespace RAPID_IMAGE_NAMESPACE {

#ifdef __clang__
// W/o this line, clang will complain undefined symbol.
constexpr PixelFormat::LayoutDesc PixelFormat::LAYOUTS[];
#endif

// *********************************************************************************************************************
// Utilty functions
// *********************************************************************************************************************

void * aalloc(size_t a, size_t s) {
#ifdef _WIN32
    return _aligned_malloc(s, a);
#else
    return aligned_alloc(a, s);
#endif
}

void afree(void * p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    ::free(p);
#endif
}

// *********************************************************************************************************************
// PixelFormat
// *********************************************************************************************************************

// ---------------------------------------------------------------------------------------------------------------------
/// Convert a float to one color channel, based on the INITIAL channel format/sign prior to the reconversion
static inline uint32_t fromFloat(float value, uint32_t width, PixelFormat::Sign sign) {
    auto castFromFloat = [](float fp) {
        union {
            uint32_t u;
            float    f;
        };
        f = fp;
        return u;
    };
    uint32_t mask = (width < 32) ? ((1u << width) - 1) : (uint32_t) -1;
    switch (sign) {
    case PixelFormat::SIGN_UNORM:
        if (value < 0.0f)
            value = 0.0f;
        else if (value > 1.0f)
            value = 1.0f;
        return (uint32_t) (value * mask);

    case PixelFormat::SIGN_FLOAT:
        // 10 bits    =>                         EE EEEFFFFF
        // 11 bits    =>                        EEE EEFFFFFF
        // Half bits  =>                   SEEEEEFF FFFFFFFF
        // Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF
        // 0x0000001F => 00000000 00000000 00000000 00011111
        // 0x0000003F => 00000000 00000000 00000000 00111111
        // 0x000003E0 => 00000000 00000000 00000011 11100000
        // 0x000007C0 => 00000000 00000000 00000111 11000000
        // 0x00007C00 => 00000000 00000000 01111100 00000000
        // 0x000003FF => 00000000 00000000 00000011 11111111
        // 0x38000000 => 00111000 00000000 00000000 00000000
        // 0x7f800000 => 01111111 10000000 00000000 00000000
        // 0x00008000 => 00000000 00000000 10000000 00000000
        if (width == 32) {
            return castFromFloat(value);
        } else if (width == 16) {
            if (value == 0.0f)
                return 0u;
            else if (std::isnan(value))
                return ~0u;
            else if (std::isinf(value)) {
                if (value > 0.0f)
                    return 0x1Fu << 10u;
                else
                    return 0x3Fu << 10u;
            }
            uint32_t u32 = castFromFloat(value);
            return ((u32 >> 16) & 0x8000) |                               // sign
                   ((((u32 & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | // exponential
                   ((u32 >> 13) & 0x03ff);                                // Mantissa
        } else if (width == 11) {
            if (value == 0.0f)
                return 0u;
            else if (std::isnan(value))
                return ~0u;
            else if (std::isinf(value))
                return 0x1Fu << 6u;
            uint32_t u32 = castFromFloat(value);
            return ((((u32 & 0x7f800000) - 0x38000000) >> 17) & 0x07c0) | // exponential
                   ((u32 >> 17) & 0x003f);                                // Mantissa
        } else if (width == 10) {
            if (value == 0.0f)
                return 0u;
            else if (std::isnan(value))
                return ~0u;
            else if (std::isinf(value))
                return 0x1Fu << 5u;
            uint32_t u32 = castFromFloat(value);
            return ((((u32 & 0x7f800000) - 0x38000000) >> 18) & 0x03E0) | // exponential
                   ((u32 >> 18) & 0x001f);                                // Mantissa
        } else {
            RII_THROW("unsupported yet.");
        }

    case PixelFormat::SIGN_UINT:
        if (value > mask) return mask;
        return (uint32_t) value;

    case PixelFormat::SIGN_SNORM:
    case PixelFormat::SIGN_GNORM:
    case PixelFormat::SIGN_BNORM:
    case PixelFormat::SIGN_SINT:
    case PixelFormat::SIGN_GINT:
    case PixelFormat::SIGN_BINT:
    default:
        // not supported yet.
        RII_THROW("unsupported yet.");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
/// Convert one color channel to float, based on the channel format/sign
/// \param value The channel value
/// \param width The number of valid bits in that value
/// \param sign  The channel's data format
static inline float tofloat(uint32_t value, uint32_t width, PixelFormat::Sign sign) {
    auto castToFloat = [](uint32_t u32) {
        union {
            ;
            float    f;
            uint32_t u;
        };
        u = u32;
        return f;
    };
    uint32_t mask = (width < 32) ? ((1u << width) - 1) : (uint32_t) -1;
    value &= mask;
    switch (sign) {
    case PixelFormat::SIGN_UNORM:
        return (float) value / (float) mask;

    case PixelFormat::SIGN_FLOAT:
        // 10 bits    =>                         EE EEEFFFFF
        // 11 bits    =>                        EEE EEFFFFFF
        // 16 bits    =>                   SEEEEEFF FFFFFFFF
        // Float bits => SEEEEEEE EFFFFFFF FFFFFFFF FFFFFFFF
        // 0x0000001F => 00000000 00000000 00000000 00011111
        // 0x0000003F => 00000000 00000000 00000000 00111111
        // 0x000003E0 => 00000000 00000000 00000011 11100000
        // 0x000007C0 => 00000000 00000000 00000111 11000000
        // 0x00007C00 => 00000000 00000000 01111100 00000000
        // 0x000003FF => 00000000 00000000 00000011 11111111
        // 0x38000000 => 00111000 00000000 00000000 00000000
        // 0x7f800000 => 01111111 10000000 00000000 00000000
        // 0x00008000 => 00000000 00000000 10000000 00000000
        if (width == 32) {
            return castToFloat(value);
        } else if (width == 16) {
            uint32_t u32 = ((value & 0x8000) << 16) | (((value & 0x7c00) + 0x1C000) << 13) | // exponential
                           ((value & 0x03FF) << 13);                                         // mantissa
            return castToFloat(u32);
        } else if (width == 11) {
            uint32_t u32 = ((((value & 0x07c0) << 17) + 0x38000000) & 0x7f800000) | // exponential
                           ((value & 0x003f) << 17);                                // Mantissa
            return castToFloat(u32);
        } else if (width == 10) {
            uint32_t u32 = ((((value & 0x03E0) << 18) + 0x38000000) & 0x7f800000) | // exponential
                           ((value & 0x001f) << 18);                                // Mantissa
            return castToFloat(u32);
        } else {
            RII_THROW("unsupported yet.");
        }

    case PixelFormat::SIGN_UINT:
        return (float) value;

    case PixelFormat::SIGN_SNORM:
    case PixelFormat::SIGN_GNORM:
    case PixelFormat::SIGN_BNORM:
    case PixelFormat::SIGN_SINT:
    case PixelFormat::SIGN_GINT:
    case PixelFormat::SIGN_BINT:
    default:
        // not supported yet.
        RII_THROW("unsupported yet.");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
uint128_t PixelFormat::loadFromFloat4(const float4 & pixel) const {
    const PixelFormat::LayoutDesc & ld = layoutDesc();

    RII_ASSERT(1 == ld.blockWidth && 1 == ld.blockHeight); // do not support compressed format.

    uint128_t result = {};

    auto convertToChannel = [&](uint32_t swizzle) {
        const auto & ch   = ld.channels[swizzle];
        auto         sign = (PixelFormat::Sign)((swizzle < 3) ? sign012 : sign3);
        uint32_t     value;
        switch (swizzle) {
        case PixelFormat::SWIZZLE_X:
            value = fromFloat(pixel.x, ch.bits, sign);
            break;
        case PixelFormat::SWIZZLE_Y:
            value = fromFloat(pixel.y, ch.bits, sign);
            break;
        case PixelFormat::SWIZZLE_Z:
            value = fromFloat(pixel.z, ch.bits, sign);
            break;
        case PixelFormat::SWIZZLE_W:
            value = fromFloat(pixel.w, ch.bits, sign);
            break;
        case PixelFormat::SWIZZLE_0:
            value = 0u;
            break;
        case PixelFormat::SWIZZLE_1:
            value = 1u;
            break;
        default:
            RII_THROW("invalid enumeration.");
        }
        result.set(value, ch.shift, ch.bits);
    };

    convertToChannel(swizzle0);
    convertToChannel(swizzle1);
    convertToChannel(swizzle2);
    convertToChannel(swizzle3);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
/// Convert pixel of arbitrary format to float4. Do not support compressed format.
float4 PixelFormat::storeToFloat4(const void * pixel) const {
    const PixelFormat::LayoutDesc & ld = layoutDesc();

    RII_ASSERT(1 == ld.blockWidth && 1 == ld.blockHeight); // do not support compressed format.

    auto src = uint128_t::make(pixel, ld.blockBytes);

    // lambda to convert one channel
    auto convertChannel = [&](uint32_t swizzle) {
        if (PixelFormat::SWIZZLE_0 == swizzle) return 0.f;
        if (PixelFormat::SWIZZLE_1 == swizzle) return 1.f;
        const auto & ch   = ld.channels[swizzle];
        auto         sign = (PixelFormat::Sign)((swizzle < 3) ? sign012 : sign3);
        return tofloat(src.segment(ch.shift, ch.bits), ch.bits, sign);
    };

    return float4::make(convertChannel(swizzle0), convertChannel(swizzle1), convertChannel(swizzle2), convertChannel(swizzle3));
}

// ---------------------------------------------------------------------------------------------------------------------
/// convert single pixel to RGBA8 format.
static void convertToRGBA8(RGBA8 * result, const PixelFormat::LayoutDesc & ld, const PixelFormat & format, const void * src) {
    if (PixelFormat::RGBA8() == format) {
        // shortcut for RGBA8 format.
        *result = *(const RGBA8 *) src;
    } else if (1 == ld.blockWidth && 1 == ld.blockHeight) {
        // this is the general case that could in theory handle any format.
        auto f4 = format.storeToFloat4(src);
        *result =
            RGBA8::make((uint8_t) std::clamp<uint32_t>((uint32_t) (f4.x * 255.0f), 0, 255), (uint8_t) std::clamp<uint32_t>((uint32_t) (f4.y * 255.0f), 0, 255),
                        (uint8_t) std::clamp<uint32_t>((uint32_t) (f4.z * 255.0f), 0, 255), (uint8_t) std::clamp<uint32_t>((uint32_t) (f4.w * 255.0f), 0, 255));
    } else {
        // decompressed format
        RII_THROW("NOT IMPLEMENTED");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
float PixelFormat::getPixelChannelFloat(const void * pixel, size_t channel) {
    // Get the swizzle for the desired channel.
    unsigned int swizzle;
    switch (channel) {
    case 0:
        swizzle = swizzle0;
        break;
    case 1:
        swizzle = swizzle1;
        break;
    case 2:
        swizzle = swizzle2;
        break;
    case 3:
        swizzle = swizzle3;
        break;
    default:
        RII_THROW("Used invalid channel %zu when channel must be in range [0..3].", channel);
        break;
    }

    if (swizzle == PixelFormat::SWIZZLE_0) { return 0.0f; }
    if (swizzle == PixelFormat::SWIZZLE_1) { return 1.0f; }

    const LayoutDesc & selfLayoutDesc = layoutDesc();

    // Cast the pixel data so we can read it.
    auto src = uint128_t::make(pixel, selfLayoutDesc.blockBytes);

    // Get objects defining how pixels are read.
    const auto & channelDesc = selfLayoutDesc.channels[swizzle];

    auto sign = (PixelFormat::Sign)((swizzle < 3) ? sign012 : sign3);

    return tofloat(src.segment(channelDesc.shift, channelDesc.bits), channelDesc.bits, sign);
}

inline static constexpr PixelFormat DXGI_FORMATS[] = {
    PixelFormat::UNKNOWN(),                // DXGI_FORMAT_UNKNOWN                 = 0,
    PixelFormat::RGBA_32_32_32_32_UINT(),  // DXGI_FORMAT_RGBA_32_32_32_32_UINT   = 1,
    PixelFormat::RGBA_32_32_32_32_FLOAT(), // DXGI_FORMAT_RGBA_32_32_32_32_FLOAT  = 2,
    PixelFormat::RGBA_32_32_32_32_UINT(),  // DXGI_FORMAT_RGBA_32_32_32_32_UINT   = 3,
    PixelFormat::RGBA_32_32_32_32_SINT(),  // DXGI_FORMAT_RGBA_32_32_32_32_SINT   = 4,
    PixelFormat::RGB_32_32_32_UINT(),      // DXGI_FORMAT_RGB_32_32_32_UINT       = 5,
    PixelFormat::RGB_32_32_32_FLOAT(),     // DXGI_FORMAT_RGB_32_32_32_FLOAT      = 6,
    PixelFormat::RGB_32_32_32_UINT(),      // DXGI_FORMAT_RGB_32_32_32_UINT       = 7,
    PixelFormat::RGB_32_32_32_SINT(),      // DXGI_FORMAT_RGB_32_32_32_SINT       = 8,
    PixelFormat::RGBA_16_16_16_16_UINT(),  // DXGI_FORMAT_RGBA_16_16_16_16_UINT   = 9,
    PixelFormat::RGBA_16_16_16_16_FLOAT(), // DXGI_FORMAT_RGBA_16_16_16_16_FLOAT  = 10,
    PixelFormat::RGBA_16_16_16_16_UNORM(), // DXGI_FORMAT_RGBA_16_16_16_16_UNORM  = 11,
    PixelFormat::RGBA_16_16_16_16_UINT(),  // DXGI_FORMAT_RGBA_16_16_16_16_UINT   = 12,
    PixelFormat::RGBA_16_16_16_16_SNORM(), // DXGI_FORMAT_RGBA_16_16_16_16_SNORM  = 13,
    PixelFormat::RGBA_16_16_16_16_SINT(),  // DXGI_FORMAT_RGBA_16_16_16_16_SINT   = 14,
    PixelFormat::RG_32_32_UINT(),          // DXGI_FORMAT_RG_32_32_UINT           = 15,
    PixelFormat::RG_32_32_FLOAT(),         // DXGI_FORMAT_RG_32_32_FLOAT          = 16,
    PixelFormat::RG_32_32_UINT(),          // DXGI_FORMAT_RG_32_32_UINT           = 17,
    PixelFormat::RG_32_32_SINT(),          // DXGI_FORMAT_RG_32_32_SINT           = 18,
    PixelFormat::RGX_32_8_24_UINT(),       // DXGI_FORMAT_RG_32_8X24_UINT         = 19,
    PixelFormat::RGX_32_FLOAT_8_UINT_24(), // DXGI_FORMAT_D32_FLOAT_S8X24_UINT    = 20,
    PixelFormat::RXX_32_8_24_FLOAT(),      // DXGI_FORMAT_R32_FLOAT_X8X24_UINT    = 21,
    PixelFormat::XGX_32_8_24_UINT(),       // DXGI_FORMAT_X32_UINT_G8X24_UINT     = 22,
    PixelFormat::RGBA_10_10_10_2_UINT(),   // DXGI_FORMAT_RGBA_10_10_10_2_UINT    = 23,
    PixelFormat::RGBA_10_10_10_2_UNORM(),  // DXGI_FORMAT_RGBA_10_10_10_2_UNORM   = 24,
    PixelFormat::RGBA_10_10_10_2_UINT(),   // DXGI_FORMAT_RGBA_10_10_10_2_UINT    = 25,
    PixelFormat::RGB_11_11_10_FLOAT(),     // DXGI_FORMAT_RGB_11_11_10_FLOAT      = 26,
    PixelFormat::RGBA_8_8_8_8_UINT(),      // DXGI_FORMAT_RGBA_8_8_8_8_UINT       = 27,
    PixelFormat::RGBA_8_8_8_8_UNORM(),     // DXGI_FORMAT_RGBA_8_8_8_8_UNORM      = 28,
    PixelFormat::RGBA_8_8_8_8_SRGB(),      // DXGI_FORMAT_RGBA_8_8_8_8_UNORM_SRGB = 29,
    PixelFormat::RGBA_8_8_8_8_UINT(),      // DXGI_FORMAT_RGBA_8_8_8_8_UINT       = 30,
    PixelFormat::RGBA_8_8_8_8_SNORM(),     // DXGI_FORMAT_RGBA_8_8_8_8_SNORM      = 31,
    PixelFormat::RGBA_8_8_8_8_SINT(),      // DXGI_FORMAT_RGBA_8_8_8_8_SINT       = 32,
    PixelFormat::RG_16_16_UINT(),          // DXGI_FORMAT_RG_16_16_UINT           = 33,
    PixelFormat::RG_16_16_FLOAT(),         // DXGI_FORMAT_RG_16_16_FLOAT          = 34,
    PixelFormat::RG_16_16_UNORM(),         // DXGI_FORMAT_RG_16_16_UNORM          = 35,
    PixelFormat::RG_16_16_UINT(),          // DXGI_FORMAT_RG_16_16_UINT           = 36,
    PixelFormat::RG_16_16_SNORM(),         // DXGI_FORMAT_RG_16_16_SNORM          = 37,
    PixelFormat::RG_16_16_SINT(),          // DXGI_FORMAT_RG_16_16_SINT           = 38,
    PixelFormat::R_32_UINT(),              // DXGI_FORMAT_R32_UINT                = 39,
    PixelFormat::R_32_FLOAT(),             // DXGI_FORMAT_D32_FLOAT               = 40,
    PixelFormat::R_32_FLOAT(),             // DXGI_FORMAT_R32_FLOAT               = 41,
    PixelFormat::R_32_UINT(),              // DXGI_FORMAT_R32_UINT                = 42,
    PixelFormat::R_32_SINT(),              // DXGI_FORMAT_R32_SINT                = 43,
    PixelFormat::RG_24_8_UINT(),           // DXGI_FORMAT_RG_24_8_UINT            = 44,
    PixelFormat::RG_24_UNORM_8_UINT(),     // DXGI_FORMAT_D24_UNORM_S8_UINT       = 45,
    PixelFormat::RX_24_8_UNORM(),          // DXGI_FORMAT_R24_UNORM_X8_UINT       = 46,
    PixelFormat::XG_24_8_UINT(),           // DXGI_FORMAT_X24_UINT_G8_UINT        = 47,
    PixelFormat::RG_8_8_UINT(),            // DXGI_FORMAT_RG_8_8_UINT             = 48,
    PixelFormat::RG_8_8_UNORM(),           // DXGI_FORMAT_RG_8_8_UNORM            = 49,
    PixelFormat::RG_8_8_UINT(),            // DXGI_FORMAT_RG_8_8_UINT             = 50,
    PixelFormat::RG_8_8_SNORM(),           // DXGI_FORMAT_RG_8_8_SNORM            = 51,
    PixelFormat::RG_8_8_SINT(),            // DXGI_FORMAT_RG_8_8_SINT             = 52,
    PixelFormat::R_16_UINT(),              // DXGI_FORMAT_R16_UINT                = 53,
    PixelFormat::R_16_FLOAT(),             // DXGI_FORMAT_R16_FLOAT               = 54,
    PixelFormat::R_16_UNORM(),             // DXGI_FORMAT_D16_UNORM               = 55,
    PixelFormat::R_16_UNORM(),             // DXGI_FORMAT_R16_UNORM               = 56,
    PixelFormat::R_16_UINT(),              // DXGI_FORMAT_R16_UINT                = 57,
    PixelFormat::R_16_SNORM(),             // DXGI_FORMAT_R16_SNORM               = 58,
    PixelFormat::R_16_SINT(),              // DXGI_FORMAT_R16_SINT                = 59,
    PixelFormat::R_8_UINT(),               // DXGI_FORMAT_R8_UINT                 = 60,
    PixelFormat::R_8_UNORM(),              // DXGI_FORMAT_R8_UNORM                = 61,
    PixelFormat::R_8_UINT(),               // DXGI_FORMAT_R8_UINT                 = 62,
    PixelFormat::R_8_SNORM(),              // DXGI_FORMAT_R8_SNORM                = 63,
    PixelFormat::R_8_SINT(),               // DXGI_FORMAT_R8_SINT                 = 64,
    PixelFormat::A_8_UNORM(),              // DXGI_FORMAT_A8_UNORM                = 65,
    PixelFormat::UNKNOWN(),                // DXGI_FORMAT_R1_UNORM                = 66,
    PixelFormat::UNKNOWN(),                // DXGI_FORMAT_RGB_9_9_9E5_SHAREDEXP   = 67,
    PixelFormat::UNKNOWN(),                // DXGI_FORMAT_RG_8_8_BG_8_8_UNORM     = 68,
    PixelFormat::UNKNOWN(),                // DXGI_FORMAT_GR_8_8_GB_8_8_UNORM     = 69,
    PixelFormat::DXT1_UINT(),              // DXGI_FORMAT_BC1_UINT                = 70,
    PixelFormat::DXT1_UNORM(),             // DXGI_FORMAT_BC1_UNORM               = 71,
    PixelFormat::DXT1_SRGB(),              // DXGI_FORMAT_BC1_UNORM_SRGB          = 72,
    PixelFormat::DXT2_UINT(),              // DXGI_FORMAT_BC2_UINT                = 73,
    PixelFormat::DXT2_UNORM(),             // DXGI_FORMAT_BC2_UNORM               = 74,
    PixelFormat::DXT2_SRGB(),              // DXGI_FORMAT_BC2_UNORM_SRGB          = 75,
    PixelFormat::DXT3_UINT(),              // DXGI_FORMAT_BC3_UINT                = 76,
    PixelFormat::DXT3_UNORM(),             // DXGI_FORMAT_BC3_UNORM               = 77,
    PixelFormat::DXT3_SRGB(),              // DXGI_FORMAT_BC3_UNORM_SRGB          = 78,
    PixelFormat::DXT4_UINT(),              // DXGI_FORMAT_BC4_UINT                = 79,
    PixelFormat::DXT4_UNORM(),             // DXGI_FORMAT_BC4_UNORM               = 80,
    PixelFormat::DXT4_SNORM(),             // DXGI_FORMAT_BC4_SNORM               = 81,
    PixelFormat::DXT5_UINT(),              // DXGI_FORMAT_BC5_UINT                = 82,
    PixelFormat::DXT5_UNORM(),             // DXGI_FORMAT_BC5_UNORM               = 83,
    PixelFormat::DXT5_SNORM(),             // DXGI_FORMAT_BC5_SNORM               = 84,
    PixelFormat::BGR_5_6_5_UNORM(),        // DXGI_FORMAT_BGR_5_6_5_UNORM         = 85,
    PixelFormat::BGRA_5_5_5_1_UNORM(),     // DXGI_FORMAT_BGRA_5_5_5_1_UNORM      = 86,
    PixelFormat::BGRA_8_8_8_8_UNORM(),     // DXGI_FORMAT_BGRA_8_8_8_8_UNORM      = 87,
    PixelFormat::UNKNOWN(),                // DXGI_FORMAT_BGR_8_8_8X8_UNORM       = 88,
};
static_assert(std::size(DXGI_FORMATS) == 89);

PixelFormat PixelFormat::makeFromDXGIFormat(uint32_t dxgiFormat) {
    PixelFormat result = PixelFormat::UNKNOWN();
    if (dxgiFormat < std::size(DXGI_FORMATS)) { result = DXGI_FORMATS[dxgiFormat]; }
    if (!result.valid()) { RAPID_IMAGE_LOGE("unsupported DXGI format: %u", dxgiFormat); }
    return result;
}

// *********************************************************************************************************************
// PlaneDesc
// *********************************************************************************************************************

bool PlaneDesc::valid() const {
    // check format
    if (!format.valid()) {
        RAPID_IMAGE_LOGE("invalid format");
        return false;
    }

    // check dimension
    if (extent.empty()) {
        RAPID_IMAGE_LOGE("dimension can't zero!");
        return false;
    }

    // check pitches
    auto & cld             = format.layoutDesc();
    auto   numBlocksPerRow = (uint32_t) ((extent.w + cld.blockWidth - 1) / cld.blockWidth);
    auto   numBlocksPerCol = (uint32_t) ((extent.h + cld.blockHeight - 1) / cld.blockHeight);
    if (step < cld.blockBytes) {
        RAPID_IMAGE_LOGE("step is too small!");
        return false;
    }
    if (pitch < numBlocksPerRow * cld.blockBytes) {
        RAPID_IMAGE_LOGE("pitch is too small!");
        return false;
    }
    if (slice < numBlocksPerCol * numBlocksPerRow * cld.blockBytes) {
        RAPID_IMAGE_LOGE("slice is too small!");
        return false;
    }
    if (size < slice * extent.d) {
        RAPID_IMAGE_LOGE("size is too small!");
        return false;
    }

    // check alignment
    if (pitch % cld.blockBytes) {
        RAPID_IMAGE_LOGE("Pitch is not aligned to the pixel block boundary.");
        return false;
    }
    if (offset % cld.blockBytes) { RAPID_IMAGE_LOGE("Offset is not aligned to the pixel block boundary."); }
    if (slice % cld.blockBytes) {
        RAPID_IMAGE_LOGE("Slice is not aligned to the pixel block boundary.");
        return false;
    }

    // done
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
//
PlaneDesc PlaneDesc::make(PixelFormat format, const Extent3D & extent, size_t step, size_t pitch, size_t slice) {
    if (!format.valid()) {
        RAPID_IMAGE_LOGE("invalid color format: 0x%X", format.u32);
        return {};
    }

    const auto & fd = format.layoutDesc();

    // calculate alignments
    // if (0 != alignment && !isPowerOf2(alignment)) {
    //     RAPID_IMAGE_LOGE("image alignment (%d) must be 0 or power of 2.", alignment);
    //     return {};
    // }
    // alignment = ceilPowerOf2(std::max((uint32_t)alignment, (uint32_t)fd.blockBytes));

    PlaneDesc p;
    p.format   = format;
    p.extent.w = extent.w ? extent.w : 1;
    p.extent.h = extent.h ? extent.h : 1;
    p.extent.d = extent.d ? extent.d : 1;
    // Check for ASTC format enumerations
    p.step = std::max((uint32_t) step, (uint32_t) (fd.blockBytes));
    // p.alignment = (uint32_t)alignment;

    // calculate row pitch, aligned to rowAlignment.
    // Note that we can't just use nextMultiple here, since rowAlignment might not be power of 2.
    auto numBlocksPerRow = (uint32_t) ((extent.w + fd.blockWidth - 1) / fd.blockWidth);
    p.pitch              = std::max(p.step * numBlocksPerRow, (uint32_t) pitch);
    auto numBlocksPerCol = (uint32_t) ((extent.h + fd.blockHeight - 1) / fd.blockHeight);
    p.slice              = std::max(p.pitch * numBlocksPerCol, (uint32_t) slice);
    RII_ASSERT(p.pitch > 0 && p.slice > 0);

    // calculate plane size of the image.
    p.size = p.slice * p.extent.d;

    // done
    RII_ASSERT(p.valid());
    return p;
}

// // ---------------------------------------------------------------------------------------------------------------------
// //
// void PlaneDesc::save(const std::string & filename, const void * pixels) const {
//     std::ofstream file(filename, std::ios::binary);
//     if (!file) {
//         RAPID_IMAGE_LOGE("failed to open file %s for writing.", filename.c_str());
//         return;
//     }
//     auto ext = fs::path(filename).extension().string();
//     std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return (char) tolower(c); });
//     if (".jpg" == ext || ".jpeg" == ext) {
//         saveToJPG(file, pixels);
//     } else if (".png" == ext) {
//         saveToPNG(file, pixels);
//     } else if (".hdr" == ext) {
//         saveToHDR(file, pixels);
//     } else if (".raw" == ext) {
//         saveToRAW(file, pixels);
//     } else {
//         RAPID_IMAGE_LOGE("Unsupported file extension: %s", ext.c_str());
//     }
// }

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<float4> PlaneDesc::toFloat4(const void * pixels) const {
    if (empty()) {
        RAPID_IMAGE_LOGE("Can't save empty image plane.");
        return {};
    }
    auto ld = format.layoutDesc();
    if (ld.blockWidth > 1 || ld.blockHeight > 1) {
        RAPID_IMAGE_LOGE("Do not support compressed texture format yet.");
        return {};
    }
    const uint8_t *     p = (const uint8_t *) pixels;
    std::vector<float4> colors;
    colors.reserve(extent.w * extent.h * extent.d);
    for (uint32_t z = 0; z < extent.d; ++z) {
        for (uint32_t y = 0; y < extent.h; ++y) {
            for (uint32_t x = 0; x < extent.w; ++x) { colors.push_back(format.storeToFloat4(p + pixel(x, y, z))); }
        }
    }
    return colors;
}

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<RGBA8> PlaneDesc::toRGBA8(const void * pixels) const {
    if (empty()) {
        RAPID_IMAGE_LOGE("Can't save empty image plane.");
        return {};
    }
    const uint8_t *    p  = (const uint8_t *) pixels;
    auto               ld = format.layoutDesc();
    std::vector<RGBA8> colors;
    std::vector<RGBA8> block(ld.blockWidth * ld.blockHeight);

    colors.resize(extent.w * extent.h * extent.d);
    for (uint32_t z = 0; z < extent.d; ++z) {
        for (uint32_t y = 0; y < extent.h; y += ld.blockHeight) {
            for (uint32_t x = 0; x < extent.w; x += ld.blockWidth) {
                // convert the block
                convertToRGBA8(block.data(), ld, format, p + pixel(x, y, z));

                // copy the block to output buffer
                uint32_t right  = std::min(x + ld.blockWidth, extent.w);
                uint32_t bottom = std::min(y + ld.blockHeight, extent.h);
                for (uint32_t yy = y; yy < bottom; ++yy) {
                    for (uint32_t xx = x; xx < right; ++xx) {
                        colors[z * extent.w * extent.h + yy * extent.w + xx] = block[(yy - y) * ld.blockWidth + (xx - x)];
                    }
                }
            }
        }
    }
    return colors;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void PlaneDesc::fromFloat4(void * dst, size_t dstSize, size_t dstZ, const void * src) const {
    if (empty()) {
        RAPID_IMAGE_LOGE("Can't load data to empty image plane.");
        return;
    }
    const float4 * p  = (const float4 *) src;
    auto           ld = format.layoutDesc();
    if (ld.blockWidth > 1 || ld.blockHeight > 1) {
        RAPID_IMAGE_LOGE("does not support loading pixel data to compressed image plane.");
        return;
    }
    for (uint32_t y = 0; y < extent.h; ++y) {
        for (uint32_t x = 0; x < extent.w; ++x) {
            size_t dstOffset = pixel(x, y, dstZ);
            if ((dstOffset + ld.blockBytes) > dstSize) {
                RAPID_IMAGE_LOGE("Destination buffer size (%zu) is not large enough.", dstSize);
                return;
            }
            uint8_t * d    = (uint8_t *) dst + dstOffset;
            uint128_t temp = format.loadFromFloat4(*(p + y * extent.w + x));
            memcpy(d, &temp, ld.blockBytes); // TODO: Can we avoid this memcpy's?
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
Image PlaneDesc::generateMipmaps(const void * pixels, size_t maxLevels) const {
    struct Local {
        // static int imax(int a, int b) {
        //     if (a > b) return a;
        //     return b;
        // }

        // static int imin(int a, int b) {
        //     if (a < b) return a;
        //     return b;
        // }

        // static float lerp(float v0, float v1) { return 0.5f * v0 + 0.5f * v1; }

        // static float4 lerpPixel(const float4 & p0, const float4 & p1) {
        //     float4 result;
        //     result.x = lerp(p0.x, p1.x);
        //     result.y = lerp(p0.y, p1.y);
        //     result.z = lerp(p0.z, p1.z);
        //     result.w = lerp(p0.w, p1.w);
        //     return result;
        // }

        static void generateMipmap(uint8_t * data, const PlaneDesc & src, const PlaneDesc & dst) {
            RII_ASSERT(src.extent.w == 1 || src.extent.w == dst.extent.w * 2);
            RII_ASSERT(src.extent.h == 1 || src.extent.h == dst.extent.h * 2);
            RII_ASSERT(src.extent.d == 1 || src.extent.d == dst.extent.d * 2);
            auto sx = src.extent.w / dst.extent.w;
            auto sy = src.extent.h / dst.extent.h;
            auto sz = src.extent.d / dst.extent.d;
            auto pc = sx * sy * sz;                       // pixel count
            auto ps = src.format.layoutDesc().blockBytes; // pixel size
            RII_ASSERT(pc <= 8);
            RII_ASSERT(ps <= sizeof(uint128_t));
            for (size_t z = 0; z < dst.extent.d; ++z) {
                for (size_t y = 0; y < dst.extent.h; ++y) {
                    for (size_t x = 0; x < dst.extent.w; ++x) {
                        // [x * sx, y * sy, z * sz] defines the corner pixel in the source image
                        // [sx, sy, sz] defines the extent of the pixel block in the source image
                        float4 sum = {{0.0f, 0.0f, 0.0f, 0.0f}};
                        for (size_t i = 0; i < pc; ++i) {
                            auto xx = x * sx + i % sx;
                            auto yy = y * sy + (i / sx) % sy;
                            auto zz = z * sz + i / (sx * sy);
                            sum += src.format.storeToFloat4(data + src.pixel(xx, yy, zz));
                        }
                        sum *= 1.0f / pc;
                        auto avg = dst.format.loadFromFloat4(sum);
                        memcpy(data + dst.pixel(x, y, z), &avg, ps);
                    }
                }
            }
            // float widthDivisor  = static_cast<float>(imax(static_cast<int>(dstWidth) - 1, 1));
            // float heightDivisor = static_cast<float>(imax(static_cast<int>(dstHeight) - 1, 1));
            // for (int i = 0; i < static_cast<int>(dstHeight); ++i) {
            //     int y0 = static_cast<int>(i * (static_cast<int>(srcHeight) - 1) / heightDivisor);
            //     int y1 = imin(y0 + 1, static_cast<int>(srcHeight) - 1);
            //     for (int j = 0; j < static_cast<int>(dstWidth); ++j) {
            //         int    x0               = static_cast<int>(j * (static_cast<int>(srcWidth) - 1) / widthDivisor);
            //         int    x1               = imin(x0 + 1, static_cast<int>(srcWidth) - 1);
            //         float4 p0               = src[(srcWidth * y0) + x0];m
            //         float4 p1               = src[(srcWidth * y0) + x1];
            //         float4 p2               = src[(srcWidth * y1) + x0];
            //         float4 p3               = src[(srcWidth * y1) + x1];
            //         float4 v0               = lerpPixel(p0, p1);
            //         float4 v1               = lerpPixel(p2, p3);
            //         float4 pixel            = lerpPixel(v0, v1);
            //         dst[(dstWidth * i) + j] = pixel;
            //     }
            // }
        }
    };

    // create the result image
    Image        result = Image(ImageDesc {}.reset(PlaneDesc::make(format, extent), 1, maxLevels));
    const auto & desc   = result.desc();

    // Copy data into the base map of the result image.
    auto & base = result.plane(0, 0);
    memcpy(result.data() + base.offset, pixels, base.size);

    for (size_t i = 0; i < desc.planes.size(); ++i) {
        size_t layer = i % desc.layers;
        size_t level = i / desc.layers;
        if (0 == level) continue; // skip the base map.
        const auto & src = desc.planes[desc.index(layer, level - 1)];
        const auto & dst = desc.planes[i];
        Local::generateMipmap(result.data(), src, dst);
    }

    return result;
}

// *********************************************************************************************************************
// RIL Image
// *********************************************************************************************************************

#pragma pack(push, 4)
struct RILFileTag {
    const char tag[4] = {'R', 'I', 'L', '_'};
    uint32_t   version;
    explicit RILFileTag(uint32_t version_ = 0): version(version_) {}

    bool valid() const { return tag[0] == 'R' && tag[1] == 'I' && tag[2] == 'L' && tag[3] == '_' && version > 0; }
};

struct RILHeaderV1 {
    /// this is to ensure that we don't change the layout of PlaneDesc accidentally w/o changing the file version number.
    const uint32_t headerSize    = sizeof(RILHeaderV1);
    const uint32_t planeDescSize = sizeof(PlaneDesc);
    const uint32_t offset        = sizeof(RILFileTag) + sizeof(RILHeaderV1); ///< offset to the plane array
    uint32_t       layers        = 0;
    uint32_t       levels        = 0;
    uint32_t       alignment     = 0;
    uint64_t       size          = 0; ///< total size of the pixel array.

    bool valid() const {
        return headerSize == sizeof(RILHeaderV1) && planeDescSize == sizeof(PlaneDesc) && offset == (sizeof(RILFileTag) + sizeof(RILHeaderV1));
    }

    bool empty() const { return 0 == size || 0 == layers || 0 == levels; }
};
#pragma pack(pop)

bool checkedRead(std::istream & stream, const char * action, void * buffer, size_t size) {
    stream.read((char *) buffer, size);
    if (!stream) {
        RAPID_IMAGE_LOGE("failed to %s: stream is not in good state.", action);
        return false;
    }
    return true;
};

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc::AlignedUniquePtr ImageDesc::loadFromRIL(std::istream & stream) {
    if (!stream) {
        RAPID_IMAGE_LOGE("failed to write image to stream: stream is in good state.");
        return {};
    }

    // read file tag
    RILFileTag tag;
    if (!checkedRead(stream, "read image tag", &tag, sizeof(tag))) return {};
    if (!tag.valid()) {
        RAPID_IMAGE_LOGE("failed to read image from stream: Invalid file tag. The stream is probably not a RIL file.");
        return {};
    }

    // check file version
    if (1 == tag.version) {
        // read V1 file
        RILHeaderV1 header;
        if (!checkedRead(stream, "read V1 header", &header, sizeof(header))) return {};
        if (!header.valid()) {
            RAPID_IMAGE_LOGE("failed to read image from stream: Invalid file header.");
            return {};
        }
        if (header.empty()) {
            RAPID_IMAGE_LOGE("failed to read image from stream: empty image.");
            return {};
        }
        // read image descriptor
        ImageDesc desc;
        desc.layers    = header.layers;
        desc.levels    = header.levels;
        desc.size      = header.size;
        desc.alignment = header.alignment;
        desc.planes    = std::vector<PlaneDesc>(header.layers * header.levels);
        if (!checkedRead(stream, "read image planes", desc.planes.data(), desc.planes.size() * sizeof(PlaneDesc))) return {};
        if (!desc.valid()) {
            RAPID_IMAGE_LOGE("failed to read image from stream: Invalid image descriptor.");
            return {};
        }
        // read pixel array
        auto pixels = AlignedUniquePtr((uint8_t *) aalloc(header.alignment, desc.size));
        if (!checkedRead(stream, "read pixels", pixels.get(), desc.size)) return {};
        // done
        *this = std::move(desc);
        return pixels;
    } else {
        RAPID_IMAGE_LOGE("failed to read image from stream: unsupported file version %u.", tag.version);
        return {};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ImageDesc::saveToRIL(std::ostream & stream, const void * pixels) const {
    if (!stream) {
        RAPID_IMAGE_LOGE("failed to write image to stream: stream is not good.");
        return;
    }
    if (!pixels) {
        RAPID_IMAGE_LOGE("failed to write image to stream: pixel array is null.");
        return;
    }

    auto planeArraySize = planes.size() * sizeof(PlaneDesc);

    // write file tag
    RILFileTag tag(1);
    stream.write((const char *) &tag, sizeof(tag));

    // write file header
    RILHeaderV1 header;
    header.size      = size;
    header.layers    = layers;
    header.levels    = levels;
    header.alignment = alignment;
    stream.write((const char *) &header, sizeof(header));

    // write plane array
    stream.write((const char *) planes.data(), planeArraySize);

    // write pixel array
    stream.write((const char *) pixels, size);
}

// *********************************************************************************************************************
// DDS Image
// *********************************************************************************************************************

// Check out this page for the layout of DDS file format: https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide

constexpr uint32_t MAKE_FOURCC(char ch0, char ch1, char ch2, char ch3) {
    return ((uint32_t) (uint8_t) (ch0) | ((uint32_t) (uint8_t) (ch1) << 8) | ((uint32_t) (uint8_t) (ch2) << 16) | ((uint32_t) (uint8_t) (ch3) << 24));
}

struct DDPixelFormat {
    uint32_t size;   ///< size of this structure
    uint32_t flags;  ///< pixel format flags
    uint32_t fourcc; ///< fourcc
    uint32_t bits;   ///< bits of the format
    uint32_t rMask;  ///< R, Y
    uint32_t gMask;  ///< G, U
    uint32_t bMask;  ///< B, V
    uint32_t aMask;  ///< A, A
};

enum DdsFlag {
    DDS_DDPF_SIZE                = sizeof(DDPixelFormat),
    DDS_DDPF_ALPHAPIXELS         = 0x00000001,
    DDS_DDPF_ALPHA               = 0x00000002,
    DDS_DDPF_FOURCC              = 0x00000004,
    DDS_DDPF_PALETTEINDEXED8     = 0x00000020,
    DDS_DDPF_RGB                 = 0x00000040,
    DDS_DDPF_ZBUFFER             = 0x00000400,
    DDS_DDPF_STENCILBUFFER       = 0x00004000,
    DDS_DDPF_LUMINANCE           = 0x00020000,
    DDS_DDPF_BUMPLUMINANCE       = 0x00040000,
    DDS_DDPF_BUMPDUDV            = 0x00080000,
    DDS_DDSD_CAPS                = 0x00000001,
    DDS_DDSD_HEIGHT              = 0x00000002,
    DDS_DDSD_WIDTH               = 0x00000004,
    DDS_DDSD_PIXELFORMAT         = 0x00001000,
    DDS_DDSD_MIPMAPCOUNT         = 0x00020000,
    DDS_DDSD_DEPTH               = 0x00800000,
    DDS_CAPS_ALPHA               = 0x00000002,
    DDS_CAPS_COMPLEX             = 0x00000008,
    DDS_CAPS_PALETTE             = 0x00000100,
    DDS_CAPS_TEXTURE             = 0x00001000,
    DDS_CAPS_MIPMAP              = 0x00400000,
    DDS_CAPS2_CUBEMAP            = 0x00000200,
    DDS_CAPS2_CUBEMAP_ALLFACES   = 0x0000fc00,
    DDS_CAPS2_VOLUME             = 0x00200000,
    DDS_FOURCC_UYVY              = MAKE_FOURCC('U', 'Y', 'V', 'Y'),
    DDS_FOURCC_RG_8_8_BG_8_8     = MAKE_FOURCC('R', 'G', 'B', 'G'),
    DDS_FOURCC_YUY2              = MAKE_FOURCC('Y', 'U', 'Y', '2'),
    DDS_FOURCC_GR_8_8_GB_8_8     = MAKE_FOURCC('G', 'R', 'G', 'B'),
    DDS_FOURCC_DXT1              = MAKE_FOURCC('D', 'X', 'T', '1'),
    DDS_FOURCC_DXT2              = MAKE_FOURCC('D', 'X', 'T', '2'),
    DDS_FOURCC_DXT3              = MAKE_FOURCC('D', 'X', 'T', '3'),
    DDS_FOURCC_DXT4              = MAKE_FOURCC('D', 'X', 'T', '4'),
    DDS_FOURCC_DXT5              = MAKE_FOURCC('D', 'X', 'T', '5'),
    DDS_FOURCC_ABGR_16_16_16_16  = 36,
    DDS_FOURCC_Q16W16V16U16      = 110,
    DDS_FOURCC_R16F              = 111,
    DDS_FOURCC_GR_16_16F         = 112,
    DDS_FOURCC_ABGR_16_16_16_16F = 113,
    DDS_FOURCC_R32F              = 114,
    DDS_FOURCC_GR_32_32F         = 115,
    DDS_FOURCC_ABGR_32_32_32_32F = 116,
    DDS_FOURCC_CxV8U8            = 117,
};

struct DDSFileHeader {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize; // The number of bytes per scan line in an uncompressed texture; the total number of bytes in the top level texture for a
                                // compressed texture. The pitch must be DWORD aligned.
    uint32_t      depth;
    uint32_t      mipCount;
    uint32_t      reserved[11];
    DDPixelFormat ddpf;
    uint32_t      caps;
    uint32_t      caps2;
    uint32_t      caps3;
    uint32_t      caps4;
    uint32_t      reserved2;
};
static_assert(sizeof(DDSFileHeader) == 128);

struct DDSHeaderDX10 {
    enum Dimension : uint32_t {
        UNKNOWN   = 0,
        BUFFER    = 1,
        TEXTURE1D = 2,
        TEXTURE2D = 3,
        TEXTURE3D = 4,
    };
    uint32_t  format; // DXGI_FORMAT
    Dimension dimension;
    uint32_t  miscFlag; // D3D10_RESOURCE_MISC_FLAG
    uint32_t  arraySize;
    uint32_t  miscFlags2;
};

///
/// \note this struct should be synchronized with color format definition
///
static struct DdpfDesc {
    PixelFormat   format;
    DDPixelFormat ddpf;
} const s_ddpfDescTable[] = {
    {PixelFormat::BGR_8_8_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 24, 0xff0000, 0x00ff00, 0x0000ff, 0}},
    {PixelFormat::BGRA_8_8_8_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000}},
    {PixelFormat::BGRX_8_8_8_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0}},
    {PixelFormat::BGR_5_6_5_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0}},
    {PixelFormat::BGRX_5_5_5_1_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0}},
    {PixelFormat::BGRA_5_5_5_1_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000}},
    {PixelFormat::BGRA_4_4_4_4_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000}},
    //{ PixelFormat::BGR_2_3_3(),                     { DDS_DDPF_SIZE, DDS_DDPF_RGB,                                     0,  8,       0xe0,       0x1c,
    // 0x03,          0 } },
    {PixelFormat::A_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_ALPHA, 0, 8, 0, 0, 0, 0xff}},
    //{ PixelFormat::BGRA_2_3_3_8(),                  { DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS,              0, 16,     0x00e0,     0x001c,
    // 0x0003,     0xff00 } },
    {PixelFormat::BGRX_4_4_4_4_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 16, 0x0f00, 0x00f0, 0x000f, 0}},
    {PixelFormat::BGRA_10_10_10_2_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000}},
    {PixelFormat::RGBA_8_8_8_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000}},
    {PixelFormat::RGBX_8_8_8_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0}},
    {PixelFormat::RG_16_16_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB, 0, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0}},
    {PixelFormat::RGBA_10_10_10_2_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_RGB | DDS_DDPF_ALPHAPIXELS, 0, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000}},
    //{ PixelFormat::A_8P8_UNORM(),                    { DDS_DDPF_SIZE, DDS_DDPF_PALETTEINDEXED8 | DDS_DDPF_ALPHAPIXELS,  0, 16,          0,          0,
    // 0,     0xff00 } }, { PixelFormat::P8_UNORM(),                      { DDS_DDPF_SIZE, DDS_DDPF_PALETTEINDEXED8,                         0,  8, 0, 0,
    // 0,          0 } },
    {PixelFormat::L_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_LUMINANCE, 0, 8, 0xff, 0, 0, 0}},
    {PixelFormat::LA_8_8_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_LUMINANCE | DDS_DDPF_ALPHAPIXELS, 0, 16, 0x00ff, 0, 0, 0xff00}},
    //{ PixelFormat::LA_4_4_UNORM(),                  { DDS_DDPF_SIZE, DDS_DDPF_LUMINANCE | DDS_DDPF_ALPHAPIXELS,        0,  8,       0x0f,          0,
    // 0,       0xf0 } }, { PixelFormat::L_16_UNORM(),                    { DDS_DDPF_SIZE, DDS_DDPF_LUMINANCE,                               0, 16,
    // 0xffff,          0,          0,          0 } },
    {PixelFormat::RG_8_8_SNORM(), {DDS_DDPF_SIZE, DDS_DDPF_BUMPDUDV, 0, 16, 0x00ff, 0xff00, 0x0000, 0x0000}},
    //{ PixelFormat::UVL_5_5_6(),                     { DDS_DDPF_SIZE, DDS_DDPF_BUMPLUMINANCE,                           0, 16,     0x001f,     0x03e0,
    // 0xfc00,          0 } }, { PixelFormat::UVLX_8_8_8_8(),                  { DDS_DDPF_SIZE, DDS_DDPF_BUMPLUMINANCE,                           0, 32,
    // 0x000000ff, 0x0000ff00, 0x00ff0000,          0 } },
    {PixelFormat::RGBA_8_8_8_8_SNORM(), {DDS_DDPF_SIZE, DDS_DDPF_BUMPDUDV, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000}},
    {PixelFormat::RG_16_16_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_BUMPDUDV, 0, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000}},
    //{ PixelFormat::UVWA_10_10_10_2(),               { DDS_DDPF_SIZE, DDS_DDPF_BUMPDUDV | DDS_DDPF_ALPHAPIXELS,         0, 32, 0x3ff00000, 0x000ffc00,
    // 0x000003ff, 0xc0000000 } },
    {PixelFormat::R_16_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_ZBUFFER, 0, 16, 0, 0xffff, 0, 0}},
    //{ PixelFormat::UYVY(),                          { DDS_DDPF_SIZE, DDS_DDPF_FOURCC,                    DDS_FOURCC_UYVY,  0,          0,          0,
    // 0,          0 } }, { PixelFormat::RG_8_8_BG_8_8(),                     { DDS_DDPF_SIZE, DDS_DDPF_FOURCC,               DDS_FOURCC_RG_8_8_BG_8_8,  0, 0,
    // 0, 0,          0 } }, { PixelFormat::YUY2(),                          { DDS_DDPF_SIZE, DDS_DDPF_FOURCC,                    DDS_FOURCC_YUY2,  0, 0, 0, 0,
    // 0 } }, { PixelFormat::GR_8_8_GB_8_8(),                     { DDS_DDPF_SIZE, DDS_DDPF_FOURCC,               DDS_FOURCC_GR_8_8_GB_8_8,  0, 0, 0, 0, 0 } },
    {PixelFormat::DXT1_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_DXT1, 0, 0, 0, 0, 0}},
    {PixelFormat::DXT3_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_DXT2, 0, 0, 0, 0, 0}},
    {PixelFormat::DXT3_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_DXT3, 0, 0, 0, 0, 0}},
    {PixelFormat::DXT5_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_DXT4, 0, 0, 0, 0, 0}},
    {PixelFormat::DXT5_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_DXT5, 0, 0, 0, 0, 0}},
    // {PixelFormat::R_32_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, D3DFMT_D32F_LOCKABLE, 0, 0, 0, 0, 0}},
    {PixelFormat::RGBA_16_16_16_16_UNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_ABGR_16_16_16_16, 0, 0, 0, 0, 0}},
    {PixelFormat::RGBA_16_16_16_16_SNORM(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_Q16W16V16U16, 0, 0, 0, 0, 0}},
    {PixelFormat::R_16_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_R16F, 0, 0, 0, 0, 0}},
    {PixelFormat::RG_16_16_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_GR_16_16F, 0, 0, 0, 0, 0}},
    {PixelFormat::RGBA_16_16_16_16_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_ABGR_16_16_16_16F, 0, 0, 0, 0, 0}},
    {PixelFormat::R_32_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_R32F, 0, 0, 0, 0, 0}},
    {PixelFormat::RG_32_32_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_GR_32_32F, 0, 0, 0, 0, 0}},
    {PixelFormat::RGBA_32_32_32_32_FLOAT(), {DDS_DDPF_SIZE, DDS_DDPF_FOURCC, DDS_FOURCC_ABGR_32_32_32_32F, 0, 0, 0, 0, 0}},
    //{ PixelFormat::CxV8U8(),                        { DDS_DDPF_SIZE, DDS_DDPF_FOURCC,                  DDS_FOURCC_CxV8U8,  0,          0,          0,
    // 0,          0 } },
};

// -----------------------------------------------------------------------------
//
static PixelFormat getPixelFormatFromDDPF(const DDPixelFormat & ddpf) {
    uint32_t flags = ddpf.flags;
    if (flags & DDS_DDPF_FOURCC) flags = DDS_DDPF_FOURCC;

    bool fourcc = !!(flags & DDS_DDPF_FOURCC);
    bool bits   = !!(flags & (DDS_DDPF_ALPHA | DDS_DDPF_PALETTEINDEXED8 | DDS_DDPF_RGB | DDS_DDPF_ZBUFFER | DDS_DDPF_STENCILBUFFER | DDS_DDPF_BUMPLUMINANCE |
                            DDS_DDPF_BUMPDUDV));
    bool r      = !!(flags & (DDS_DDPF_RGB | DDS_DDPF_STENCILBUFFER | DDS_DDPF_LUMINANCE | DDS_DDPF_BUMPLUMINANCE | DDS_DDPF_BUMPDUDV));
    bool g      = !!(flags & (DDS_DDPF_RGB | DDS_DDPF_ZBUFFER | DDS_DDPF_STENCILBUFFER | DDS_DDPF_BUMPLUMINANCE | DDS_DDPF_BUMPDUDV));
    bool b      = !!(flags & (DDS_DDPF_RGB | DDS_DDPF_STENCILBUFFER | DDS_DDPF_BUMPLUMINANCE | DDS_DDPF_BUMPDUDV));
    bool a      = !!(flags & (DDS_DDPF_ALPHAPIXELS | DDS_DDPF_ALPHA | DDS_DDPF_BUMPDUDV));

    for (size_t i = 0; i < sizeof(s_ddpfDescTable) / sizeof(s_ddpfDescTable[0]); ++i) {
        const DdpfDesc & desc = s_ddpfDescTable[i];
        if (DDS_DDPF_SIZE != ddpf.size) continue;
        if (flags != desc.ddpf.flags) continue;
        if (fourcc && ddpf.fourcc != desc.ddpf.fourcc) continue;
        if (bits && ddpf.bits != desc.ddpf.bits) continue;
        if (r && ddpf.rMask != desc.ddpf.rMask) continue;
        if (g && ddpf.gMask != desc.ddpf.gMask) continue;
        if (b && ddpf.bMask != desc.ddpf.bMask) continue;
        if (a && ddpf.aMask != desc.ddpf.aMask) continue;

        // found!
        return desc.format;
    }

    // failed
    RAPID_IMAGE_LOGE("unknown DDS pixel format!");
    return PixelFormat::UNKNOWN();
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc::AlignedUniquePtr ImageDesc::loadFromDDS(std::istream & stream) {
    // read file header
    DDSFileHeader header;
    if (!checkedRead(stream, "read DDS header", &header, sizeof(header))) return {};
    constexpr uint32_t required_flags = DDS_DDSD_WIDTH | DDS_DDSD_HEIGHT;
    if (required_flags != (required_flags & header.flags)) {
        RAPID_IMAGE_LOGE("damage DDS header!");
        return {};
    }
    if (DDS_DDPF_PALETTEINDEXED8 & header.ddpf.flags) {
        RAPID_IMAGE_LOGE("do not support palette format!");
        return {};
    }

    // get image format
    PixelFormat format;
    if (MAKE_FOURCC('D', 'X', '1', '0') == header.ddpf.fourcc) {
        // read DX10 info
        DDSHeaderDX10 dx10;
        if (!checkedRead(stream, "read DX10 info", &dx10, sizeof(dx10))) return {};
        format = PixelFormat::makeFromDXGIFormat(dx10.format);
        if (!format.valid()) return {};
    } else {
        format = getPixelFormatFromDDPF(header.ddpf);
        if (!format.valid()) return {};
    }

    // BGRX_8888 format is not compatible with D3D10/D3D11 hardware. So we need to convert it to RGB format.
    bool bgr2rgb = false;
    if (PixelFormat::LAYOUT_8_8_8_8 == format.layout && PixelFormat::SWIZZLE_Z == format.swizzle0 && PixelFormat::SWIZZLE_Y == format.swizzle1 &&
        PixelFormat::SWIZZLE_X == format.swizzle2) {
        format.swizzle0 = PixelFormat::SWIZZLE_X;
        format.swizzle1 = PixelFormat::SWIZZLE_Y;
        format.swizzle2 = PixelFormat::SWIZZLE_Z;
        bgr2rgb         = true;
    }

    // check image dimension (TODO: array texture)
    uint32_t faces = 0;
    if (DDS_DDSD_DEPTH & header.flags && DDS_CAPS_COMPLEX & header.caps && DDS_CAPS2_VOLUME & header.caps2) {
        faces = 1; // volume texture
    } else if (DDS_CAPS_COMPLEX & header.caps && DDS_CAPS2_CUBEMAP & header.caps2 &&
               DDS_CAPS2_CUBEMAP_ALLFACES == (header.caps2 & DDS_CAPS2_CUBEMAP_ALLFACES)) {
        faces = 6; // cubemap
    } else if (0 == (DDS_CAPS2_CUBEMAP & header.caps2) && 0 == (DDS_CAPS2_VOLUME & header.caps2)) {
        faces = 1; // 2D texture
    } else {
        RAPID_IMAGE_LOGE("Fail to detect image face count!");
        return {};
    }
    uint32_t width  = header.width;
    uint32_t height = header.height;
    uint32_t depth  = DDS_DDSD_DEPTH & header.flags ? header.depth : 1;

    // check miplevel information
    bool     hasMipmap = (DDS_DDSD_MIPMAPCOUNT & header.flags) && (DDS_CAPS_MIPMAP & header.caps) && (DDS_CAPS_COMPLEX & header.caps);
    uint32_t levels    = hasMipmap ? header.mipCount : 1;
    if (0 == levels) levels = 1;

    // Now we have everything we need to create the image descriptor. Note that DDS image's pixel data is always aligned to 4 bytes.
    auto desc = ImageDesc::make(PlaneDesc::make(format, {width, height, depth}), faces, levels, ril::ImageDesc::FACE_MAJOR, 4);
    RII_ASSERT(desc.valid());

    // Read pixel data
    auto pixels = AlignedUniquePtr((uint8_t *) aalloc(desc.alignment, desc.size));
    if (!checkedRead(stream, "read pixels", pixels.get(), desc.size)) return {};

    // bgr -> rgb
    if (bgr2rgb) {
        auto   p          = pixels.get();
        size_t pixelCount = desc.size / 4;
        for (uint32_t i = 0; i < pixelCount; ++i, p += 4) { std::swap(p[0], p[2]); }
    }

    // done
    *this = std::move(desc);
    return pixels;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ImageDesc::saveToDDS(std::ostream &, const void *) const {
    //
    RII_REQUIRE(false, "not implemented yet.");
}

// *********************************************************************************************************************
// ImageDesc
// *********************************************************************************************************************

static inline bool isPowerOf2(uint32_t value) { return 0 == (value & (value - 1)); }

static inline uint32_t nextMultiple(uint32_t value, uint32_t multiple) {
    RII_ASSERT(isPowerOf2(multiple));
    return (value + multiple - 1) & ~(multiple - 1);
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc::ImageDesc(ImageDesc && rhs) {
    planes    = std::move(rhs.planes);
    layers    = rhs.layers;
    levels    = rhs.levels;
    size      = rhs.size;
    alignment = rhs.alignment;
    rhs.clear();
    RII_ASSERT(rhs.empty());
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::operator=(ImageDesc && rhs) {
    if (this != &rhs) {
        planes    = std::move(rhs.planes);
        layers    = rhs.layers;
        levels    = rhs.levels;
        size      = rhs.size;
        alignment = rhs.alignment;
        rhs.clear();
        RII_ASSERT(rhs.empty());
    }
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::clear() {
    planes.clear();
    layers    = 0;
    levels    = 0;
    size      = 0;
    alignment = DEFAULT_ALIGNMENT;
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
bool ImageDesc::valid() const {
    if (planes.size() == 0) {
        // supposed to be an empty descriptor
        if (0 != levels || 0 != layers || 0 != size) {
            RAPID_IMAGE_LOGE("empty descriptor should have zero on all members variables.");
            return false;
        } else {
            return true;
        }
    }

    // alignment should be positive and should be power of 2.
    if (0 == alignment) {
        RAPID_IMAGE_LOGE("pixel data alignment must be positive.");
        return false;
    }
    if (!isPowerOf2(alignment)) {
        RAPID_IMAGE_LOGE("pixel data alignment must be power of 2.");
        return false;
    }

    // make sure plane size is correct.
    if (layers * levels != planes.size()) {
        RAPID_IMAGE_LOGE("image plane array size must be equal to (layers * levels)");
        return false;
    }

    // check mipmaps
    for (uint32_t f = 0; f < layers; ++f) {
        for (uint32_t l = 0; l < levels; ++l) {
            auto & m = plane(f, l);
            if (!m.valid()) {
                RAPID_IMAGE_LOGE("image plane [%zu] is invalid", index(f, l));
                return false;
            }
            if ((m.offset + m.size) > size) {
                RAPID_IMAGE_LOGE("image plane [%zu]'s (offset + size) is out of range.", index(f, l));
                return false;
            }
        }
    }

    // success
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::reset(const PlaneDesc & baseMap, size_t layers_, size_t levels_, ConstructionOrder order, size_t planeOffsetAlignment) {
    // make sure the alignment is power of 2
    RII_REQUIRE(planeOffsetAlignment > 0 && isPowerOf2((uint32_t) planeOffsetAlignment));

    // clear old data
    clear();
    RII_ASSERT(valid());

    if (!baseMap.valid()) {
        RAPID_IMAGE_LOGE("ImageDesc reset failed: invalid base plane descriptor.");
        return *this;
    }

    if (0 == layers_) layers_ = 1;

    // calculate number of mipmap levels.
    {
        size_t maxLevels = 1;
        size_t w         = baseMap.extent.w;
        size_t h         = baseMap.extent.h;
        size_t d         = baseMap.extent.d;
        while (w > 1 || h > 1 || d > 1) {
            if (w > 1) w >>= 1;
            if (h > 1) h >>= 1;
            if (d > 1) d >>= 1;
            ++maxLevels;
        }
        if (0 == levels_ || levels_ > maxLevels) levels_ = maxLevels;
    }

    // build full mipmap chain
    layers = (uint32_t) layers_;
    levels = (uint32_t) levels_;
    planes.resize(layers_ * levels_);
    uint32_t offset = 0;
    if (MIP_MAJOR == order) {
        // In this mode, the outer loop is mipmap level, the inner loop is layers/faces
        PlaneDesc mip = baseMap;
        for (uint32_t m = 0; m < levels_; ++m) {
            for (size_t i = 0; i < layers_; ++i) {
                mip.offset          = offset;
                planes[index(i, m)] = mip;
                offset              = nextMultiple(mip.size + mip.offset, (uint32_t) planeOffsetAlignment);
                RII_ASSERT(0 == (offset % planeOffsetAlignment));
            }
            // next level
            if (mip.extent.w > 1) mip.extent.w >>= 1;
            if (mip.extent.h > 1) mip.extent.h >>= 1;
            if (mip.extent.d > 1) mip.extent.d >>= 1;
            mip = PlaneDesc::make(mip.format, mip.extent, mip.step, 0, 0);
        }
    } else {
        // In this mode, the outer loop is faces/layers, the inner loop is mipmap levels
        for (size_t f = 0; f < layers_; ++f) {
            PlaneDesc mip = baseMap;
            for (size_t m = 0; m < levels_; ++m) {
                mip.offset          = offset;
                planes[index(f, m)] = mip;
                offset              = nextMultiple(offset + mip.size, (uint32_t) planeOffsetAlignment);
                // next level
                if (mip.extent.w > 1) mip.extent.w >>= 1;
                if (mip.extent.h > 1) mip.extent.h >>= 1;
                if (mip.extent.d > 1) mip.extent.d >>= 1;
                mip = PlaneDesc::make(mip.format, mip.extent, mip.step, 0, 0);
            }
        }
    }

    size      = offset;
    alignment = (uint32_t) planeOffsetAlignment;

    // done
    RII_ASSERT(valid());
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::set2D(PixelFormat format, size_t width, size_t height, size_t levels_, ConstructionOrder order, size_t planeOffsetAlignment) {
    auto baseMap = PlaneDesc::make(format, {(uint32_t) width, (uint32_t) height, 1});
    return reset(baseMap, 1, levels_, order, planeOffsetAlignment);
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::setCube(PixelFormat format, size_t width, size_t levels_, ConstructionOrder order, size_t planeOffsetAlignment) {
    auto baseMap = PlaneDesc::make(format, {(uint32_t) width, (uint32_t) width, 1});
    return reset(baseMap, 6, levels_, order, planeOffsetAlignment);
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc::AlignedUniquePtr ImageDesc::load(std::istream & stream) {
    if (!stream) {
        RAPID_IMAGE_LOGE("failed to write image to stream: stream is not good.");
        return {};
    }

    // store current stream position
    auto begin = stream.tellg();

    // check if the file is RIL or not.
    RILFileTag rilTag;
    if (checkedRead(stream, "read RIL image tag", &rilTag, sizeof(rilTag)) && rilTag.valid()) {
        stream.seekg(begin, std::ios::beg);
        return loadFromRIL(stream);
    }

    // try read as DDS
    uint32_t ddsTag = 0;
    stream.seekg(begin, std::ios::beg);
    if (checkedRead(stream, "read DDS image tag", &ddsTag, sizeof(ddsTag)) && 0x20534444 == ddsTag) {
        stream.seekg(begin, std::ios::beg);
        return loadFromDDS(stream);
    }

    // done
    RAPID_IMAGE_LOGE("failed to read image from stream: unsupported/unrecognized file format.");
    return {};
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc::AlignedUniquePtr ImageDesc::load(const void * data_, size_t size_) {
    if (!data_ || !size_) {
        RAPID_IMAGE_LOGW("load image from null or zero size data returns empty image.");
        return {};
    }
    auto str = std::string((const char *) data_, size_);
    auto iss = std::istringstream(str);
    return load(iss);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ImageDesc::save(const std::string & filename, const void * pixels) const {
    // lambda to open file stream
    auto openFileStream = [](const std::string & filename) -> std::ofstream {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            RAPID_IMAGE_LOGE("failed to open file %s for writing.", filename.c_str());
            return {};
        }
        return file;
    };

    // determine file format from file extension
    auto ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return (char) tolower(c); });
    if (".ril" == ext) {
        auto file = openFileStream(filename);
        if (!file) return;
        saveToRIL(file, pixels);
    } else if (".dds" == ext) {
        auto file = openFileStream(filename);
        if (!file) return;
        saveToDDS(file, pixels);
    } else {
        RAPID_IMAGE_LOGE("Unsupported file extension: %s", ext.c_str());
    }
}

// *********************************************************************************************************************
// Image
// *********************************************************************************************************************

// ---------------------------------------------------------------------------------------------------------------------
//
Image::Image(ImageDesc && desc, const void * initialContent, size_t initialContentSizeInbytes) {
    _proxy.desc = std::move(desc);
    construct(initialContent, initialContentSizeInbytes);
}

// ---------------------------------------------------------------------------------------------------------------------
//
Image::Image(const ImageDesc & desc, const void * initialContent, size_t initialContentSizeInbytes) {
    _proxy.desc = std::move(desc);
    construct(initialContent, initialContentSizeInbytes);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Image::clear() {
    afree(_proxy.data);
    _proxy.data = nullptr;
    _proxy.desc = {};
    RII_ASSERT(empty());
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Image::construct(const void * initialContent, size_t initialContentSizeInbytes) {
    // clear old image data.
    afree(_proxy.data);
    _proxy.data = nullptr;

    // deal with empty image
    if (_proxy.desc.empty()) {
        // Usually constructing an empty image is a valid behavior. So we don't print any error/warning messages.
        // But if we given an empty descriptor but non empty initial content, then it might imply that something
        // went wrong.
        if (initialContent && initialContentSizeInbytes) { RAPID_IMAGE_LOGW("constructing an empty image with non-empty content array"); }
        return;
    }

    // (re)allocate pixel buffer
    size_t imageSize = size();
    _proxy.data      = (uint8_t *) aalloc(_proxy.desc.alignment, imageSize);
    if (!_proxy.data) {
        RAPID_IMAGE_LOGE("failed to construct image: out of memory.");
        return;
    }

    // store the initial content
    if (initialContent) {
        if (0 == initialContentSizeInbytes) {
            initialContentSizeInbytes = imageSize;
        } else if (initialContentSizeInbytes != imageSize) {
            RAPID_IMAGE_LOGW("incoming pixel buffer size does not equal to calculated image size.");
        }
        memcpy(_proxy.data, initialContent, std::min(imageSize, initialContentSizeInbytes));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
Image Image::load(std::istream & fp) {
    Image r;
    auto  pixels = r._proxy.desc.load(fp);
    if (!pixels) return {};
    r._proxy.data = pixels.release();
    return r;
}

// ---------------------------------------------------------------------------------------------------------------------
//
Image Image::load(const void * data, size_t size) {
    Image r;
    auto  pixels = r._proxy.desc.load(data, size);
    if (!pixels) return {};
    r._proxy.data = pixels.release();
    return r;
}

// ---------------------------------------------------------------------------------------------------------------------
//
Image Image::load(const std::string & filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) {
        RAPID_IMAGE_LOGE("Failed to open image file %s : errno=%d", filename.c_str(), errno);
        return {};
    }
    return load(f);
}

} // namespace RAPID_IMAGE_NAMESPACE