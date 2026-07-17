#include "catch_amalgamated.hpp"
#include "mesh.h"
#include "scene.h"

using namespace scimesh;
using Catch::Approx;

static Mesh make_unit_cube() {
    Mesh mesh;
    mesh.vertices = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, // back face
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}   // front face
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

TEST_CASE("Mesh has_colors returns false for empty colors", "[mesh]") {
    Mesh mesh = make_unit_cube();
    REQUIRE_FALSE(mesh.has_colors());
}

TEST_CASE("Mesh has_colors returns true when colors present", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.colors.resize(8, Color(1.0f, 0.0f, 0.0f));
    REQUIRE(mesh.has_colors());
}

TEST_CASE("Mesh is_valid returns true for valid mesh", "[mesh]") {
    Mesh mesh = make_unit_cube();
    REQUIRE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns false for out-of-range indices", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.triangles[0].v0 = 100;
    REQUIRE_FALSE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns false for NaN coordinates", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.vertices[0].x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns false for mismatched color count", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.colors.resize(5); // 5 colors for 8 vertices — mismatch
    REQUIRE_FALSE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns false for mismatched normal count", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.normals.resize(3); // 3 normals for 8 vertices — mismatch
    REQUIRE_FALSE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns false for mismatched face_color count", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.face_colors.resize(5); // 5 face colors for 12 triangles — mismatch
    REQUIRE_FALSE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns false for mismatched uv count", "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.uvs.resize(7); // 7 UVs for 8 vertices — mismatch
    REQUIRE_FALSE(mesh.is_valid());
}

TEST_CASE("Mesh is_valid returns true with correctly-sized optional arrays",
          "[mesh]") {
    Mesh mesh = make_unit_cube();
    mesh.colors.resize(8);
    mesh.normals.resize(8);
    mesh.uvs.resize(8);
    mesh.face_colors.resize(12); // one per triangle
    REQUIRE(mesh.is_valid());
}

TEST_CASE("Mesh empty returns true for no vertices", "[mesh]") {
    Mesh mesh;
    REQUIRE(mesh.empty());
}

TEST_CASE("Mesh empty returns false for valid mesh", "[mesh]") {
    Mesh mesh = make_unit_cube();
    REQUIRE_FALSE(mesh.empty());
}

TEST_CASE("Mesh compute_bounding_box returns correct bounds for cube", "[mesh]") {
    Mesh mesh = make_unit_cube();
    Vec3 min_b, max_b;
    mesh.compute_bounding_box(min_b, max_b);
    REQUIRE(min_b.x == Approx(-1.0f));
    REQUIRE(min_b.y == Approx(-1.0f));
    REQUIRE(min_b.z == Approx(-1.0f));
    REQUIRE(max_b.x == Approx(1.0f));
    REQUIRE(max_b.y == Approx(1.0f));
    REQUIRE(max_b.z == Approx(1.0f));
}

TEST_CASE("Scene compute_bounding_box combines all meshes", "[scene]") {
    Scene scene;
    Mesh mesh1 = make_unit_cube();
    Mesh mesh2 = make_unit_cube();
    for (auto &v : mesh2.vertices)
        v += Vec3(10.0f, 0.0f, 0.0f);
    scene.meshes = {mesh1, mesh2};

    Vec3 min_b, max_b;
    scene.compute_bounding_box(min_b, max_b);
    REQUIRE(min_b.x == Approx(-1.0f));
    REQUIRE(max_b.x == Approx(11.0f));
}
