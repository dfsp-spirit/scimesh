#include "catch_amalgamated.hpp"
#include "normals.h"
#include "mesh.h"

using namespace scimesh;
using Catch::Approx;

static Mesh make_unit_cube() {
    Mesh mesh;
    mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    };
    mesh.triangles = {
        {0, 3, 2}, {0, 2, 1}, // back (z=-1, normal=-z)
        {4, 5, 6}, {4, 6, 7}, // front (z=+1, normal=+z)
        {0, 1, 5}, {0, 5, 4}, // bottom (y=-1, normal=-y)
        {2, 3, 7}, {2, 7, 6}, // top (y=+1, normal=+y)
        {0, 4, 7}, {0, 7, 3}, // left (x=-1, normal=-x)
        {1, 2, 6}, {1, 6, 5}  // right (x=+1, normal=+x)
    };
    return mesh;
}

TEST_CASE("compute_vertex_normals produces normalized vectors", "[normals]") {
    Mesh mesh = make_unit_cube();
    std::vector<Vec3> normals;
    compute_vertex_normals(mesh, normals);

    REQUIRE(normals.size() == mesh.vertices.size());
    for (const auto &n : normals) {
        REQUIRE(glm::length(n) == Approx(1.0f).margin(1e-5f));
    }
}

TEST_CASE("compute_vertex_normals for cube faces point outward", "[normals]") {
    Mesh mesh = make_unit_cube();
    std::vector<Vec3> normals;
    compute_vertex_normals(mesh, normals);

    // Vertex 6 (1, 1, 1) is a corner of front/top/right faces
    // Its averaged normal should point roughly in the (1,1,1) direction
    Vec3 n = normals[6];
    REQUIRE(n.x > 0.0f);
    REQUIRE(n.y > 0.0f);
    REQUIRE(n.z > 0.0f);
}

TEST_CASE("compute_vertex_normals handles empty mesh", "[normals]") {
    Mesh mesh;
    std::vector<Vec3> normals;
    compute_vertex_normals(mesh, normals);
    REQUIRE(normals.empty());
}

TEST_CASE("compute_vertex_normals with single triangle", "[normals]") {
    Mesh mesh;
    mesh.vertices = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh.triangles = {{0, 1, 2}};
    std::vector<Vec3> normals;
    compute_vertex_normals(mesh, normals);

    REQUIRE(normals.size() == 3);
    // All normals should point in +Z direction (CCW winding viewed from +Z)
    for (const auto &n : normals) {
        REQUIRE(n.z == Approx(1.0f).margin(1e-5f));
    }
}
