// include stb image header to enable load/save from common image formats like png, jpg, bmp, tga, ...
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "../3rd-party/stb/stb_image.h"

#define RAPID_IMAGE_IMPLEMENTATION
#include "../../inc/rapid-image/rapid-image.h"

#include "../3rd-party/catch2/catch.hpp"

TEST_CASE("load-jpg") {
    auto path = std::filesystem::path(TEST_SOURCE_DIR) / "alien-planet.jpg";
    RAPID_IMAGE_LOGI("load from file: %s", path.string().c_str());
    auto image = ril::Image::load(path.string());
    REQUIRE(!image.empty());
}
