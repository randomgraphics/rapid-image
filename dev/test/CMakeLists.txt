set(CMAKE_CXX_STANDARD 20)
add_executable(rapid-image-test
    smoke-test.cpp
    test-main.cpp
)
target_link_libraries(rapid-image-test PUBLIC rapid-image-static)
