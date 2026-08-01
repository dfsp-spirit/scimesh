cmake_minimum_required(VERSION 3.10)
project(scimesh_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Pull in the scimesh library from the repo root
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/scimesh_build)

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
    test_colormap.cpp
)

target_link_libraries(scimesh_tests PRIVATE scimesh)

# Output test images to a known directory
set_property(TARGET scimesh_tests PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Enable testing with CTest
enable_testing()
add_test(NAME scimesh_tests COMMAND scimesh_tests)
