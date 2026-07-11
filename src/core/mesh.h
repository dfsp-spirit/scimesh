#pragma once

#include "types.h"
#include <vector>
#include <cmath>

namespace scimesh {

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    std::vector<Color> colors;
    std::vector<Vec3> normals;
    Color default_color = DEFAULT_COLOR;
    bool has_transparency = false;

    bool has_colors() const { return !colors.empty(); }
    bool has_normals() const { return !normals.empty(); }

    bool is_valid() const {
        for (const auto &tri : triangles) {
            if (tri.v0 >= vertices.size() || tri.v1 >= vertices.size() ||
                tri.v2 >= vertices.size())
                return false;
        }
        for (const auto &v : vertices) {
            if (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z))
                return false;
            if (std::isinf(v.x) || std::isinf(v.y) || std::isinf(v.z))
                return false;
        }
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
