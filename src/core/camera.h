#pragma once

#include "types.h"

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

} // namespace scimesh
