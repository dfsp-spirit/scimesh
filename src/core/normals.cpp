#include "normals.h"

namespace scimesh {

void compute_vertex_normals(const Mesh &mesh, std::vector<Vec3> &normals) {
    normals.assign(mesh.vertices.size(), Vec3(0.0f));

    for (const auto &tri : mesh.triangles) {
        Vec3 face_normal = compute_face_normal(
            mesh.vertices[tri.v0],
            mesh.vertices[tri.v1],
            mesh.vertices[tri.v2]);

        normals[tri.v0] += face_normal;
        normals[tri.v1] += face_normal;
        normals[tri.v2] += face_normal;
    }

    for (auto &n : normals) {
        float len = glm::length(n);
        if (len > 1e-12f)
            n /= len;
        else
            n = Vec3(0.0f, 0.0f, 1.0f);
    }
}

} // namespace scimesh
