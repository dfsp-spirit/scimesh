#include "catch_amalgamated.hpp"
#include "math_utils.h"

using namespace scimesh;
using Catch::Approx;

TEST_CASE("compute_face_normal returns correct normal for CCW triangle", "[math]") {
    Vec3 v0(0, 0, 0), v1(1, 0, 0), v2(0, 1, 0);
    Vec3 n = compute_face_normal(v0, v1, v2);
    REQUIRE(n.x == Approx(0.0f).margin(1e-6f));
    REQUIRE(n.y == Approx(0.0f).margin(1e-6f));
    REQUIRE(n.z == Approx(1.0f).margin(1e-6f));
}

TEST_CASE("compute_face_normal returns flipped normal for CW triangle", "[math]") {
    Vec3 v0(0, 0, 0), v1(0, 1, 0), v2(1, 0, 0);
    Vec3 n = compute_face_normal(v0, v1, v2);
    REQUIRE(n.z == Approx(-1.0f).margin(1e-6f));
}

TEST_CASE("compute_face_normal handles degenerate triangle", "[math]") {
    Vec3 v0(0, 0, 0), v1(1, 0, 0), v2(2, 0, 0);
    Vec3 n = compute_face_normal(v0, v1, v2);
    REQUIRE(glm::length(n) == Approx(1.0f).margin(1e-6f));
}

TEST_CASE("perspective_divide converts clip to NDC", "[math]") {
    Vec4 clip(2.0f, 4.0f, 0.5f, 2.0f);
    Vec3 ndc = perspective_divide(clip);
    REQUIRE(ndc.x == Approx(1.0f));
    REQUIRE(ndc.y == Approx(2.0f));
    REQUIRE(ndc.z == Approx(0.25f));
}

TEST_CASE("perspective_divide handles zero w safely", "[math]") {
    Vec4 clip(1.0f, 2.0f, 3.0f, 0.0f);
    Vec3 ndc = perspective_divide(clip);
    REQUIRE(ndc.x == Approx(1.0f));
    REQUIRE(ndc.y == Approx(2.0f));
}

TEST_CASE("ndc_to_screen maps NDC to pixel coordinates", "[math]") {
    float sx, sy, depth;
    ndc_to_screen(Vec3(0.0f, 0.0f, 0.5f), 800, 600, sx, sy, depth);
    REQUIRE(sx == Approx(400.0f));
    REQUIRE(sy == Approx(300.0f));
    REQUIRE(depth == Approx(0.5f));
}

TEST_CASE("ndc_to_screen maps corners correctly", "[math]") {
    float sx, sy, depth;
    ndc_to_screen(Vec3(-1.0f, 1.0f, 0.0f), 100, 100, sx, sy, depth);
    REQUIRE(sx == Approx(0.0f));
    REQUIRE(sy == Approx(0.0f));
    ndc_to_screen(Vec3(1.0f, -1.0f, 1.0f), 100, 100, sx, sy, depth);
    REQUIRE(sx == Approx(100.0f));
    REQUIRE(sy == Approx(100.0f));
}

TEST_CASE("shade_pixel applies ambient + diffuse lighting", "[math]") {
    Color base(1.0f, 1.0f, 1.0f, 1.0f);
    Vec3 normal(0.0f, 0.0f, 1.0f);
    Vec3 light(0.0f, 0.0f, -1.0f);
    Color result = shade_pixel(base, normal, light);
    // Light points -Z, normal points +Z, so dot product is 0
    // Result should be ambient only (0.3)
    REQUIRE(result.r == Approx(0.3f).margin(0.01f));
}

TEST_CASE("shade_pixel with aligned normal and light gives full brightness", "[math]") {
    Color base(1.0f, 0.5f, 0.25f, 1.0f);
    Vec3 normal(0.0f, 0.0f, -1.0f);
    Vec3 light(0.0f, 0.0f, -1.0f);
    Color result = shade_pixel(base, normal, light);
    // Full diffuse + ambient = 1.0
    REQUIRE(result.r == Approx(1.0f).margin(0.01f));
    REQUIRE(result.g == Approx(0.5f).margin(0.01f));
    REQUIRE(result.b == Approx(0.25f).margin(0.01f));
}

TEST_CASE("compute_barycentric returns correct weights at vertices", "[math]") {
    float u, v, w;
    compute_barycentric(0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 10.0f, u, v, w);
    REQUIRE(u == Approx(1.0f).margin(1e-5f));
    REQUIRE(v == Approx(0.0f).margin(1e-5f));
    REQUIRE(w == Approx(0.0f).margin(1e-5f));

    compute_barycentric(10.0f, 0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 10.0f, u, v, w);
    REQUIRE(u == Approx(0.0f).margin(1e-5f));
    REQUIRE(v == Approx(1.0f).margin(1e-5f));

    compute_barycentric(0.0f, 10.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 10.0f, u, v, w);
    REQUIRE(u == Approx(0.0f).margin(1e-5f));
    REQUIRE(w == Approx(1.0f).margin(1e-5f));

    compute_barycentric(3.33f, 3.33f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 10.0f, u, v, w);
    REQUIRE(u + v + w == Approx(1.0f).margin(1e-5f));
}
