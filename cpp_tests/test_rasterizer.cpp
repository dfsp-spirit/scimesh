#include "catch_amalgamated.hpp"
#include "rasterizer.h"
#include "image.h"
#include "math_utils.h"

using namespace scimesh;
using Catch::Approx;

TEST_CASE("Rasterizer initializes with correct dimensions and z-buffer", "[rasterizer]") {
    Rasterizer r(100, 50);
    REQUIRE(r.width == 100);
    REQUIRE(r.height == 50);
    REQUIRE(r.z_buffer.size() == 100 * 50);
}

TEST_CASE("Rasterizer clear sets z-buffer to given depth", "[rasterizer]") {
    Rasterizer r(10, 10);
    r.clear(0.5f);
    for (float d : r.z_buffer) {
        REQUIRE(d == Approx(0.5f));
    }
}

TEST_CASE("Rasterizer renders a single visible triangle", "[rasterizer]") {
    Image img(100, 100);
    img.clear((uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)255);
    Rasterizer r(100, 100);
    r.clear(1.0f);

    // Front-facing winding: CCW visually in Y-down screen → negative signed area.
    Vec3 v0(30, 30, 0.5f), v1(50, 70, 0.5f), v2(70, 30, 0.5f);
    Color c(1, 0, 0, 1);
    Vec3 n(0, 0, -1);
    Vec3 light(0, 0, -1);

    r.rasterize_triangle(v0, c, n, v1, c, n, v2, c, n,
                         true, true, light, false, Color(), img);

    // Center of triangle should be red
    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 50, rr, gg, bb, aa);
    REQUIRE(rr > 0);
    REQUIRE(gg == 0);
    REQUIRE(bb == 0);
}

TEST_CASE("Rasterizer Z-buffer occludes farther triangle", "[rasterizer]") {
    Image img(50, 50);
    img.clear((uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)255);
    Rasterizer r(50, 50);
    r.clear(1.0f);

    // Front-facing winding (negative signed area in screen Y-down)
    Vec3 v0(10, 10, 0.8f), v1(25, 40, 0.8f), v2(40, 10, 0.8f);
    Vec3 v3(10, 10, 0.2f), v4(25, 40, 0.2f), v5(40, 10, 0.2f);
    Color red(1, 0, 0, 1);
    Color green(0, 1, 0, 1);
    Vec3 n(0, 0, -1);
    Vec3 light(0, 0, -1);

    // Render far triangle first (depth 0.8), then near triangle (depth 0.2)
    r.rasterize_triangle(v0, red, n, v1, red, n, v2, red, n,
                         true, true, light, false, Color(), img);
    r.rasterize_triangle(v3, green, n, v4, green, n, v5, green, n,
                         true, true, light, false, Color(), img);

    // Center should be green (nearer triangle wins)
    uint8_t rr, gg, bb, aa;
    img.get_pixel(25, 25, rr, gg, bb, aa);
    REQUIRE(gg > 0);
    REQUIRE(rr == 0);
}

TEST_CASE("Rasterizer backface culling discards back-facing triangle", "[rasterizer]") {
    Image img(100, 100);
    img.clear((uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)255);
    Rasterizer r(100, 100);
    r.clear(1.0f);

    // Back-facing winding: CW visually in Y-down screen → positive signed area → culled.
    Vec3 v0(30, 30, 0.5f), v1(70, 30, 0.5f), v2(50, 70, 0.5f);
    Color c(1, 0, 0, 1);
    Vec3 n(0, 0, -1);
    Vec3 light(0, 0, -1);

    r.rasterize_triangle(v0, c, n, v1, c, n, v2, c, n,
                         true, true, light, false, Color(), img);

    // Should be culled, center should remain black
    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 50, rr, gg, bb, aa);
    REQUIRE(rr == 0);
}

TEST_CASE("Rasterizer without backface culling renders both sides", "[rasterizer]") {
    Image img(100, 100);
    img.clear((uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)255);
    Rasterizer r(100, 100);
    r.clear(1.0f);

    // Back-facing winding (positive signed area), but culling disabled.
    Vec3 v0(30, 30, 0.5f), v1(70, 30, 0.5f), v2(50, 70, 0.5f);
    Color c(1, 0, 0, 1);
    Vec3 n(0, 0, -1);
    Vec3 light(0, 0, -1);

    r.rasterize_triangle(v0, c, n, v1, c, n, v2, c, n,
                         false, true, light, false, Color(), img);

    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 50, rr, gg, bb, aa);
    REQUIRE(rr > 0);
}
