#define _CRT_SECURE_NO_WARNINGS
#include "../ri.h"
#include "../3rd-party/catch2/catch.hpp"
#include <cstdio>
#include <filesystem>
#include <memory>
#include <chrono>
#include <thread>

using namespace ph;
using namespace std::string_literals;

// ---------------------------------------------------------------------------------------------------------------------
// quick test of image loading from file.
TEST_CASE("dxt1", "[image]") {
    auto desc = ImageDesc(ImagePlaneDesc::make(ColorFormat::DXT1_UNORM(), 256, 256, 1), 6, 0);
    // REQUIRE(desc.plane(0, 0).alignment == 8);
    REQUIRE(desc.slice(0, 0) == 32768);
    REQUIRE(desc.slice(0, 1) == 8192);
    REQUIRE(desc.slice(0, 2) == 2048);
    REQUIRE(desc.slice(0, 3) == 512);
    REQUIRE(desc.slice(0, 4) == 128);
    REQUIRE(desc.slice(0, 5) == 32);
    REQUIRE(desc.slice(0, 6) == 8);
    REQUIRE(desc.slice(0, 7) == 8);
    REQUIRE(desc.slice(0, 8) == 8);
    REQUIRE(desc.size == 43704 * 6);
}

TEST_CASE("face-major", "[image]") {
    // in face major mode, pixels from same face are packed together.
    auto desc = ImageDesc(ImagePlaneDesc::make(ColorFormat::DXT1_UNORM(), 256, 256, 1), 6, 0, ImageDesc::FACE_MAJOR);
    // REQUIRE(desc.plane(0, 0).alignment == 8);
    REQUIRE(desc.slice(0, 0) == 32768);
    REQUIRE(desc.slice(0, 1) == 8192);
    REQUIRE(desc.slice(0, 2) == 2048);
    REQUIRE(desc.slice(0, 3) == 512);
    REQUIRE(desc.slice(0, 4) == 128);
    REQUIRE(desc.slice(0, 5) == 32);
    REQUIRE(desc.slice(0, 6) == 8);
    REQUIRE(desc.slice(0, 7) == 8);
    REQUIRE(desc.slice(0, 8) == 8);
    REQUIRE(desc.size == 43704 * 6);

    // check offsets
    REQUIRE(desc.pixel(0, 0) == 0);
    REQUIRE(desc.pixel(0, 1) == desc.pixel(0, 0) + desc.slice(0, 0));
    REQUIRE(desc.pixel(0, 2) == desc.pixel(0, 1) + desc.slice(0, 1));
    REQUIRE(desc.pixel(0, 3) == desc.pixel(0, 2) + desc.slice(0, 2));
    REQUIRE(desc.pixel(0, 4) == desc.pixel(0, 3) + desc.slice(0, 3));
    REQUIRE(desc.pixel(0, 5) == desc.pixel(0, 4) + desc.slice(0, 4));
    REQUIRE(desc.pixel(0, 6) == desc.pixel(0, 5) + desc.slice(0, 5));
    REQUIRE(desc.pixel(0, 7) == desc.pixel(0, 6) + desc.slice(0, 6));
    REQUIRE(desc.pixel(0, 8) == desc.pixel(0, 7) + desc.slice(0, 7));
}

TEST_CASE("load", "[image]") {
    auto path = std::filesystem::path(TEST_FOLDER) / "alien-planet.jpg";
    PH_LOGI("load image from file: %s", path.string().c_str());
    auto ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    REQUIRE(ri.width() == 600);
    REQUIRE(ri.height() == 486);
}

TEST_CASE("rgba32f", "[image]") {
    auto path = std::filesystem::path(TEST_FOLDER) / "rgba32f-64x64.dds";
    PH_LOGI("load image from file: %s", path.string().c_str());
    auto image = Image::load(path.string());
    REQUIRE(image.width() == 64);
    REQUIRE(image.height() == 64);
    auto pixels = image.desc().plane().toRGBA8(image.data());
    auto p0     = pixels[0];
    REQUIRE(p0.x == 166);
    REQUIRE(p0.y == 166);
    REQUIRE(p0.z == 166);
    REQUIRE(p0.w == 255);
}

TEST_CASE("save-to-png", "[image]") {
    auto path1 = (std::filesystem::path(TEST_FOLDER) / "alien-planet.jpg").string();
    PH_LOGI("load image from file: %s", path1.c_str());
    auto r1 = Image::load(path1);
    REQUIRE(r1.format() == ColorFormat::RGBA8());

    auto path2 = (std::filesystem::path(TEST_FOLDER) / "alien-planet-2.png").string();
    PH_LOGI("save image to file: %s", path2.c_str());
    r1.desc().plane(0).saveToPNG(path2, r1.data());
    auto r2 = Image::load(path2);
    REQUIRE(r2.format() == ColorFormat::RGBA8());

    // Delete the 2nd image.
    std::filesystem::remove(std::filesystem::path(TEST_FOLDER) / "alien-planet-2.png");

    // r1 and r2 should be completely identical
    REQUIRE(r1.size() == r2.size());
    REQUIRE(0 == memcmp(r1.data(), r2.data(), r1.size()));
}

TEST_CASE("astc-texture-handling", "[image]") {
    auto path = std::filesystem::path(TEST_FOLDER) / "alien-planet-4x4.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    auto ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_4x4_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-2-4x4.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_4x4_UNORM());
    CHECK(ri.width() == 537);
    CHECK(ri.height() == 393);
    CHECK(ri.pitch() == 135 * 16);
    CHECK(ri.size() == 99 * 135 * 16);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-5x4.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_5x4_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-5x5.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_5x5_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-6x5.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_6x5_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);
    CHECK(ri.pitch() == 100 * 16);
    CHECK(ri.size() == 98 * 100 * 16);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-6x6.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_6x6_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-8x5.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_8x5_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-8x6.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_8x6_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-8x8.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_8x8_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-10x5.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_10x5_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-10x6.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_10x6_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-10x8.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_10x8_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-10x10.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_10x10_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-12x10.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_12x10_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);

    path = std::filesystem::path(TEST_FOLDER) / "alien-planet-12x12.astc";
    PH_LOGI("load image from file: %s", path.string().c_str());
    ri = Image::load(path.string());
    REQUIRE(!ri.empty());
    CHECK(ri.format() == ColorFormat::ASTC_12x12_UNORM());
    CHECK(ri.width() == 600);
    CHECK(ri.height() == 486);
}

TEST_CASE("plane-data-reconversion-from-float-functions", "[image]") {
    auto path = std::filesystem::path(TEST_FOLDER) / "alien-planet.jpg";
    auto ri   = Image::load(path.string());

    auto      plane = ri.desc().planes.front();
    uint8_t * dst   = new uint8_t[plane.size];
    plane.fromFloat4(dst, plane.size, 0, plane.toFloat4(ri.data()).data());

    for (uint32_t i = 0; i < plane.size; ++i) { REQUIRE(*(ri.data() + i) == *(dst + i)); }

    delete[] dst;
}

TEST_CASE("step-and-pitch-checks-on-mipmap-generation-routines", "[image]") {
    auto ri = Image(ImageDesc(ImagePlaneDesc::make(ColorFormat::RG_8_8_UNORM(), 2, 2, 1, 4, 16)));
    REQUIRE(ri.desc().levels == 1);

    ri.data()[0] = 1; // R00
    ri.data()[1] = 2; // G00
    ri.data()[4] = 3; // R10
    ri.data()[5] = 4; // G10

    ri.data()[6]  = 255; // Garbage data
    ri.data()[7]  = 254; // Garbage data
    ri.data()[8]  = 253; // Garbage data
    ri.data()[9]  = 252; // Garbage data
    ri.data()[10] = 251; // Garbage data
    ri.data()[11] = 250; // Garbage data
    ri.data()[12] = 249; // Garbage data
    ri.data()[13] = 248; // Garbage data
    ri.data()[14] = 247; // Garbage data
    ri.data()[15] = 246; // Garbage data

    ri.data()[16] = 5; // R01
    ri.data()[17] = 6; // G01
    ri.data()[20] = 7; // R11
    ri.data()[21] = 8; // G11

    ri.data()[22] = 255; // Garbage data
    ri.data()[23] = 254; // Garbage data
    ri.data()[24] = 253; // Garbage data
    ri.data()[25] = 252; // Garbage data
    ri.data()[26] = 251; // Garbage data
    ri.data()[27] = 250; // Garbage data
    ri.data()[28] = 249; // Garbage data
    ri.data()[29] = 248; // Garbage data
    ri.data()[30] = 247; // Garbage data
    ri.data()[31] = 246; // Garbage data

    auto ri2 = ri.desc().plane().generateMipmaps(ri.data());
    REQUIRE(ri2.desc().levels == 2);

    // Basemap plane description checks
    REQUIRE(ri2.width() == 2);
    REQUIRE(ri2.height() == 2);
    REQUIRE(ri2.depth() == 1);
    REQUIRE(ri2.step() == 2);
    REQUIRE(ri2.pitch() == 4);

    // Mip plane description checks
    REQUIRE(ri2.width(0, 1) == 1);
    REQUIRE(ri2.height(0, 1) == 1);
    REQUIRE(ri2.depth(0, 1) == 1);
    REQUIRE(ri2.step(0, 1) == 2);
    REQUIRE(ri2.pitch(0, 1) == 2);

    // Basemap data checks
    REQUIRE(ri2.data()[0] == 1);
    REQUIRE(ri2.data()[1] == 2);
    REQUIRE(ri2.data()[2] == 3);
    REQUIRE(ri2.data()[3] == 4);
    REQUIRE(ri2.data()[4] == 5);
    REQUIRE(ri2.data()[5] == 6);
    REQUIRE(ri2.data()[6] == 7);
    REQUIRE(ri2.data()[7] == 8);

    // Mip data checks
    REQUIRE(ri2.data()[8] == 4);
    REQUIRE(ri2.data()[9] == 5);
}

TEST_CASE("ktx2", "[image]") {
    auto path = std::filesystem::path(TEST_FOLDER) / "bumper-panorama.ktx2";
    PH_LOGI("load image from file: %s", path.string().c_str());
    auto ri = Image::load(path.string());

    REQUIRE(!ri.empty());
    REQUIRE(ri.data());

    CHECK(ri.width() == 4096);
    CHECK(ri.height() == 2048);
    CHECK(ri.depth() == 1);
    CHECK(ri.desc().levels == 13);
    CHECK(ri.desc().layers == 1);
    CHECK(ri.desc().plane().format.layoutDesc().blockWidth == 4);
    CHECK(ri.desc().plane().format.layoutDesc().blockHeight == 4);
    CHECK(ri.desc().plane().format.layoutDesc().blockBytes == 16); // 128 bits
}

// this is currently a win32 only test, since it replies on VirtualAlloc() and VirtualProtect() change page permissions.
#if PH_MSWIN
    #include <windows.h>
    #include <excpt.h>
TEST_CASE("convert-float4", "[image]") {
    // allocate 2 pages of memory (each page is 4K bytes)
    uint8_t * data = (uint8_t *) VirtualAlloc(0, 8 * 1024 * 1024, MEM_RESERVE, PAGE_READWRITE);

    // Commit only the first page
    VirtualAlloc((LPVOID) data, 4 * 1024, MEM_COMMIT, PAGE_READWRITE);

    // So visiting the 2nd page will cause an page fault exception.
    uint8_t * page2 = data + 4 * 1024;
    // uncomment this page to test the page fault error,
    // *page = 0;

    // Now let's create a 1 byte pixel that is right on the last byte of the first page. Accessing memory ofter
    // this pixel will cause page fault exception.
    uint8_t * pixel = page2 - 1;
    *pixel          = 127;

    // Conver this pixel from u8 TO float. The conversion function should not access any bytes on page2
    // If it does, it'll trigger page fault exception and cause the whole test to fail.

    float f1 = ColorFormat::R_8_UNORM().getPixelChannelFloat(pixel, 0);
    REQUIRE(f1 == 127.0f / 255.0f);

    uint8_t u8 = ColorFormat::R_8_UNORM().getPixelChannelByte(pixel, 0);
    REQUIRE(u8 == 127);

    auto plane = ImagePlaneDesc::make(ColorFormat::R_8_UNORM(), 1, 1);
    auto f4    = plane.toFloat4(pixel);
    REQUIRE(f4[0].x == 127.0f / 255.0f);

    // done
    VirtualFree(data, 8 * 1024, MEM_RELEASE);
}
#endif
