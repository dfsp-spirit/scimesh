/// @file scene.h
/// @brief The Scene — a collection of meshes rendered together.

#pragma once

#include <scimesh/mesh.h>
#include <scimesh/math_utils.h>
#include <string>

namespace scimesh {

/// @brief A lightweight, non-owning reference to one mesh in a scene, together
///        with its placement transform (and optional name).
///
/// Used by the renderer pipeline and by exporters (e.g. glTF) to consume a
/// scene without copying mesh data.  The mesh itself is owned by the `Scene`.
///
/// @see Scene, Scene::node(), Scene::nodes()
struct SceneNodeRef {
    const Mesh *mesh = nullptr;     ///< Pointer to the owned mesh (never null for a valid node).
    Mat4 transform = Mat4(1.0f);    ///< Model matrix placing the mesh in world space (identity = as-is).
    std::string name;               ///< Optional node name (used by exporters / debugging).
};

/// @brief A collection of `Mesh` objects to be rendered together.
///
/// A `Scene` is a list of meshes with optional per-mesh placement transforms
/// and names.  When rendered, meshes are drawn in order — later meshes appear
/// on top of earlier ones.  Each mesh's placement transform is applied as a
/// model matrix at render time (meshes themselves are never modified).
///
/// ## Construction
///
/// @code{.cpp}
/// Scene scene;
/// scene.add(sphere);   // identity transform
/// scene.add(cube, glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f)));
/// scene.add(arrow_mesh, glm::rotate(Mat4(1.0f), 0.5f, Vec3(0,1,0)), "arrow");
/// @endcode
///
/// For convenience the raw `meshes` vector is still public and can be modified
/// directly (e.g. `scene.meshes.push_back(m)`); any mesh without a matching
/// transform is treated as identity.
///
/// ## Bounding box
///
/// The combined bounding box of all meshes (after their transforms) can be
/// computed for camera framing:
///
/// @code{.cpp}
/// Vec3 bmin, bmax;
/// scene.compute_bounding_box(bmin, bmax);
/// Camera cam = camera_look_at((bmin+bmax)*0.5f, ...);
/// @endcode
///
/// @see Mesh, SceneNodeRef, Renderer::render_scene(), camera_fit_scene()
struct Scene {
    /// @brief The meshes in this scene, drawn in order.
    std::vector<Mesh> meshes;

    /// @brief Per-mesh placement transforms, parallel to `meshes`.
    ///
    /// May be shorter than `meshes`; missing entries default to identity.
    std::vector<Mat4> transforms;

    /// @brief Optional per-mesh node names, parallel to `meshes`.
    std::vector<std::string> names;

    /// @brief Add a mesh to the scene with an optional placement transform and name.
    ///
    /// @param mesh      The mesh to add (copied into the scene).
    /// @param transform Model matrix placing the mesh in world space
    ///                  (default: identity).
    /// @param name      Optional node name (used by exporters / debugging).
    void add(const Mesh &mesh, const Mat4 &transform = Mat4(1.0f),
             const std::string &name = "") {
        meshes.push_back(mesh);
        transforms.push_back(transform);
        names.push_back(name);
    }

    /// @brief Set the placement transform of the mesh at `index`.
    void set_transform(size_t index, const Mat4 &transform) {
        if (index >= meshes.size())
            return;
        while (transforms.size() <= index)
            transforms.push_back(Mat4(1.0f));
        transforms[index] = transform;
    }

    /// @brief Set the name of the mesh at `index`.
    void set_name(size_t index, const std::string &name) {
        if (index >= meshes.size())
            return;
        while (names.size() <= index)
            names.push_back("");
        names[index] = name;
    }

    /// @brief The placement transform of the mesh at `index` (identity when unset).
    const Mat4 &transform(size_t index) const {
        static const Mat4 kIdentity(1.0f);
        if (index >= transforms.size())
            return kIdentity;
        return transforms[index];
    }

    /// @brief The name of the mesh at `index` (empty when unset).
    const std::string &name(size_t index) const {
        static const std::string kEmpty;
        if (index >= names.size())
            return kEmpty;
        return names[index];
    }

    /// @brief Number of meshes in the scene.
    size_t size() const { return meshes.size(); }

    /// @brief Whether the scene holds no meshes.
    bool empty() const { return meshes.empty(); }

    /// @brief A non-owning reference to the mesh at `index` (mesh + transform + name).
    ///
    /// @pre `index < meshes.size()`
    SceneNodeRef node(size_t index) const {
        SceneNodeRef r;
        r.mesh = &meshes[index];
        r.transform = transform(index);
        r.name = name(index);
        return r;
    }

    /// @brief Non-owning references to all meshes, in draw order.
    std::vector<SceneNodeRef> nodes() const {
        std::vector<SceneNodeRef> out;
        out.reserve(meshes.size());
        for (size_t i = 0; i < meshes.size(); ++i)
            out.push_back(node(i));
        return out;
    }

    /// @brief Compute the combined axis-aligned bounding box of all meshes,
    ///        after applying each mesh's placement transform.
    ///
    /// Iterates over all meshes, transforms the 8 corners of each mesh's
    /// bounding box, and computes the union.  Empty meshes are skipped.
    ///
    /// @param[out] min_bound  Minimum corner of the combined AABB.
    /// @param[out] max_bound  Maximum corner of the combined AABB.
    ///
    /// @see Mesh::compute_bounding_box()
    void compute_bounding_box(Vec3 &min_bound, Vec3 &max_bound) const {
        bool first = true;
        for (size_t i = 0; i < meshes.size(); ++i) {
            const Mesh &mesh = meshes[i];
            if (mesh.vertices.empty())
                continue;
            Vec3 mesh_min, mesh_max;
            mesh.compute_bounding_box(mesh_min, mesh_max);
            const Mat4 &m = transform(i);
            for (int c = 0; c < 8; ++c) {
                Vec3 corner(
                    (c & 1) ? mesh_max.x : mesh_min.x,
                    (c & 2) ? mesh_max.y : mesh_min.y,
                    (c & 4) ? mesh_max.z : mesh_min.z);
                Vec3 t = transform_point(m, corner);
                if (first) {
                    min_bound = t;
                    max_bound = t;
                    first = false;
                } else {
                    min_bound = glm::min(min_bound, t);
                    max_bound = glm::max(max_bound, t);
                }
            }
        }
        if (first) {  // scene empty or all meshes empty
            min_bound = Vec3(0.0f);
            max_bound = Vec3(0.0f);
        }
    }
};

} // namespace scimesh
