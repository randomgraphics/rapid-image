// include stb image header to enable load/save from common image formats like png, jpg, bmp, tga, ...
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // sprintf() is deprecaited
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4244) // conversion from 'int' to 'char', possible loss of data
#endif
#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image_write.h"

#define RAPID_IMAGE_IMPLEMENTATION
#include "../../inc/rapid-image/rapid-image.h"

#include "../3rd-party/catch2/catch.hpp"

TEST_CASE("stb-save-load") {
    auto path = std::filesystem::path(TEST_SOURCE_DIR) / "alien-planet.jpg";
    RAPID_IMAGE_LOGI("load from file: %s", path.string().c_str());
    auto image1 = ril::Image::load(std::ifstream(path, std::ios::binary), path.string().c_str());
    REQUIRE(!image1.empty());

    // save to PNG
    std::stringstream ss;
    image1.save({ril::ImageDesc::PNG}, ss);

    // then load it back.
    auto str    = ss.str();
    auto image2 = ril::Image::load(str.data(), str.size());

    // make sure the image2 is the same as image1;
    REQUIRE(image1.desc() == image2.desc());
    REQUIRE(0 == memcmp(image1.data(), image2.data(), image1.size()));
}
