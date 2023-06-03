set(CMAKE_CXX_STANDARD 20)
add_executable(rapid-image-test
    custom-namespace.cpp
    smoke-test.cpp
    stb-test.cpp
    test-main.cpp
)
# convert CMAKE_CURRENT_SOURCE_DIR to absolute path
get_filename_component(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
target_compile_definitions(rapid-image-test PRIVATE TEST_SOURCE_DIR="${SOURCE_DIR}")
target_link_libraries(rapid-image-test PUBLIC rapid-image-static)
