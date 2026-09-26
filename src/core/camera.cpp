#include <scimesh/camera.h>
#include <scimesh/math_utils.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>

namespace scimesh {

Mat4 Camera::get_view_matrix() const {
    return glm::lookAt(eye, center, up);
}

Mat4 Camera::get_projection_matrix(float aspect_ratio, float near_plane, float far_plane) const {
    if (projection == ProjectionType::PERSPECTIVE) {
        if (aspect_ratio <= 0.0f) {
            throw std::invalid_argument(
                "aspect_ratio must be > 0 for perspective projection");
        }
        if (near_plane <= 0.0f) {
            throw std::invalid_argument(
                "near_plane must be > 0 for perspective projection");
        }
        if (far_plane <= near_plane) {
            throw std::invalid_argument(
                "far_plane must be > near_plane for perspective projection");
        }
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

ProjectedPoint world_to_screen(const Camera &camera, const Vec3 &world, int width,
                               int height, ProjectionType projection,
                               float near_plane, float far_plane) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("world_to_screen: width and height must be > 0");
    }
    ProjectedPoint out;

    // Same matrices as the render pipeline: the render options decide the
    // projection type, the camera only provides its position and framing.
    Camera proj_cam = camera;
    proj_cam.projection = projection;
    const Mat4 view = camera.get_view_matrix();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const Mat4 projection_matrix =
        proj_cam.get_projection_matrix(aspect, near_plane, far_plane);

    const Vec4 clip = transform_point_homogeneous(projection_matrix * view, world);
    if (clip.w <= 1e-6f) {
        return out;  // at or behind the camera plane: not visible
    }

    const Vec3 ndc = perspective_divide(clip);
    float screen_x = 0.0f, screen_y = 0.0f, depth = 0.0f;
    ndc_to_screen(ndc, width, height, screen_x, screen_y, depth);

    out.pixel = Vec2(screen_x, screen_y);
    out.depth = depth;
    out.in_front = true;
    return out;
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

Camera camera_orbit(const Camera &camera, const Vec3 &axis, float angle_degrees) {
    Camera result = camera;
    float angle_rad = glm::radians(angle_degrees);
    Mat4 rotation = glm::rotate(Mat4(1.0f), angle_rad, axis);
    Vec4 rotated_dir = rotation * Vec4(camera.eye - camera.center, 0.0f);
    result.eye = camera.center + Vec3(rotated_dir);
    Vec4 rotated_up = rotation * Vec4(camera.up, 0.0f);
    result.up = glm::normalize(Vec3(rotated_up));
    return result;
}

} // namespace scimesh
