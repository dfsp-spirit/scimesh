cmake_minimum_required(VERSION 3.10)
project(full_CLI_renderer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Pull in the scimesh library from the repo root
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/scimesh_build)

add_executable(scimesh_render main.cpp)
target_link_libraries(scimesh_render PRIVATE scimesh)

# Copy config.toml next to the executable so it's found by default
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.toml
    ${CMAKE_CURRENT_BINARY_DIR}/config.toml
    COPYONLY
)
