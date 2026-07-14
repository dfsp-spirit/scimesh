#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace scimesh {

Mat4 Camera::get_view_matrix() const {
    return glm::lookAt(eye, center, up);
}

Mat4 Camera::get_projection_matrix(float aspect_ratio, float near_plane, float far_plane) const {
    if (projection == ProjectionType::PERSPECTIVE) {
        return glm::perspective(glm::radians(fov_degrees), aspect_ratio, near_plane, far_plane);
    } else {
        float dist = glm::length(eye - center);
        if (dist < 1e-6f)
            dist = 1.0f;
        float half_h = dist;
        float half_w = half_h * aspect_ratio;
        return glm::ortho(-half_w, half_w, -half_h, half_h, near_plane, far_plane);
    }
}

Camera camera_look_at(const Vec3 &center, float radius,
                      const Vec3 &direction, const Vec3 &up,
                      float fov_degrees, float margin,
                      ProjectionType projection) {
    Camera cam;
    cam.center = center;
    cam.up = glm::normalize(up);
    cam.fov_degrees = fov_degrees;
    cam.projection = projection;
    float fov_rad = glm::radians(fov_degrees);
    float dist;
    if (projection == ProjectionType::ORTHOGRAPHIC) {
        dist = radius * margin;
    } else {
        dist = radius / std::sin(fov_rad * 0.5f) * margin;
    }
    cam.eye = center + glm::normalize(direction) * dist;
    return cam;
}

Camera camera_fit_scene(const Scene &scene, const Vec3 &direction,
                        const Vec3 &up, float fov_degrees, float margin,
                        ProjectionType projection) {
    Vec3 bmin(0.0f), bmax(0.0f);
    scene.compute_bounding_box(bmin, bmax);
    Vec3 center = (bmin + bmax) * 0.5f;
    Vec3 dir = glm::normalize(direction);
    if (projection == ProjectionType::ORTHOGRAPHIC) {
        float extent = max_ortho_extent(bmin, bmax, center, dir);
        return camera_look_at(center, extent, direction, up, fov_degrees, margin, projection);
    } else {
        float radius = perp_extent_radius(bmin, bmax, center, dir,
                                           glm::radians(fov_degrees));
        return camera_look_at(center, radius, direction, up, fov_degrees, margin, projection);
    }
}

Camera camera_fit_mesh(const Mesh &mesh, const Vec3 &direction,
                       const Vec3 &up, float fov_degrees, float margin,
                       ProjectionType projection) {
    Vec3 bmin(0.0f), bmax(0.0f);
    mesh.compute_bounding_box(bmin, bmax);
    Vec3 center = (bmin + bmax) * 0.5f;
    Vec3 dir = glm::normalize(direction);
    if (projection == ProjectionType::ORTHOGRAPHIC) {
        float extent = max_ortho_extent(bmin, bmax, center, dir);
        return camera_look_at(center, extent, direction, up, fov_degrees, margin, projection);
    } else {
        float radius = perp_extent_radius(bmin, bmax, center, dir,
                                           glm::radians(fov_degrees));
        return camera_look_at(center, radius, direction, up, fov_degrees, margin, projection);
    }
}

} // namespace scimesh
