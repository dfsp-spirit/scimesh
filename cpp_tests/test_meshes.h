#pragma once

// Shared test mesh generators for the scimesh C++ test suite.
// Provides procedurally-generated meshes for unit and integration tests.

#include "mesh.h"
#include <cmath>

namespace scimesh_test {

using scimesh::Mesh;
using scimesh::Vec3;
using scimesh::Color;
using scimesh::Triangle;

// Unit cube: 8 vertices, 12 triangles, centered at origin, side length 2.
inline Mesh make_unit_cube() {
    Mesh mesh;
    mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    };
    mesh.triangles = {
        {0, 3, 2}, {0, 2, 1}, // back (z=-1)
        {4, 5, 6}, {4, 6, 7}, // front (z=+1)
        {0, 1, 5}, {0, 5, 4}, // bottom (y=-1)
        {2, 3, 7}, {2, 7, 6}, // top (y=+1)
        {0, 4, 7}, {0, 7, 3}, // left (x=-1)
        {1, 2, 6}, {1, 6, 5}  // right (x=+1)
    };
    return mesh;
}

// Colored unit cube: each vertex has a distinct color.
inline Mesh make_colored_cube() {
    Mesh mesh = make_unit_cube();
    mesh.colors = {
        {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1},
        {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}, {0.5f, 0.5f, 0.5f, 1}
    };
    return mesh;
}

// Tetrahedron: 4 vertices, 4 triangles.
inline Mesh make_tetrahedron() {
    Mesh mesh;
    mesh.vertices = {
        {0, 1, 0},
        {-0.866f, -0.5f, 0.5f},
        {0.866f, -0.5f, 0.5f},
        {0, -0.5f, -1.0f}
    };
    mesh.triangles = {
        {0, 2, 1},
        {0, 1, 3},
        {0, 3, 2},
        {1, 2, 3}
    };
    return mesh;
}

// Simple plane: 4 vertices, 2 triangles, in the XY plane at z=0.
// Spans [-1,-1] to [1,1]. Normal points +Z.
inline Mesh make_plane() {
    Mesh mesh;
    mesh.vertices = {
        {-1, -1, 0},
        {1, -1, 0},
        {1, 1, 0},
        {-1, 1, 0}
    };
    mesh.triangles = {
        {0, 1, 2},
        {0, 2, 3}
    };
    return mesh;
}

// Icosphere: a sphere built by subdividing an icosahedron.
// `subdivisions` controls detail level (0 = 12 vertices/20 triangles).
// Radius is 1.0, centered at origin. Useful for testing smooth shading.
inline Mesh make_icosphere(int subdivisions = 1) {
    Mesh mesh;

    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    // 12 vertices of the icosahedron, then normalized to unit sphere
    std::vector<Vec3> verts = {
        {-1,  t, 0}, {1,  t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1,  t}, {0, 1,  t}, {0, -1, -t}, {0, 1, -t},
        { t, 0, -1}, { t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}
    };
    for (auto &v : verts)
        v = glm::normalize(v);

    // 20 base faces of the icosahedron
    std::vector<Triangle> faces = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    mesh.vertices = verts;
    mesh.triangles = faces;

    // Subdivide
    for (int s = 0; s < subdivisions; ++s) {
        std::vector<Triangle> new_faces;
        new_faces.reserve(mesh.triangles.size() * 4);

        for (const auto &tri : mesh.triangles) {
            uint32_t a = tri.v0, b = tri.v1, c = tri.v2;
            // Compute midpoints, normalized to sphere
            Vec3 mab = glm::normalize((mesh.vertices[a] + mesh.vertices[b]) * 0.5f);
            Vec3 mbc = glm::normalize((mesh.vertices[b] + mesh.vertices[c]) * 0.5f);
            Vec3 mca = glm::normalize((mesh.vertices[c] + mesh.vertices[a]) * 0.5f);

            uint32_t iab = static_cast<uint32_t>(mesh.vertices.size());
            uint32_t ibc = iab + 1;
            uint32_t ica = iab + 2;
            mesh.vertices.push_back(mab);
            mesh.vertices.push_back(mbc);
            mesh.vertices.push_back(mca);

            new_faces.push_back({a, iab, ica});
            new_faces.push_back({b, ibc, iab});
            new_faces.push_back({c, ica, ibc});
            new_faces.push_back({iab, ibc, ica});
        }
        mesh.triangles = std::move(new_faces);
    }

    return mesh;
}

} // namespace scimesh_test