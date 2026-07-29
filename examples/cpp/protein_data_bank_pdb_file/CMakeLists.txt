cmake_minimum_required(VERSION 3.10)
project(protein_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Pull in the scimesh library from the repo root
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/scimesh_build)

add_executable(protein_demo main.cpp)
target_link_libraries(protein_demo PRIVATE scimesh)
