# Setup common build flags. Do this after glfw build
if (MSVC)
    add_compile_options(/MP /W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -Wconversion)
endif()
if (UNIX)
    link_libraries(dl)
endif()
if(ANDROID)
    link_libraries(dl log)
endif()

# Build rapid-image into a static library
add_library(rapid-image-static STATIC ril.cpp)
target_compile_features(rapid-image-static PUBLIC cxx_std_17)
target_compile_definitions(rapid-image-static PUBLIC  $<$<CONFIG:Debug>:RAPID_IMAGE_ENABLE_DEBUG_BUILD=1>)
target_include_directories(rapid-image-static PUBLIC ../inc)

# Build tests
add_subdirectory(test)
