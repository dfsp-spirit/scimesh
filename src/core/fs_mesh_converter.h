/// @file fs_mesh_converter.h
/// @brief Convert FreeSurfer meshes (libfs) to scimesh Mesh objects.
///
/// FreeSurfer is a widely used neuroimaging software suite.  Its mesh format
/// (`fs::Mesh`) stores brain surface data with optional per-vertex
/// morphological measurements and RGB coloring.  These converters bridge
/// between the FreeSurfer C++ API (libfs) and scimesh's rendering pipeline.

#pragma once

#include "mesh.h"
#include "libfs.h"

#include <vector>
#include <cmath>

namespace scimesh {

/// @brief Convert a FreeSurfer mesh to a scimesh Mesh (no colors).
///
/// Copies vertex positions and face indices.  No colors are set — you
/// should set `Mesh::default_color` or populate `Mesh::colors` before
/// rendering.
///
/// @param fs_mesh The FreeSurfer mesh to convert.
/// @return A scimesh Mesh with vertices and triangles populated.
///
/// @par Example
/// @code{.cpp}
/// fs::Mesh fs_brain = fs::read_fs_mesh("lh.white");
/// Mesh brain = scimesh::convert_fs_mesh(fs_brain);
/// brain.default_color = Color(0.7f, 0.7f, 0.7f);
/// @endcode
///
/// @see convert_fs_mesh(const fs::Mesh&, const Color&),
///      convert_fs_mesh(const fs::Mesh&, const std::vector<uint8_t>&)
inline Mesh convert_fs_mesh(const fs::Mesh &fs_mesh) {
    Mesh out;
    size_t nv = fs_mesh.num_vertices();
    out.vertices.reserve(nv);
    for (size_t i = 0; i < nv; i++) {
        out.vertices.push_back(Vec3(
            fs_mesh.vertices[i * 3],
            fs_mesh.vertices[i * 3 + 1],
            fs_mesh.vertices[i * 3 + 2]));
    }
    size_t nf = fs_mesh.num_faces();
    out.triangles.reserve(nf);
    for (size_t i = 0; i < nf; i++) {
        out.triangles.push_back(Triangle{
            static_cast<uint32_t>(fs_mesh.faces[i * 3]),
            static_cast<uint32_t>(fs_mesh.faces[i * 3 + 1]),
            static_cast<uint32_t>(fs_mesh.faces[i * 3 + 2])});
    }
    return out;
}

/// @brief Convert a FreeSurfer mesh to a scimesh Mesh with a solid color.
///
/// All vertices are assigned the same `solid_color`.  This is a convenience
/// wrapper — it calls the basic converter, then fills `mesh.colors`.
///
/// @param fs_mesh     The FreeSurfer mesh to convert.
/// @param solid_color The uniform color to assign to all vertices.
/// @return A scimesh Mesh with vertices, triangles, and per-vertex colors.
///
/// @par Example
/// @code{.cpp}
/// fs::Mesh fs_brain = fs::read_fs_mesh("lh.white");
/// Mesh brain = scimesh::convert_fs_mesh(fs_brain, Color(0.5f, 0.5f, 0.5f));
/// @endcode
///
/// @see convert_fs_mesh(const fs::Mesh&)
inline Mesh convert_fs_mesh(const fs::Mesh &fs_mesh, const Color &solid_color) {
    Mesh out = convert_fs_mesh(fs_mesh);
    size_t nv = out.vertices.size();
    out.colors.reserve(nv);
    for (size_t i = 0; i < nv; i++) {
        out.colors.push_back(solid_color);
    }
    return out;
}

/// @brief Convert a FreeSurfer mesh with per-vertex RGB coloring.
///
/// Each vertex gets a color from the `rgb_colors` array (3 bytes per vertex:
/// red, green, blue, each 0–255).
///
/// @param fs_mesh    The FreeSurfer mesh to convert.
/// @param rgb_colors Flat array of RGB bytes (size = 3 × vertex count).
/// @return A scimesh Mesh with per-vertex colors.
///
/// @par Example
/// @code{.cpp}
/// std::vector<uint8_t> rgb = fs_mesh.get_vertex_rgb();
/// Mesh brain = scimesh::convert_fs_mesh(fs_mesh, rgb);
/// @endcode
///
/// @see convert_fs_mesh(const fs::Mesh&, const Color&)
inline Mesh convert_fs_mesh(const fs::Mesh &fs_mesh,
                            const std::vector<uint8_t> &rgb_colors) {
    Mesh out = convert_fs_mesh(fs_mesh);
    size_t nv = out.vertices.size();
    out.colors.reserve(nv);
    for (size_t i = 0; i < nv; i++) {
        out.colors.push_back(Color(
            rgb_colors[i * 3] / 255.0f,
            rgb_colors[i * 3 + 1] / 255.0f,
            rgb_colors[i * 3 + 2] / 255.0f,
            1.0f));
    }
    return out;
}

/// @brief Convert a FreeSurfer mesh with per-vertex morphological data and
///        RGB coloring.
///
/// This is the most feature-rich converter.  Each vertex gets:
/// - An RGB color from `rgb_colors` (3 bytes per vertex).
/// - If `morph_data[i]` is NaN, the vertex is colored white (often used to
///   mark the medial wall or "unknown" regions in brain surface data).
///
/// @param fs_mesh    The FreeSurfer mesh to convert.
/// @param morph_data Per-vertex scalar values (NaN = mark as white).
/// @param rgb_colors Flat array of RGB bytes (size = 3 × vertex count).
/// @return A scimesh Mesh with per-vertex colors.
///
/// @par Example
/// @code{.cpp}
/// fs::Mesh fs_brain = fs::read_fs_mesh("lh.white");
/// std::vector<float> curv = fs::read_curv("lh.thickness");
/// std::vector<uint8_t> rgb = fs_mesh.get_vertex_rgb();
/// Mesh brain = scimesh::convert_fs_mesh(fs_brain, curv, rgb);
/// @endcode
///
/// @see convert_fs_mesh(const fs::Mesh&, const std::vector<uint8_t>&)
inline Mesh convert_fs_mesh(const fs::Mesh &fs_mesh,
                            const std::vector<float> &morph_data,
                            const std::vector<uint8_t> &rgb_colors) {
    Mesh out = convert_fs_mesh(fs_mesh);
    size_t nv = out.vertices.size();
    out.colors.reserve(nv);
    for (size_t i = 0; i < nv; i++) {
        if (std::isnan(morph_data[i])) {
            out.colors.push_back(Color(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            out.colors.push_back(Color(
                rgb_colors[i * 3] / 255.0f,
                rgb_colors[i * 3 + 1] / 255.0f,
                rgb_colors[i * 3 + 2] / 255.0f,
                1.0f));
        }
    }
    return out;
}

} // namespace scimesh
