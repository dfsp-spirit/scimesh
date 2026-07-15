#pragma once

#include "types.h"
#include "scene.h"

namespace scimesh {

enum class ProjectionType {
    ORTHOGRAPHIC,
    PERSPECTIVE
};

struct Camera {
    Vec3 eye = Vec3(0.0f, 0.0f, 10.0f);
    Vec3 center = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
    ProjectionType projection = ProjectionType::PERSPECTIVE;
    float fov_degrees = 45.0f;

    Mat4 get_view_matrix() const;
    Mat4 get_projection_matrix(float aspect_ratio, float near_plane, float far_plane) const;
};

// Compute the camera distance needed to keep every AABB corner in view.
// This is tighter than the sphere-radius method because it accounts for
// the actual shape of the mesh: corners further from the centre plane
// require less margin (they are further from the camera), while corners
// closer to the camera enlarge the visible extent and constrain more.
//
// The returned value is fed to the same distance formula as a sphere
// radius:  dist = radius / sin(fov/2) * margin.
inline float perp_extent_radius(const Vec3 &bmin, const Vec3 &bmax,
                                 const Vec3 &center, const Vec3 &dir,
                                 float fov_radians, float *out_dist = nullptr) {
    Vec3 corners[8] = {
        Vec3(bmin.x, bmin.y, bmin.z), Vec3(bmax.x, bmin.y, bmin.z),
        Vec3(bmin.x, bmax.y, bmin.z), Vec3(bmax.x, bmax.y, bmin.z),
        Vec3(bmin.x, bmin.y, bmax.z), Vec3(bmax.x, bmin.y, bmax.z),
        Vec3(bmin.x, bmax.y, bmax.z), Vec3(bmax.x, bmax.y, bmax.z),
    };
    float max_dist = 0.0f;
    float half_tan = std::tan(fov_radians * 0.5f);
    if (half_tan < 1e-6f) half_tan = 1e-6f;

    for (int i = 0; i < 8; i++) {
        Vec3 delta = corners[i] - center;
        float along  = glm::dot(delta, dir);
        float perp_sq = glm::dot(delta, delta) - along * along;
        float perp = std::sqrt(perp_sq);
        // At the corner''s depth (centre_z + along), required eye distance
        // so that this corner stays inside the frustum:
        //   perp  ≤  (dist - along) * tan(half_fov)
        // ⇒ dist  ≥  perp / tan(half_fov)  +  along
        float d = perp / half_tan + along;
        if (d > max_dist) max_dist = d;
    }
    if (max_dist < 1e-6f) max_dist = 1.0f;

    if (out_dist) *out_dist = max_dist;
    // Convert back to the "sphere radius" that camera_look_at expects.
    float sin_half = std::sin(fov_radians * 0.5f);
    if (sin_half < 1e-6f) sin_half = 1e-6f;
    return max_dist * sin_half;
}

// Maximum perpendicular extent of the AABB from the view ray through center.
// Used for orthographic projection framing (no FOV dependence).
inline float max_ortho_extent(const Vec3 &bmin, const Vec3 &bmax,
                               const Vec3 &center, const Vec3 &dir) {
    Vec3 corners[8] = {
        Vec3(bmin.x, bmin.y, bmin.z), Vec3(bmax.x, bmin.y, bmin.z),
        Vec3(bmin.x, bmax.y, bmin.z), Vec3(bmax.x, bmax.y, bmin.z),
        Vec3(bmin.x, bmin.y, bmax.z), Vec3(bmax.x, bmin.y, bmax.z),
        Vec3(bmin.x, bmax.y, bmax.z), Vec3(bmax.x, bmax.y, bmax.z),
    };
    float max_perp = 0.0f;
    for (int i = 0; i < 8; i++) {
        Vec3 delta = corners[i] - center;
        float along = glm::dot(delta, dir);
        float perp = std::sqrt(std::max(0.0f, glm::dot(delta, delta) - along * along));
        if (perp > max_perp) max_perp = perp;
    }
    if (max_perp < 1e-6f) max_perp = 1.0f;
    return max_perp;
}

Camera camera_look_at(const Vec3 &center, float radius,
                      const Vec3 &direction, const Vec3 &up,
                      float fov_degrees, float margin = 1.1f,
                      ProjectionType projection = ProjectionType::PERSPECTIVE);

Camera camera_fit_scene(const Scene &scene, const Vec3 &direction,
                        const Vec3 &up, float fov_degrees, float margin = 1.1f,
                        ProjectionType projection = ProjectionType::PERSPECTIVE);

Camera camera_fit_mesh(const Mesh &mesh, const Vec3 &direction,
                       const Vec3 &up, float fov_degrees, float margin = 1.1f,
                       ProjectionType projection = ProjectionType::PERSPECTIVE);

Camera camera_orbit(const Camera &camera, const Vec3 &axis, float angle_degrees);

} // namespace scimesh
