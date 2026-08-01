cmake_minimum_required(VERSION 3.10)
project(scimesh VERSION 0.2.8 LANGUAGES CXX)

# ---- scimesh static library ------------------------------------------------
# This is the core C++ software renderer.  Link against this target to use
# scimesh in your own CMake project.  The target can be consumed via
# add_subdirectory() or FetchContent.
#
# Usage from an external project with FetchContent:
#
#   include(FetchContent)
#   FetchContent_Declare(
#       scimesh
#       GIT_REPOSITORY https://github.com/dfsp-spirit/scimesh.git
#       GIT_TAG        main          # or a release tag, e.g. v0.2.8
#   )
#   FetchContent_MakeAvailable(scimesh)
#
#   add_executable(my_app main.cpp)
#   target_link_libraries(my_app PRIVATE scimesh::scimesh)

add_library(scimesh STATIC
    src/core/image.cpp
    src/core/camera.cpp
    src/core/colormap.cpp
    src/core/normals.cpp
    src/core/clipping.cpp
    src/core/rasterizer.cpp
    src/core/renderer.cpp
    src/core/primitives.cpp
    src/core/transforms.cpp
    src/core/ply_io.cpp
    src/core/obj_io.cpp
    src/third_party/tinyply.cpp
)

# Alias target for modern CMake usage (scimesh::scimesh)
add_library(scimesh::scimesh ALIAS scimesh)

# ---- include paths ----------------------------------------------------------
target_include_directories(scimesh PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(scimesh SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/third_party>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/glm>
)

# ---- compile features & definitions -----------------------------------------
target_compile_features(scimesh PUBLIC cxx_std_17)
target_compile_definitions(scimesh PRIVATE SCIMESH_STB_WRITE_IMPL)

# ---- generated version header -----------------------------------------------
# Generates scimesh/version.h from the project version declared above.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scimesh/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/scimesh/version.h
    @ONLY
)
target_include_directories(scimesh PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# ---- optional OpenMP --------------------------------------------------------
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(scimesh PUBLIC OpenMP::OpenMP_CXX)
endif()

# ---- install rules ----------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS scimesh
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY src/core/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "*.h.in" EXCLUDE
    PATTERN "*.o" EXCLUDE
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/scimesh/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/scimesh
)
