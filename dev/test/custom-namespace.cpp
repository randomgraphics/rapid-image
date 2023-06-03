// this is to verify that the library can compile with a custom namespace
#define RAPID_IMAGE_NAMESPACE custom_ril_namespace
#define RAPID_IMAGE_IMPLEMENTATION
#include "../../inc/rapid-image/rapid-image.h"

#include "../3rd-party/catch2/catch.hpp"

TEST_CASE("custom-namespace") { custom_ril_namespace::Image image; }
