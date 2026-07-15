cmake_minimum_required(VERSION 3.10)
project(whole_brain_sulc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}/../../../src/core
)
include_directories(SYSTEM
    ${CMAKE_SOURCE_DIR}/../../../src/third_party
    ${CMAKE_SOURCE_DIR}/../../../src/third_party/glm
)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/../../../src/core/image.cpp
    ${CMAKE_SOURCE_DIR}/../../../src/core/camera.cpp
    ${CMAKE_SOURCE_DIR}/../../../src/core/normals.cpp
    ${CMAKE_SOURCE_DIR}/../../../src/core/clipping.cpp
    ${CMAKE_SOURCE_DIR}/../../../src/core/rasterizer.cpp
    ${CMAKE_SOURCE_DIR}/../../../src/core/renderer.cpp
    ${CMAKE_SOURCE_DIR}/../../../src/core/primitives.cpp
)

add_executable(whole_brain_sulc
    main.cpp
    ${CORE_SOURCES}
)

target_compile_features(whole_brain_sulc PRIVATE cxx_std_17)
target_compile_definitions(whole_brain_sulc PRIVATE SCIMESH_STB_WRITE_IMPL)
