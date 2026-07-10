cmake_minimum_required(VERSION 3.10)
project(scimesh_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include paths: core renderer headers and vendored GLM
include_directories(
    ${CMAKE_SOURCE_DIR}/../src/core
    ${CMAKE_SOURCE_DIR}/../src/core/glm
    ${CMAKE_SOURCE_DIR}
)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Source files for the renderer core library
set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/../src/core/image.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/camera.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/normals.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/clipping.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/rasterizer.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/renderer.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/tinyply.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/primitives.cpp
)

# Catch2 test runner (amalgamated)
add_executable(scimesh_tests
    catch_amalgamated.cpp
    main.cpp
    test_math.cpp
    test_image.cpp
    test_mesh.cpp
    test_camera.cpp
    test_normals.cpp
    test_clipping.cpp
    test_rasterizer.cpp
    test_integration.cpp
    test_visual_validation.cpp
    test_brain_mesh.cpp
    test_primitives.cpp
    ${CORE_SOURCES}
)

target_compile_features(scimesh_tests PRIVATE cxx_std_17)

# Output test images to a known directory
set_property(TARGET scimesh_tests PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Enable testing with CTest
enable_testing()
add_test(NAME scimesh_tests COMMAND scimesh_tests)
