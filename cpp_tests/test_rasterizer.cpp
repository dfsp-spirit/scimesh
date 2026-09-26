#include "catch_amalgamated.hpp"
#include <scimesh/rasterizer.h>
#include <scimesh/image.h>
#include <scimesh/math_utils.h>

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

    r.rasterize_triangle(v0, c, n, Vec2(0,0), v1, c, n, Vec2(0,0), v2, c, n, Vec2(0,0),
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
    r.rasterize_triangle(v0, red, n, Vec2(0,0), v1, red, n, Vec2(0,0), v2, red, n, Vec2(0,0),
                         true, true, light, false, Color(), img);
    r.rasterize_triangle(v3, green, n, Vec2(0,0), v4, green, n, Vec2(0,0), v5, green, n, Vec2(0,0),
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

    r.rasterize_triangle(v0, c, n, Vec2(0,0), v1, c, n, Vec2(0,0), v2, c, n, Vec2(0,0),
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

    r.rasterize_triangle(v0, c, n, Vec2(0,0), v1, c, n, Vec2(0,0), v2, c, n, Vec2(0,0),
                         false, true, light, false, Color(), img);

    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 50, rr, gg, bb, aa);
    REQUIRE(rr > 0);
}

TEST_CASE("A pixel on a shared triangle edge is rasterized by one triangle",
          "[rasterizer]") {
    // Two translucent triangles forming a quad whose shared diagonal lies
    // exactly on the centers of the pixels it crosses: (10, 10) to (50, 50) is
    // the line y = x, and pixel centers are at (x + 0.5, y + 0.5).  Every pixel
    // of the quad must be blended exactly once, including the ones on the
    // diagonal.  Evaluating the edge function in the direction each triangle
    // happens to walk along the shared edge gives the two triangles values that
    // differ in their last bits, so on some platforms such a pixel used to be
    // covered by *both* triangles (blended twice - a dark seam) and on others by
    // neither (a crack).
    Image img(64, 64);
    img.clear(255, 255, 255, 255);
    Rasterizer r(64, 64);
    r.clear(1.0f);
    r.set_blend_mode(true);

    const Color half_red(1, 0, 0, 0.5f);
    const Vec3 n(0, 0, 1);
    const Vec3 light(0, 0, 1);
    auto triangle = [&](const Vec3 &a, const Vec3 &b, const Vec3 &c) {
        r.rasterize_triangle(a, half_red, n, Vec2(0, 0),
                             b, half_red, n, Vec2(0, 0),
                             c, half_red, n, Vec2(0, 0),
                             false, false, light, false, Color(), img);
    };
    triangle(Vec3(10, 10, 0.5f), Vec3(50, 10, 0.5f), Vec3(50, 50, 0.5f));
    triangle(Vec3(10, 10, 0.5f), Vec3(50, 50, 0.5f), Vec3(10, 50, 0.5f));

    uint8_t rr, gg, bb, aa;
    img.get_pixel(20, 30, rr, gg, bb, aa);   // inside the lower right half
    const uint8_t lower = gg;
    img.get_pixel(30, 20, rr, gg, bb, aa);   // inside the upper left half
    const uint8_t upper = gg;
    img.get_pixel(30, 30, rr, gg, bb, aa);   // exactly on the shared diagonal
    const uint8_t on_edge = gg;

    REQUIRE(upper == lower);
    REQUIRE(on_edge == lower);   // exactly one blend, not two and not zero
    REQUIRE(on_edge > 0);
    REQUIRE(on_edge < 255);
}
