/// @file normals.h
/// @brief Compute per-vertex surface normals for lighting.
///
/// Surface normals are direction vectors perpendicular to the surface at
/// each vertex.  They are essential for lighting — without normals, the
/// renderer cannot tell which way a surface is facing and cannot compute
/// shading.

#pragma once

#include "mesh.h"
#include "math_utils.h"

namespace scimesh {

/// @brief Compute per-vertex normals by averaging adjacent face normals.
///
/// For each vertex, this function finds all triangles that share that vertex,
/// computes each triangle's face normal using compute_face_normal(), and
/// averages them together (weighted equally).  The resulting normals are
/// suitable for smooth (Gouraud) shading.
///
/// @param[in]  mesh    The input mesh (only `vertices` and `triangles` are read).
/// @param[out] normals Output array — will be resized to `mesh.vertices.size()`
///                     and filled with unit-length normal vectors.
///
/// @par When do I need this?
/// Most file formats (OBJ, PLY, STL) may or may not include normals.
/// If you load a mesh and `mesh.has_normals()` returns `false`, call this
/// function to compute them before rendering with `ShadingMode::SMOOTH`.
///
/// @par Example
/// @code{.cpp}
/// Mesh m = scimesh::obj_io::read_obj("model.obj");
/// if (!m.has_normals()) {
///     compute_vertex_normals(m, m.normals);
/// }
/// // now render with smooth shading
/// @endcode
///
/// @see compute_face_normal(), Mesh::normals, Mesh::has_normals(), ShadingMode
void compute_vertex_normals(const Mesh &mesh, std::vector<Vec3> &normals);

} // namespace scimesh
