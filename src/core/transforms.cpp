#include <scimesh/transforms.h>
#include <scimesh/math_utils.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace scimesh {

namespace {

/// @brief Transform vertex normals with the inverse transpose of `matrix`.
///
/// The inverse transpose is what keeps normals perpendicular to the surface
/// under non-uniform scaling, shearing and mirroring (for pure rotations it is
/// the rotation itself, so those are unaffected).  The result is normalized, so
/// normals stay unit-length whatever the caller does to the geometry.
///
/// For singular matrices (e.g. a scale of 0, or a flattened mesh) the inverse
/// does not exist; the plain 3x3 transform is used as a best effort.
/// Meshes without normals are left alone.
void transform_normals(Mesh &mesh, const Mat4 &matrix) {
    if (!mesh.has_normals()) {
        return;
    }

    glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(matrix)));
    bool usable = true;
    for (int col = 0; col < 3 && usable; ++col) {
        for (int row = 0; row < 3 && usable; ++row) {
            if (!std::isfinite(normal_matrix[col][row])) {
                usable = false;
            }
        }
    }
    if (!usable) {
        normal_matrix = glm::mat3(matrix);
    }

    for (auto &n : mesh.normals) {
        Vec3 transformed = normal_matrix * n;
        float len = glm::length(transformed);
        if (len > 1e-12f) {
            n = transformed / len;
        }
    }
}

} // anonymous namespace

void translate_mesh(Mesh &mesh, const Vec3 &translation) {
    Mat4 m = glm::translate(Mat4(1.0f), translation);
    for (auto &v : mesh.vertices) {
        v = transform_point(m, v);
    }
    // Translation does not change directions, so normals stay valid as they are.
}

void scale_mesh(Mesh &mesh, const Vec3 &scale) {
    Mat4 m = glm::scale(Mat4(1.0f), scale);
    for (auto &v : mesh.vertices) {
        v = transform_point(m, v);
    }
    transform_normals(mesh, m);
}

void scale_mesh(Mesh &mesh, float uniform_scale) {
    scale_mesh(mesh, Vec3(uniform_scale));
}

void rotate_mesh(Mesh &mesh, float angle_radians, const Vec3 &axis) {
    Mat4 m = glm::rotate(Mat4(1.0f), angle_radians, axis);
    for (auto &v : mesh.vertices) {
        v = transform_point(m, v);
    }
    transform_normals(mesh, m);
}

void transform_mesh(Mesh &mesh, const Mat4 &matrix) {
    for (auto &v : mesh.vertices) {
        v = transform_point(matrix, v);
    }
    transform_normals(mesh, matrix);
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
            // NaN marks "no data" (e.g. the medial wall in FreeSurfer brain
            // surfaces).  With detect_transparency such vertices become holes
            // (alpha 0) instead of opaque white ones - the renderer then shows
            // whatever is behind them.
            out.colors.push_back(Color(1.0f, 1.0f, 1.0f,
                                       detect_transparency ? 0.0f : 1.0f));
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

    // Transparent vertices (NaN per-vertex values with detect_transparency) need
    // the renderer's blended pass.
    out.update_transparency();

    return out;
}

} // namespace scimesh
