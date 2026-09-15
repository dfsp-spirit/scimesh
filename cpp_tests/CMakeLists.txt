cmake_minimum_required(VERSION 3.10)
project(scimesh_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# ---- optional LLVM source-based coverage (development tool) -----------------
# Configure with -DSCIMESH_COVERAGE=ON and clang to instrument the tests *and*
# the scimesh library (which is added as a subdirectory below, and inherits
# these compile options).  Instrumenting everything matters: for functions that
# are defined inline in headers, the linker picks one of the identical copies,
# so instrumenting only the library would report those headers as uncovered.
# See dev_tools/coverage_cpp.sh for the full workflow (build, run, report).
option(SCIMESH_COVERAGE "Instrument the C++ tests and scimesh for LLVM source-based coverage" OFF)
if(SCIMESH_COVERAGE)
    if(CMAKE_VERSION VERSION_LESS 3.13)
        message(FATAL_ERROR "SCIMESH_COVERAGE requires CMake >= 3.13")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
    message(STATUS "scimesh_tests: LLVM source-based coverage instrumentation enabled")
endif()

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
    test_clip_planes.cpp
    test_fog.cpp
    test_io.cpp
    test_transforms.cpp
    test_to_string.cpp
    test_render_features.cpp
    test_rasterizer.cpp
    test_integration.cpp
    test_visual_validation.cpp
    test_brain_mesh.cpp
    test_primitives.cpp
    test_colormap.cpp
    test_scene_transforms.cpp
)

target_link_libraries(scimesh_tests PRIVATE scimesh)

if(SCIMESH_COVERAGE)
    # The amalgamated Catch2 is huge and never part of the coverage report;
    # skip instrumenting it to keep the build fast and the profiles small.
    set_source_files_properties(catch_amalgamated.cpp PROPERTIES
        COMPILE_OPTIONS "-fno-profile-instr-generate;-fno-coverage-mapping")
endif()

# Output test images to a known directory
set_property(TARGET scimesh_tests PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Enable testing with CTest
enable_testing()
add_test(NAME scimesh_tests COMMAND scimesh_tests)
