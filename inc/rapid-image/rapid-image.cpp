#undef RAPID_IMAGE_IMPLEMENTATION
#include "rapid-image.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include <numeric>
#include <cstring>

namespace RAPID_IMAGE_NAMESPACE {

#ifdef __clang__
// W/o this line, clang will complain undefined symbol.
constexpr PixelFormat::LayoutDesc PixelFormat::LAYOUTS[];
#endif

// ---------------------------------------------------------------------------------------------------------------------
//
static void * aalloc(size_t a, size_t s) {
#if _WIN32
    return _aligned_malloc(s, a);
#else
    return aligned_alloc(a, s);
#endif
}

// ---------------------------------------------------------------------------------------------------------------------
//
static void afree(void * p) {
#if _WIN32
    _aligned_free(p);
#else
    ::free(p);
#endif
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
    if (0 == width || 0 == height || 0 == depth) {
        RAPID_IMAGE_LOGE("dimension can't zero!");
        return false;
    }

    // check pitches
    auto & cld             = format.layoutDesc();
    auto   numBlocksPerRow = (uint32_t) ((width + cld.blockWidth - 1) / cld.blockWidth);
    auto   numBlocksPerCol = (uint32_t) ((height + cld.blockHeight - 1) / cld.blockHeight);
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
    if (size < slice * depth) {
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
PlaneDesc PlaneDesc::make(PixelFormat format, size_t width, size_t height, size_t depth, size_t step, size_t pitch, size_t slice) {
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
    p.format = format;
    p.width  = (uint32_t) (width ? width : 1);
    p.height = (uint32_t) (height ? height : 1);
    p.depth  = (uint32_t) (depth ? depth : 1);
    // Check for ASTC format enumerations
    p.step = std::max((uint32_t) step, (uint32_t) (fd.blockBytes));
    // p.alignment = (uint32_t)alignment;

    // calculate row pitch, aligned to rowAlignment.
    // Note that we can't just use nextMultiple here, since rowAlignment might not be power of 2.
    auto numBlocksPerRow = (uint32_t) ((width + fd.blockWidth - 1) / fd.blockWidth);
    p.pitch              = std::max(p.step * numBlocksPerRow, (uint32_t) pitch);
    auto numBlocksPerCol = (uint32_t) ((height + fd.blockHeight - 1) / fd.blockHeight);
    p.slice              = std::max(p.pitch * numBlocksPerCol, (uint32_t) slice);
    RII_ASSERT(p.pitch > 0 && p.slice > 0);

    // calculate plane size of the image.
    p.size = p.slice * p.depth;

    // done
    RII_ASSERT(p.valid());
    return p;
}

// ---------------------------------------------------------------------------------------------------------------------

union uint128_t {
    struct {
        uint64_t lo;
        uint64_t hi;
    };
    uint8_t u8[16];

    static inline uint128_t make(const void * data, size_t size) {
        RII_ASSERT(size <= 16);
        uint128_t r   = {};
        auto      src = (const uint8_t *) data;
        auto      end = src + size;
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
/// Convert a float to one color channel, based on the INITIAL channel format/sign prior to the reconversion
static inline uint32_t fromFloat(float value, uint32_t width, PixelFormat::Sign sign) {
    auto castFromFloat = [](float fp) {
        union {
            ;
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
/// Convert back to pixels of an arbitrary format from float4. Do not support compressed format.
static inline uint128_t convertFromFloat4(const PixelFormat::LayoutDesc & ld, const PixelFormat & format, const float4 & pixel) {
    RII_ASSERT(1 == ld.blockWidth && 1 == ld.blockHeight); // do not support compressed format.

    uint128_t result = {};

    auto convertToChannel = [&](uint32_t swizzle) {
        const auto & ch   = ld.channels[swizzle];
        auto         sign = (PixelFormat::Sign)((swizzle < 3) ? format.sign012 : format.sign3);
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

    convertToChannel(format.swizzle0);
    convertToChannel(format.swizzle1);
    convertToChannel(format.swizzle2);
    convertToChannel(format.swizzle3);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
/// Convert pixel of arbitrary format to float4. Do not support compressed format.
static inline float4 convertNonCompressedPixelToFloat4(const PixelFormat::LayoutDesc & ld, const PixelFormat & format, const void * pixel) {
    RII_ASSERT(1 == ld.blockWidth && 1 == ld.blockHeight); // do not support compressed format.

    auto src = uint128_t::make(pixel, ld.blockBytes);

    // labmda to convert one channel
    auto convertChannel = [&](uint32_t swizzle) {
        if (PixelFormat::SWIZZLE_0 == swizzle) return 0.f;
        if (PixelFormat::SWIZZLE_1 == swizzle) return 1.f;
        const auto & ch   = ld.channels[swizzle];
        auto         sign = (PixelFormat::Sign)((swizzle < 3) ? format.sign012 : format.sign3);
        return tofloat(src.segment(ch.shift, ch.bits), ch.bits, sign);
    };

    return float4::make(convertChannel(format.swizzle0), convertChannel(format.swizzle1), convertChannel(format.swizzle2), convertChannel(format.swizzle3));
}

// ---------------------------------------------------------------------------------------------------------------------
/// convert single pixel to RGBA8 format.
static void convertToRGBA8(RGBA8 * result, const PixelFormat::LayoutDesc & ld, const PixelFormat & format, const void * src) {
    if (PixelFormat::RGBA8() == format) {
        // shortcut for RGBA8 format.
        *result = *(const RGBA8 *) src;
    } else if (1 == ld.blockWidth && 1 == ld.blockHeight) {
        // this is the general case that could in theory handle any format.
        auto f4 = convertNonCompressedPixelToFloat4(ld, format, src);
        *result =
            RGBA8::make((uint8_t) std::clamp<uint32_t>((uint32_t) (f4.x * 255.0f), 0, 255), (uint8_t) std::clamp<uint32_t>((uint32_t) (f4.y * 255.0f), 0, 255),
                        (uint8_t) std::clamp<uint32_t>((uint32_t) (f4.z * 255.0f), 0, 255), (uint8_t) std::clamp<uint32_t>((uint32_t) (f4.w * 255.0f), 0, 255));
    } else if (PixelFormat::FIRST_ASTC_LAYOUT <= format.layout && format.layout <= PixelFormat::LAST_ASTC_LAYOUT) {
        // decompress ASTC block
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

struct StbiStream {
    std::ostream & stream;

    StbiStream(std::ostream & s): stream(s) {}

    ~StbiStream() { stream.flush(); }

    static void write(void * context, void * data, int size) {
        auto p = (StbiStream *) context;
        p->stream.write((const char *) data, size);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
//
void PlaneDesc::saveToRAW(std::ostream & stream, const void * pixels) const {
    constexpr size_t PLANE_SIZE = sizeof(*this);
    static_assert(PLANE_SIZE <= 256, "we currently only reserved 256 bytes for plane descriptor.");
    stream.write((const char *) this, PLANE_SIZE);
    for (size_t i = 0; i < 256 - PLANE_SIZE; ++i) stream << ' '; // fill the header up to 256 bytes.
    stream.write((const char *) pixels, this->size);
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
    colors.reserve(width * height);
    for (uint32_t z = 0; z < depth; ++z) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) { colors.push_back(convertNonCompressedPixelToFloat4(ld, format, p + pixel(x, y, z))); }
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

    colors.resize(width * height * depth);
    for (uint32_t z = 0; z < depth; ++z) {
        for (uint32_t y = 0; y < height; y += ld.blockHeight) {
            for (uint32_t x = 0; x < width; x += ld.blockWidth) {
                // convert the block
                convertToRGBA8(block.data(), ld, format, p + pixel(x, y, z));

                // copy the block to output buffer
                uint32_t right  = std::min(x + ld.blockWidth, width);
                uint32_t bottom = std::min(y + ld.blockHeight, height);
                for (uint32_t yy = y; yy < bottom; ++yy) {
                    for (uint32_t xx = x; xx < right; ++xx) { colors[z * width * height + yy * width + xx] = block[(yy - y) * ld.blockWidth + (xx - x)]; }
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
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t dstOffset = pixel(x, y, dstZ);
            if ((dstOffset + ld.blockBytes) > dstSize) {
                RAPID_IMAGE_LOGE("Destination buffer size (%zu) is not large enough.", dstSize);
                return;
            }
            uint8_t * d    = (uint8_t *) dst + dstOffset;
            uint128_t temp = convertFromFloat4(ld, format, *(p + y * width + x));
            memcpy(d, &temp, ld.blockBytes); // TODO: Can we avoid this memcpy's?
        }
    }
}

// // ---------------------------------------------------------------------------------------------------------------------
// //
// Image PlaneDesc::generateMipmaps(const void * pixels) const {
//     RII_REQUIRE(depth == 1);

//     Image  ri           = Image(ImageDesc(PlaneDesc::make(PixelFormat::FLOAT4(), width, height), 1, 0));
//     auto      baseMap      = toFloat4(pixels);
//     uint8_t * baseMapAlloc = ri.data();
//     memcpy(baseMapAlloc, baseMap.data(), baseMap.size() * sizeof(float4));
//     Image result = Image(ImageDesc(PlaneDesc::make(format, width, height), 1, 0));

//     result.desc().plane().fromFloat4(result.data(), result.size(), 0, baseMapAlloc);

//     for (size_t i = 1; i < ri.desc().levels; ++i) {
//         uint8_t * mipAlloc = ri.data() + ri.desc().pixel(0, i);
//         MipmapGenerator::generateMipmap((float4 *) mipAlloc, (const float4 *) (ri.data() + ri.desc().pixel(0, i - 1)), ri.desc(0, i - 1).width,
//                                         ri.desc(0, i - 1).height, ri.desc(0, i).width, ri.desc(0, i).height);
//         result.desc().plane(0, i).fromFloat4(result.data(), result.size(), 0, mipAlloc);
//     }

//     return result;
// }

// *********************************************************************************************************************
// ImageDesc
// *********************************************************************************************************************

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

    if (layers * levels != planes.size()) {
        RAPID_IMAGE_LOGE("image plane array size must be equal to (layers * levels)");
        return false;
    }

    // check mipmaps
    for (uint32_t f = 0; f < layers; ++f)
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

    // success
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::reset(const PlaneDesc & baseMap, size_t layers_, size_t levels_, ConstructionOrder order) {
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
        size_t w         = baseMap.width;
        size_t h         = baseMap.height;
        size_t d         = baseMap.depth;
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
                offset += mip.size;
            }
            // next level
            if (mip.width > 1) mip.width >>= 1;
            if (mip.height > 1) mip.height >>= 1;
            if (mip.depth > 1) mip.depth >>= 1;
            mip = PlaneDesc::make(mip.format, mip.width, mip.height, mip.depth, mip.step, 0, 0);
        }
    } else {
        // In this mode, the outer loop is faces/layers, the inner loop is mipmap levels
        for (size_t f = 0; f < layers_; ++f) {
            PlaneDesc mip = baseMap;
            for (size_t m = 0; m < levels_; ++m) {
                mip.offset          = offset;
                planes[index(f, m)] = mip;
                offset += mip.size;
                // next level
                if (mip.width > 1) mip.width >>= 1;
                if (mip.height > 1) mip.height >>= 1;
                if (mip.depth > 1) mip.depth >>= 1;
                mip = PlaneDesc::make(mip.format, mip.width, mip.height, mip.depth, mip.step, 0, 0);
            }
        }
    }

    size = offset;

    // done
    RII_ASSERT(valid());
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::set2D(PixelFormat format, size_t width, size_t height, size_t levels_, ConstructionOrder order) {
    auto baseMap = PlaneDesc::make(format, width, height);
    return reset(baseMap, 1, levels_, order);
}

// ---------------------------------------------------------------------------------------------------------------------
//
ImageDesc & ImageDesc::setCube(PixelFormat format, size_t width, size_t levels_, ConstructionOrder order) {
    auto baseMap = PlaneDesc::make(format, width, width);
    return reset(baseMap, 6, levels_, order);
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
    _proxy.data      = (uint8_t *) aalloc(4, imageSize); // TODO: larger alignment for larger pixel?
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

// // ---------------------------------------------------------------------------------------------------------------------
// //
// Image Image::load(std::istream & fp) {
//     // store current stream position
//     auto begin = fp.tellg();

//     // try read as ASTC
//     ASTCReader astc(fp);
//     if (!astc.empty()) { return astc.getRawImage(); }

//     fp.seekg(begin, std::ios::beg);

//     // setup stbi io callback
//     stbi_io_callbacks io = {};
//     io.read              = [](void * user, char * data, int size) -> int {
//         auto fp = (std::istream *) user;
//         fp->read(data, size);
//         return (int) fp->gcount();
//     };
//     io.skip = [](void * user, int n) {
//         auto fp = (std::istream *) user;
//         fp->seekg(n, std::ios::cur);
//     };
//     io.eof = [](void * user) -> int {
//         auto fp = (std::istream *) user;
//         return fp->eof();
//     };

//     // try read as DDS
//     DDSReader dds(fp);
//     if (dds.checkFormat()) {
//         auto image = Image(dds.readHeader());
//         if (!image.empty() && dds.readPixels(image.data(), image.size())) { return image; }
//     }

//     // try read as KTX2
//     KTX2Reader ktx2(fp);
//     auto       image = ktx2.readFile();
//     if (!image.empty() && image.data()) { return image; }

//     // Load from common image file via stb_image library
//     // TODO: hdr/grayscale support
//     int x, y, n;
//     fp.seekg(begin, std::ios::beg);
//     auto data = stbi_load_from_callbacks(&io, &fp, &x, &y, &n, 4);
//     if (data) {
//         auto image = Image(ImageDesc(PlaneDesc::make(PixelFormat::RGBA_8_8_8_8_UNORM(), (uint32_t) x, (uint32_t) y)), data);
//         RII_ASSERT(image.desc().valid());
//         stbi_image_free(data);
//         return image;
//     }

//     // RAPID_IMAGE_LOGE("Failed to load image from stream: unrecognized file format.");
//     return {};
// }

// // ---------------------------------------------------------------------------------------------------------------------
// //
// Image Image::load(const void * data, size_t size) {
//     auto str = std::string((const char *) data, size);
//     auto iss = std::istringstream(str);
//     return load(iss);
// }

// // ---------------------------------------------------------------------------------------------------------------------
// //
// Image Image::load(const std::string & filename) {
//     std::ifstream f(filename, std::ios::binary);
//     if (!f.good()) {
//         RAPID_IMAGE_LOGE("Failed to open image file %s : errno=%d", filename.c_str(), errno);
//         return {};
//     }
//     return load(f);
// }

} // namespace RAPID_IMAGE_NAMESPACE