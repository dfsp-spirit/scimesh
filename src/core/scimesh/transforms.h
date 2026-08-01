/// @file transforms.h
/// @brief Mesh transformation functions: translate, scale, rotate, and
///        general matrix transforms.
///
/// These functions modify meshes **in-place**.  All transformations are
/// applied to vertex positions; normals are also updated when present.

#pragma once

#include <scimesh/mesh.h>

namespace scimesh {

/// @brief Translate (move) a mesh by a displacement vector.
///
/// Adds `translation` to every vertex position.
///
/// @param[in,out] mesh        The mesh to modify.
/// @param         translation The displacement vector to add.
///
/// @par Example
/// @code{.cpp}
/// translate_mesh(mesh, Vec3(0.0f, 5.0f, 0.0f));  // move 5 units up
/// @endcode
///
/// @see scale_mesh(), rotate_mesh(), transform_mesh()
void translate_mesh(Mesh &mesh, const Vec3 &translation);

/// @brief Scale a mesh non-uniformly along each axis.
///
/// Multiplies each vertex position component-wise by `scale`.
///
/// @param[in,out] mesh  The mesh to modify.
/// @param         scale Scale factors per axis (e.g., {2,1,1} doubles width).
///
/// @par Example
/// @code{.cpp}
/// scale_mesh(mesh, Vec3(2.0f, 1.0f, 0.5f));  // double X, halve Z
/// @endcode
///
/// @see scale_mesh(Mesh&, float), rotate_mesh(), translate_mesh()
void scale_mesh(Mesh &mesh, const Vec3 &scale);

/// @brief Scale a mesh uniformly in all directions.
///
/// Multiplies every vertex position by `uniform_scale`.
///
/// @param[in,out] mesh          The mesh to modify.
/// @param         uniform_scale Scale factor (1.0 = unchanged, 2.0 = double size).
///
/// @par Example
/// @code{.cpp}
/// scale_mesh(mesh, 0.5f);  // half size
/// @endcode
///
/// @see scale_mesh(Mesh&, const Vec3&)
void scale_mesh(Mesh &mesh, float uniform_scale);

/// @brief Rotate a mesh around an arbitrary axis.
///
/// Uses the right-hand rule: positive angle = counter-clockwise when
/// looking along the axis toward the origin.
///
/// @param[in,out] mesh          The mesh to modify.
/// @param         angle_radians Rotation angle in **radians**.
/// @param         axis          Rotation axis (does not need to be normalized).
///
/// @par Example
/// @code{.cpp}
/// #include <cmath>
/// rotate_mesh(mesh, M_PI / 2.0f, Vec3(0, 1, 0));  // 90° around Y axis
/// @endcode
///
/// @see translate_mesh(), transform_mesh()
void rotate_mesh(Mesh &mesh, float angle_radians, const Vec3 &axis);

/// @brief Apply an arbitrary 4×4 transformation matrix to a mesh.
///
/// Transforms all vertex positions by the matrix.  This is the most general
/// transform function — you can combine translation, rotation, and scale
/// into a single matrix using GLM functions like `glm::translate()`,
/// `glm::rotate()`, and `glm::scale()`.
///
/// @param[in,out] mesh   The mesh to modify.
/// @param         matrix A 4×4 transformation matrix (column-major, GLM style).
///
/// @par Example
/// @code{.cpp}
/// Mat4 T = glm::translate(Mat4(1.0f), Vec3(1, 0, 0));  // translate +X
/// Mat4 R = glm::rotate(Mat4(1.0f), M_PI/2, Vec3(0,1,0)); // rotate 90° Y
/// Mat4 M = T * R;  // combine: rotate, then translate
/// transform_mesh(mesh, M);
/// @endcode
///
/// @see translate_mesh(), rotate_mesh(), scale_mesh()
void transform_mesh(Mesh &mesh, const Mat4 &matrix);

/// @brief Convert a FreeSurfer-format mesh (flat vertex/face arrays) to a
///        scimesh Mesh.
///
/// FreeSurfer is a neuroimaging software suite.  Its mesh format stores
/// vertices as a flat `float` array (3 per vertex) and faces as a flat
/// `uint32_t` array (3 per face).  This function converts those raw arrays
/// into a scimesh `Mesh` with optional per-vertex coloring.
///
/// @param fs_vertices         Flat array of vertex coordinates (x0,y0,z0, x1,y1,z1, ...).
/// @param fs_faces            Flat array of face indices (v0,v1,v2, v0,v1,v2, ...).
/// @param per_vertex_values   Optional scalar per-vertex values for coloring.
/// @param rgb_bytes           Optional RGB color bytes (3 per vertex: r,g,b, ...).
/// @param detect_transparency If `true`, check for NaN values indicating
///                            transparent regions (common in brain surface data).
/// @return A scimesh Mesh.
///
/// @par Example
/// @code{.cpp}
/// std::vector<float> verts = {0,0,0, 1,0,0, 0,1,0, 1,1,0};
/// std::vector<uint32_t> faces = {0,1,2, 1,3,2};
/// Mesh m = mesh_from_fs(verts, faces);
/// @endcode
///
/// @see Mesh, fs_mesh_converter.h
Mesh mesh_from_fs(const std::vector<float> &fs_vertices,
                  const std::vector<uint32_t> &fs_faces,
                  const std::vector<float> &per_vertex_values = {},
                  const std::vector<uint8_t> &rgb_bytes = {},
                  bool detect_transparency = false);

} // namespace scimesh
