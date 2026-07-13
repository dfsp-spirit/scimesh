#pragma once

#include "mesh.h"
#include "libfs.h"

#include <vector>
#include <cmath>

namespace scimesh {

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

inline Mesh convert_fs_mesh(const fs::Mesh &fs_mesh, const Color &solid_color) {
    Mesh out = convert_fs_mesh(fs_mesh);
    size_t nv = out.vertices.size();
    out.colors.reserve(nv);
    for (size_t i = 0; i < nv; i++) {
        out.colors.push_back(solid_color);
    }
    return out;
}

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
