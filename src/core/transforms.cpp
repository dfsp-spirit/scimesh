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

Mesh mesh_from_fs(const std::vector<float> &fs_vertices,
                  const std::vector<uint32_t> &fs_faces,
                  const std::vector<float> &per_vertex_values,
                  const std::vector<uint8_t> &rgb_bytes,
                  bool detect_transparency) {
    Mesh out;
    size_t nv = fs_vertices.size() / 3;
    out.vertices.reserve(nv);

    bool have_values = !per_vertex_values.empty();
    bool have_rgb = !rgb_bytes.empty();

    for (size_t i = 0; i < nv; i++) {
        out.vertices.push_back(Vec3(
            fs_vertices[i * 3],
            fs_vertices[i * 3 + 1],
            fs_vertices[i * 3 + 2]));

        if (have_values && std::isnan(per_vertex_values[i])) {
            out.colors.push_back(Color(1.0f, 1.0f, 1.0f, 1.0f));
        } else if (have_rgb) {
            out.colors.push_back(Color(
                rgb_bytes[i * 3] / 255.0f,
                rgb_bytes[i * 3 + 1] / 255.0f,
                rgb_bytes[i * 3 + 2] / 255.0f,
                1.0f));
        }
    }

    size_t nf = fs_faces.size() / 3;
    out.triangles.reserve(nf);
    for (size_t i = 0; i < nf; i++) {
        out.triangles.push_back(Triangle{
            fs_faces[i * 3],
            fs_faces[i * 3 + 1],
            fs_faces[i * 3 + 2]});
    }

    if (detect_transparency && out.has_colors()) {
        for (const auto &c : out.colors) {
            if (c.a < 1.0f - 1e-6f) {
                out.has_transparency = true;
                break;
            }
        }
    }

    return out;
}

} // namespace scimesh
