/// @file ply_io.h
/// @brief Read Stanford PLY files (.ply).
///
/// PLY (Polygon File Format) is commonly used for 3D scanned data and
/// scientific meshes.  It supports both ASCII and binary encoding and
/// can store additional per-vertex properties like colors.

#pragma once

#include <scimesh/mesh.h>
#include <string>

namespace scimesh {
/// @brief Functions for reading PLY files.
namespace ply_io {

/// @brief Load a mesh from a Stanford PLY file.
///
/// Reads vertex positions, face indices, and optionally per-vertex colors
/// and normals.  Both ASCII and binary (little-endian) PLY files are
/// supported.
///
/// @param path Filesystem path to the `.ply` file.
/// @return A Mesh populated with the file's geometry.
///
/// @throws std::runtime_error if the file cannot be opened or parsed.
///
/// @par Example
/// @code{.cpp}
/// Mesh scan = scimesh::ply_io::read_ply("bunny.ply");
/// std::cout << "Loaded " << scan.vertices.size() << " vertices\n";
/// @endcode
///
/// @par Color support
/// If the PLY file contains per-vertex RGB colors, they are automatically
/// loaded into `Mesh::colors`.  Use `mesh.has_colors()` to check.
///
/// @see read_obj(), read_stl(), Mesh::has_colors()
Mesh read_ply(const std::string &path);

} // namespace ply_io
} // namespace scimesh
