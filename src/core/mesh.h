#pragma once

#include "types.h"
#include "image.h"
#include <vector>
#include <cmath>

namespace scimesh {

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    std::vector<Color> colors;
    std::vector<Color> face_colors;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    Image texture;
    Color default_color = DEFAULT_COLOR;
    bool has_transparency = false;

    bool has_colors() const { return !colors.empty(); }
    bool has_face_colors() const { return !face_colors.empty(); }
    bool has_normals() const { return !normals.empty(); }
    bool has_uvs() const { return !uvs.empty(); }
    bool has_texture() const { return texture.width > 0; }

    bool is_valid() const {
        // Geometry must exist
        if (vertices.empty()) return false;
        if (triangles.empty()) return false;

        // Triangle indices must be in-bounds
        const auto nv = vertices.size();
        for (const auto &tri : triangles) {
            if (tri.v0 >= nv || tri.v1 >= nv || tri.v2 >= nv)
                return false;
        }

        // No degenerate geometry
        for (const auto &v : vertices) {
            if (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z))
                return false;
            if (std::isinf(v.x) || std::isinf(v.y) || std::isinf(v.z))
                return false;
        }

        // Optional arrays: if present, must match vertex or face count
        if (!colors.empty() && colors.size() != nv) return false;
        if (!face_colors.empty() && face_colors.size() != triangles.size())
            return false;
        if (!normals.empty() && normals.size() != nv) return false;
        if (!uvs.empty() && uvs.size() != nv) return false;

        return true;
    }

    bool empty() const { return vertices.empty() || triangles.empty(); }

    void compute_bounding_box(Vec3 &min_bound, Vec3 &max_bound) const {
        if (vertices.empty()) {
            min_bound = Vec3(0.0f);
            max_bound = Vec3(0.0f);
            return;
        }
        min_bound = vertices[0];
        max_bound = vertices[0];
        for (const auto &v : vertices) {
            min_bound = glm::min(min_bound, v);
            max_bound = glm::max(max_bound, v);
        }
    }
};

} // namespace scimesh
