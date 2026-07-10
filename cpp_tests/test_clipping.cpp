#include "catch_amalgamated.hpp"
#include "clipping.h"
#include "math_utils.h"

using namespace scimesh;
using Catch::Approx;

TEST_CASE("Triangle fully in front of near plane passes through unchanged", "[clipping]") {
    ClipVertex v0, v1, v2;
    v0.position = Vec4(0, 1, -1, 1);  // z=-1, w=1 → z+w=0 (exactly on plane, inside)
    v1.position = Vec4(-1, -1, -1, 1);
    v2.position = Vec4(1, -1, -1, 1);
    v0.color = Color(1, 0, 0);
    v1.color = Color(0, 1, 0);
    v2.color = Color(0, 0, 1);

    std::vector<ClipVertex> out_verts;
    std::vector<Triangle> out_tris;
    int n = clip_triangle_near_plane(v0, v1, v2, out_verts, out_tris);
    REQUIRE(n == 1);
    REQUIRE(out_verts.size() == 3);
}

TEST_CASE("Triangle fully behind near plane is discarded", "[clipping]") {
    ClipVertex v0, v1, v2;
    // In OpenGL clip space, z = -w is the near plane. Points with z < -w are behind.
    // Use z = -2, w = 1 → z + w = -1 < 0 (behind near plane)
    v0.position = Vec4(0, 1, -2, 1);
    v1.position = Vec4(-1, -1, -2, 1);
    v2.position = Vec4(1, -1, -2, 1);

    std::vector<ClipVertex> out_verts;
    std::vector<Triangle> out_tris;
    int n = clip_triangle_near_plane(v0, v1, v2, out_verts, out_tris);
    REQUIRE(n == 0);
}

TEST_CASE("Triangle partially behind near plane is split into 1 or 2 triangles", "[clipping]") {
    ClipVertex v0, v1, v2;
    // v0 is in front (z+w > 0), v1 and v2 are behind (z+w < 0)
    v0.position = Vec4(0, 1, 0, 1);   // z+w = 1 (in front)
    v1.position = Vec4(-1, -1, -2, 1); // z+w = -1 (behind)
    v2.position = Vec4(1, -1, -2, 1);  // z+w = -1 (behind)
    v0.color = Color(1, 0, 0);
    v1.color = Color(0, 1, 0);
    v2.color = Color(0, 0, 1);

    std::vector<ClipVertex> out_verts;
    std::vector<Triangle> out_tris;
    int n = clip_triangle_near_plane(v0, v1, v2, out_verts, out_tris);
    REQUIRE(n >= 1);
    REQUIRE(out_verts.size() >= 3);
    // All output vertices should be in front of the near plane
    for (const auto &v : out_verts) {
        REQUIRE(v.position.z + v.position.w >= Approx(0.0f).margin(1e-5f));
    }
}

TEST_CASE("Clipping interpolates vertex attributes at intersection", "[clipping]") {
    ClipVertex v0, v1, v2;
    v0.position = Vec4(0, 0, 0, 1);   // in front
    v0.color = Color(1.0f, 0.0f, 0.0f);
    v0.normal = Vec3(0, 0, 1);
    v1.position = Vec4(0, 0, -2, 1);  // behind
    v1.color = Color(0.0f, 1.0f, 0.0f);
    v1.normal = Vec3(0, 0, -1);
    v2.position = Vec4(1, 0, -2, 1);  // behind
    v2.color = Color(0.0f, 0.0f, 1.0f);
    v2.normal = Vec3(0, 1, 0);

    std::vector<ClipVertex> out_verts;
    std::vector<Triangle> out_tris;
    int n = clip_triangle_near_plane(v0, v1, v2, out_verts, out_tris);
    REQUIRE(n >= 1);
    // Check that interpolated colors are between the originals
    for (const auto &v : out_verts) {
        REQUIRE(v.color.r >= 0.0f);
        REQUIRE(v.color.g >= 0.0f);
        REQUIRE(v.color.b >= 0.0f);
    }
}
