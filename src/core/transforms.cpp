#include "transforms.h"
#include "math_utils.h"
#include <glm/gtc/matrix_transform.hpp>

namespace scimesh {

void translate_mesh(Mesh &mesh, const Vec3 &translation) {
    Mat4 m = glm::translate(Mat4(1.0f), translation);
    for (auto &v : mesh.vertices) {
        v = transform_point(m, v);
    }
}

void scale_mesh(Mesh &mesh, const Vec3 &scale) {
    Mat4 m = glm::scale(Mat4(1.0f), scale);
    for (auto &v : mesh.vertices) {
        v = transform_point(m, v);
    }
}

void scale_mesh(Mesh &mesh, float uniform_scale) {
    scale_mesh(mesh, Vec3(uniform_scale));
}

void rotate_mesh(Mesh &mesh, float angle_radians, const Vec3 &axis) {
    Mat4 m = glm::rotate(Mat4(1.0f), angle_radians, axis);
    for (auto &v : mesh.vertices) {
        v = transform_point(m, v);
    }
}

void transform_mesh(Mesh &mesh, const Mat4 &matrix) {
    for (auto &v : mesh.vertices) {
        v = transform_point(matrix, v);
    }
}

} // namespace scimesh
