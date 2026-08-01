/// @file obj_io.h
/// @brief Read Wavefront OBJ files (.obj).
///
/// OBJ is one of the most widely supported 3D file formats.  It is a
/// simple text format that stores vertices, faces, normals, and texture
/// coordinates.  Almost every 3D modeling program can export to OBJ.

#pragma once

#include <scimesh/mesh.h>
#include <string>

namespace scimesh {
/// @brief Functions for reading OBJ files.
namespace obj_io {

/// @brief Load a mesh from a Wavefront OBJ file.
///
/// Reads vertex positions, face indices, and optionally normals and texture
/// coordinates from the file.  OBJ is a text format — you can open `.obj`
/// files in any text editor to inspect them.
///
/// @param path Filesystem path to the `.obj` file.
/// @return A Mesh populated with the file's geometry.
///
/// @throws std::runtime_error if the file cannot be opened or parsed.
///
/// @par Example
/// @code{.cpp}
/// Mesh model = scimesh::obj_io::read_obj("teapot.obj");
/// if (!model.is_valid()) {
///     std::cerr << "Failed to load valid mesh from OBJ file\n";
/// }
/// @endcode
///
/// @par Supported features
/// - Vertex positions (v)
/// - Faces as triangles (f v1 v2 v3)
/// - Vertex normals (vn)
/// - Texture coordinates (vt)
///
/// @par Unsupported
/// - Polygons with more than 3 vertices (use triangulated meshes)
/// - Materials (.mtl) — only geometry is loaded, colors are not imported
///   from material files.
///
/// @see read_ply(), read_stl(), Mesh::is_valid()
Mesh read_obj(const std::string &path);

} // namespace obj_io
} // namespace scimesh
