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
        // Orthographic: use symmetric frustum based on a unit-ish scale
        // The caller should adjust eye distance or we use a fixed scale.
        // We compute ortho bounds from a simple scale = 1 (caller can pre-scale mesh).
        // For auto-framing compatibility, we use the distance from eye to center as half-size.
        float dist = glm::length(eye - center);
        if (dist < 1e-6f)
            dist = 1.0f;
        float half_h = dist;
        float half_w = half_h * aspect_ratio;
        return glm::ortho(-half_w, half_w, -half_h, half_h, near_plane, far_plane);
    }
}

} // namespace scimesh
