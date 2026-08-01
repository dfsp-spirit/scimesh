/// @file scene.h
/// @brief The Scene — a collection of meshes rendered together.

#pragma once

#include "mesh.h"

namespace scimesh {

/// @brief A collection of `Mesh` objects to be rendered together.
///
/// A `Scene` is simply a list of meshes.  When rendered, meshes are drawn
/// in order — later meshes appear on top of earlier ones.
///
/// ## Construction
///
/// @code{.cpp}
/// Scene scene;
/// scene.meshes.push_back(sphere);
/// scene.meshes.push_back(cube);
/// scene.meshes.push_back(arrow);
/// @endcode
///
/// ## Bounding box
///
/// The combined bounding box of all meshes can be computed for camera framing:
///
/// @code{.cpp}
/// Vec3 bmin, bmax;
/// scene.compute_bounding_box(bmin, bmax);
/// Camera cam = camera_look_at((bmin+bmax)*0.5f, ...);
/// @endcode
///
/// @see Mesh, Renderer::render_scene(), camera_fit_scene()
struct Scene {
    /// @brief The meshes in this scene, drawn in order.
    std::vector<Mesh> meshes;

    /// @brief Compute the combined axis-aligned bounding box of all meshes.
    ///
    /// Iterates over all meshes and computes the union of their individual
    /// bounding boxes.  Empty meshes are skipped.
    ///
    /// @param[out] min_bound  Minimum corner of the combined AABB.
    /// @param[out] max_bound  Maximum corner of the combined AABB.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Vec3 bmin, bmax;
    /// scene.compute_bounding_box(bmin, bmax);
    /// std::cout << "Scene spans " << (bmax - bmin) << std::endl;
    /// @endcode
    ///
    /// @see Mesh::compute_bounding_box()
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
