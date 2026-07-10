#pragma once

#include "mesh.h"

namespace scimesh {

struct Scene {
    std::vector<Mesh> meshes;

    void compute_bounding_box(Vec3 &min_bound, Vec3 &max_bound) const {
        if (meshes.empty()) {
            min_bound = Vec3(0.0f);
            max_bound = Vec3(0.0f);
            return;
        }
        bool first = true;
        for (const auto &mesh : meshes) {
            if (mesh.vertices.empty())
                continue;
            Vec3 mesh_min, mesh_max;
            mesh.compute_bounding_box(mesh_min, mesh_max);
            if (first) {
                min_bound = mesh_min;
                max_bound = mesh_max;
                first = false;
            } else {
                min_bound = glm::min(min_bound, mesh_min);
                max_bound = glm::max(max_bound, mesh_max);
            }
        }
    }
};

} // namespace scimesh
