// Screen-space line primitives (LineLayer / Renderer::render_lines_raw).
//
// Line layers draw many independent segments with a width measured in *pixels*
// and no geometry at all.  They are part of a Scene and share the depth buffer
// with the meshes, so these tests cover rasterization (width, color, depth),
// occlusion against meshes, near-plane/user-plane clipping, and the blended
// pass for translucent lines.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/renderer.h>
#include <scimesh/rasterizer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/lines.h>
#include <scimesh/mesh.h>
#include <scimesh/image.h>

#include <cmath>
#include <vector>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using Catch::Approx;

namespace {

const Color kRed(1.0f, 0.0f, 0.0f, 1.0f);
const Color kBlue(0.0f, 0.0f, 1.0f, 1.0f);
const Color kGreen(0.0f, 1.0f, 0.0f, 1.0f);

/// Small image render options with a white background (the C++ default).
RenderOptions small_options(int size = 64) {
    RenderOptions opts;
    opts.width = size;
    opts.height = size;
    opts.background_color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    return opts;
}

/// A world-space clip plane that keeps the half space dot(normal, p) + offset >= 0.
ClipPlane make_clip_plane(const Vec3 &normal, float offset) {
    ClipPlane plane;
    plane.normal = normal;
    plane.offset = offset;
    plane.space = PlaneSpace::WORLD;
    return plane;
}

Camera front_camera(float distance = 4.0f) {
    Camera cam;
    cam.eye = Vec3(0, 0, distance);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);
    cam.fov_degrees = 45.0f;
    return cam;
}

/// Number of pixels of the image that satisfy the predicate.
template <typename Pred>
int count_pixels(const Image &img, Pred pred) {
    int count = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (pred(r, g, b, a)) {
                ++count;
            }
        }
    }
    return count;
}

/// Whether a pixel is (close to) the given color.
bool is_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
              const Color &expected, int tolerance = 12) {
    auto close_to = [tolerance](uint8_t v, float e) {
        return std::abs(static_cast<int>(v) - static_cast<int>(e * 255.0f)) <= tolerance;
    };
    (void)a;
    return close_to(r, expected.r) && close_to(g, expected.g) &&
           close_to(b, expected.b);
}

/// Rasterizer with a black background, ready for direct rasterize_*() calls.
Rasterizer make_rasterizer(int w, int h, Image &img) {
    img.clear(0, 0, 0, 255);
    Rasterizer r(w, h);
    r.clear(1.0f);
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
//  Rasterizer::rasterize_line
// ---------------------------------------------------------------------------

TEST_CASE("rasterize_line draws a horizontal line", "[lines][rasterizer]") {
    Image img(100, 100);
    Rasterizer r = make_rasterizer(100, 100, img);

    const Vec3 p0(10.0f, 50.0f, 0.5f);
    const Vec3 p1(90.0f, 50.0f, 0.5f);
    r.rasterize_line(p0, kRed, p1, kRed, 1.0f, false, Vec3(0, 0, 1),
                     Vec3(0, 0, 1), img);

    // The line covers exactly the row it is on.
    for (int x = 10; x <= 90; ++x) {
        uint8_t rr, gg, bb, aa;
        img.get_pixel(x, 50, rr, gg, bb, aa);
        REQUIRE(is_color(rr, gg, bb, aa, kRed));
    }

    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 49, rr, gg, bb, aa);
    REQUIRE_FALSE(is_color(rr, gg, bb, aa, kRed));
    img.get_pixel(50, 51, rr, gg, bb, aa);
    REQUIRE_FALSE(is_color(rr, gg, bb, aa, kRed));
}

TEST_CASE("rasterize_line draws diagonal and vertical lines",
          "[lines][rasterizer]") {
    Image img(100, 100);
    Rasterizer r = make_rasterizer(100, 100, img);

    // 45 degree diagonal: every step stays connected (no gaps).
    r.rasterize_line(Vec3(10, 10, 0.5f), kRed, Vec3(80, 80, 0.5f), kRed, 1.0f,
                     false, Vec3(0, 0, 1), Vec3(0, 0, 1), img);
    for (int i = 0; i <= 70; ++i) {
        uint8_t rr, gg, bb, aa;
        img.get_pixel(10 + i, 10 + i, rr, gg, bb, aa);
        REQUIRE(is_color(rr, gg, bb, aa, kRed));
    }

    // Vertical line on a fresh image.
    Image img2(100, 100);
    Rasterizer r2 = make_rasterizer(100, 100, img2);
    r2.rasterize_line(Vec3(25, 5, 0.5f), kGreen, Vec3(25, 95, 0.5f), kGreen,
                      1.0f, false, Vec3(0, 0, 1), Vec3(0, 0, 1), img2);
    for (int y = 5; y <= 95; ++y) {
        uint8_t rr, gg, bb, aa;
        img2.get_pixel(25, y, rr, gg, bb, aa);
        REQUIRE(is_color(rr, gg, bb, aa, kGreen));
    }
}

TEST_CASE("rasterize_line width controls the number of covered rows",
          "[lines][rasterizer]") {
    for (float width : {1.0f, 3.0f, 5.0f}) {
        Image img(100, 100);
        Rasterizer r = make_rasterizer(100, 100, img);
        r.rasterize_line(Vec3(10, 50, 0.5f), kRed, Vec3(90, 50, 0.5f), kRed,
                         width, false, Vec3(0, 0, 1), Vec3(0, 0, 1), img);

        const int expected_rows = static_cast<int>(width);
        int lit_rows = 0;
        for (int y = 0; y < 100; ++y) {
            uint8_t rr, gg, bb, aa;
            img.get_pixel(50, y, rr, gg, bb, aa);
            if (is_color(rr, gg, bb, aa, kRed)) {
                ++lit_rows;
            }
        }
        REQUIRE(lit_rows == expected_rows);
    }
}

TEST_CASE("rasterize_line applies the depth test", "[lines][rasterizer]") {
    Image img(100, 100);
    Rasterizer r = make_rasterizer(100, 100, img);

    // Opaque triangle at depth 0.2 covering the middle of the image.
    const Vec3 v0(10, 10, 0.2f), v1(90, 50, 0.2f), v2(10, 90, 0.2f);
    const Color gray(0.5f, 0.5f, 0.5f, 1.0f);
    r.rasterize_triangle(v0, gray, Vec3(0, 0, 1), Vec2(0, 0),
                         v1, gray, Vec3(0, 0, 1), Vec2(0, 0),
                         v2, gray, Vec3(0, 0, 1), Vec2(0, 0),
                         false, false, Vec3(0, 0, 1), false, Color(), img);

    // A line behind the triangle must not appear.
    r.rasterize_line(Vec3(20, 40, 0.6f), kRed, Vec3(80, 40, 0.6f), kRed, 1.0f,
                     false, Vec3(0, 0, 1), Vec3(0, 0, 1), img);
    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 40, rr, gg, bb, aa);
    REQUIRE_FALSE(is_color(rr, gg, bb, aa, kRed));

    // A line in front of the triangle is drawn.
    r.rasterize_line(Vec3(20, 60, 0.1f), kRed, Vec3(80, 60, 0.1f), kRed, 1.0f,
                     false, Vec3(0, 0, 1), Vec3(0, 0, 1), img);
    img.get_pixel(50, 60, rr, gg, bb, aa);
    REQUIRE(is_color(rr, gg, bb, aa, kRed));
}

TEST_CASE("rasterize_line interpolates colors along the segment",
          "[lines][rasterizer]") {
    Image img(100, 100);
    Rasterizer r = make_rasterizer(100, 100, img);

    // Red at the left end, blue at the right end.
    r.rasterize_line(Vec3(10, 50, 0.5f), kRed, Vec3(90, 50, 0.5f), kBlue, 1.0f,
                     false, Vec3(0, 0, 1), Vec3(0, 0, 1), img);

    uint8_t lr, lg, lb, la, mr, mg, mb, ma;
    img.get_pixel(12, 50, lr, lg, lb, la);
    img.get_pixel(50, 50, mr, mg, mb, ma);
    REQUIRE(lr > 200);              // near the start: mostly red
    REQUIRE(lb < 60);
    REQUIRE(mr > 80);               // middle: a mix of both
    REQUIRE(mb > 80);
}

TEST_CASE("rasterize_line blends translucent colors", "[lines][rasterizer]") {
    Image img(100, 100);
    img.clear(0, 0, 255, 255);      // blue background
    Rasterizer r(100, 100);
    r.clear(1.0f);
    r.set_blend_mode(true);

    const Color half_red(1.0f, 0.0f, 0.0f, 0.5f);
    r.rasterize_line(Vec3(10, 50, 0.5f), half_red, Vec3(90, 50, 0.5f),
                     half_red, 1.0f, false, Vec3(0, 0, 1), Vec3(0, 0, 1), img);
    r.set_blend_mode(false);

    uint8_t rr, gg, bb, aa;
    img.get_pixel(50, 50, rr, gg, bb, aa);
    REQUIRE(rr == Approx(127).margin(6));   // half red over blue
    REQUIRE(bb == Approx(127).margin(6));
    REQUIRE(gg == 0);
}

// ---------------------------------------------------------------------------
//  Renderer::render_lines_raw
// ---------------------------------------------------------------------------

TEST_CASE("render_lines_raw renders segments", "[lines][renderer]") {
    RenderOptions opts = small_options(64);
    opts.backface_culling = false;

    std::vector<Vec3> from = {Vec3(-2, 0, 0)};
    std::vector<Vec3> to = {Vec3(2, 0, 0)};
    std::vector<Color> colors = {kRed};

    Renderer renderer;
    Image img = renderer.render_lines_raw(from, to, colors, 2.0f,
                                          front_camera(), opts);
    REQUIRE(img.width == 64);
    REQUIRE(img.height == 64);

    const int lit = count_pixels(img, [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return is_color(r, g, b, a, kRed);
    });
    REQUIRE(lit > 20);      // a 2-pixel wide, ~60-pixel long line
}

TEST_CASE("render_lines_raw handles empty input", "[lines][renderer]") {
    RenderOptions opts = small_options(32);

    Renderer renderer;
    Image img = renderer.render_lines_raw({}, {}, {}, 1.0f, front_camera(), opts);
    REQUIRE(img.width == 32);
    // Nothing drawn: the image is the background color everywhere.
    REQUIRE(count_pixels(img, [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return !is_color(r, g, b, a, Color(1, 1, 1, 1));
    }) == 0);
}

// ---------------------------------------------------------------------------
//  Line layers in a scene
// ---------------------------------------------------------------------------

TEST_CASE("line layers are drawn together with meshes", "[lines][scene]") {
    RenderOptions opts = small_options(64);

    Scene scene;
    Mesh cube = make_unit_cube();
    cube.colors.assign(cube.vertices.size(), Color(0.5f, 0.5f, 0.5f, 1.0f));
    scene.add(cube);

    // A line in front of the cube (the cube's front face is at z = 1).
    LineLayer front;
    front.from = {Vec3(-0.8f, 0.0f, 1.5f)};
    front.to = {Vec3(0.8f, 0.0f, 1.5f)};
    front.colors = {kRed};
    front.width = 2.0f;
    scene.add_lines(front);

    // A line behind the front face: it must be hidden by the cube.
    LineLayer behind;
    behind.from = {Vec3(-0.8f, 0.5f, 0.0f)};
    behind.to = {Vec3(0.8f, 0.5f, 0.0f)};
    behind.colors = {kGreen};
    behind.width = 2.0f;
    scene.add_lines(behind);

    REQUIRE(scene.line_count() == 2);
    REQUIRE(scene.size() == 1);

    Renderer renderer;
    Image img = renderer.render_scene(scene, front_camera(), opts);

    const int red = count_pixels(img, [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return is_color(r, g, b, a, kRed);
    });
    const int green = count_pixels(img, [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return is_color(r, g, b, a, kGreen);
    });

    REQUIRE(red > 0);       // the front line is visible ...
    REQUIRE(green == 0);    // ... and the one behind the cube is not.
}

TEST_CASE("translucent line layers do not show through opaque meshes",
          "[lines][scene][transparency]") {
    RenderOptions opts = small_options(64);

    Scene scene;
    Mesh cube = make_unit_cube();
    cube.colors.assign(cube.vertices.size(), Color(0.5f, 0.5f, 0.5f, 1.0f));
    scene.add(cube);

    LineLayer behind;
    behind.from = {Vec3(-0.8f, 0.0f, 0.0f)};
    behind.to = {Vec3(0.8f, 0.0f, 0.0f)};
    behind.colors = {Color(1.0f, 0.0f, 0.0f, 0.5f)};
    behind.width = 3.0f;
    scene.add_lines(behind);

    Renderer renderer;
    Image img = renderer.render_scene(scene, front_camera(), opts);

    // The line is translucent *and* behind opaque geometry: it must not be
    // blended on top of the cube (the same rule that applies to translucent
    // triangles, see test_transparency.cpp).
    const int reddish = count_pixels(img, [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return r > 150 && g < 100 && b < 100;
    });
    REQUIRE(reddish == 0);
}

TEST_CASE("line layers are clipped at the near plane", "[lines][scene]") {
    RenderOptions opts = small_options(64);

    // The camera is at z = 4, so this segment starts in front of it (z = 3) and
    // ends behind it (z = 5).  Without clipping, the endpoint behind the camera
    // (w <= 0) would project to garbage.
    Scene scene;
    LineLayer crossing;
    crossing.from = {Vec3(0, 0, 3.0f)};
    crossing.to = {Vec3(0, 0, 5.0f)};
    crossing.colors = {kRed};
    crossing.width = 2.0f;
    scene.add_lines(crossing);

    Renderer renderer;
    Image img = renderer.render_scene(scene, front_camera(4.0f), opts);

    // The visible part (from the near plane to z = 3) is drawn ...
    const int red = count_pixels(img, [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return is_color(r, g, b, a, kRed);
    });
    REQUIRE(red > 0);

    // ... and it stays a short vertical blob around the center, i.e. the
    // garbage geometry is not smeared over the whole image.
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (is_color(r, g, b, a, kRed)) {
                REQUIRE(std::abs(x - img.width / 2) <= 4);
            }
        }
    }
}

TEST_CASE("user clip planes clip line layers", "[lines][scene][clipping]") {
    RenderOptions opts = small_options(64);

    Scene scene;
    LineLayer horizontal;
    horizontal.from = {Vec3(-2.0f, 0.0f, 0.0f)};
    horizontal.to = {Vec3(2.0f, 0.0f, 0.0f)};
    horizontal.colors = {kRed};
    horizontal.width = 3.0f;
    scene.add_lines(horizontal);

    // Keep only x >= 0 (world space): the left half of the line disappears.
    opts.clip_planes.push_back(make_clip_plane(Vec3(1, 0, 0), 0.0f));

    Renderer renderer;
    const Image img = renderer.render_scene(scene, front_camera(), opts);

    int left_half = 0;
    int right_half = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (is_color(r, g, b, a, kRed)) {
                if (x < img.width / 2) {
                    ++left_half;
                } else {
                    ++right_half;
                }
            }
        }
    }
    REQUIRE(right_half > 0);
    // The stamp of the sample that sits exactly on the clip plane may reach
    // about half a line width to the left of it (3 pixels wide line).
    REQUIRE(left_half <= 4);
    REQUIRE(right_half > 10 * left_half);
}

TEST_CASE("line layers contribute to the scene bounding box by default",
          "[lines][scene]") {
    Scene scene;
    Mesh cube = make_unit_cube();
    scene.add(cube);

    Vec3 min_a, max_a;
    scene.compute_bounding_box(min_a, max_a);

    LineLayer far_away;
    far_away.from = {Vec3(-100, -100, -100)};
    far_away.to = {Vec3(100, 100, 100)};
    far_away.colors = {kRed};
    REQUIRE(far_away.affects_bounds);      // the default
    scene.add_lines(far_away);

    Vec3 min_b, max_b;
    scene.compute_bounding_box(min_b, max_b);

    // The lines are content, so the box grows to cover them.
    REQUIRE(min_b.x == Approx(-100.0f));
    REQUIRE(min_b.y == Approx(-100.0f));
    REQUIRE(min_b.z == Approx(-100.0f));
    REQUIRE(max_b.x == Approx(100.0f));
    REQUIRE(max_b.y == Approx(100.0f));
    REQUIRE(max_b.z == Approx(100.0f));
    REQUIRE(min_a.x > min_b.x);
}

TEST_CASE("line layers can opt out of the scene bounding box", "[lines][scene]") {
    Scene scene;
    Mesh cube = make_unit_cube();
    scene.add(cube);

    Vec3 min_a, max_a;
    scene.compute_bounding_box(min_a, max_a);

    LineLayer far_away;                    // a leader line, an axis cross, ...
    far_away.from = {Vec3(-100, -100, -100)};
    far_away.to = {Vec3(100, 100, 100)};
    far_away.colors = {kRed};
    far_away.affects_bounds = false;
    scene.add_lines(far_away);

    Vec3 min_b, max_b;
    scene.compute_bounding_box(min_b, max_b);

    // Decorational lines never move the camera away from the data.
    REQUIRE(min_b.x == Approx(min_a.x));
    REQUIRE(min_b.y == Approx(min_a.y));
    REQUIRE(min_b.z == Approx(min_a.z));
    REQUIRE(max_b.x == Approx(max_a.x));
    REQUIRE(max_b.y == Approx(max_a.y));
    REQUIRE(max_b.z == Approx(max_a.z));
}

TEST_CASE("a scene without meshes is framed by its line layers", "[lines][scene]") {
    LineLayer layer;
    layer.from = {Vec3(-1, 0, 0), Vec3(0, -2, 0)};
    layer.to = {Vec3(1, 0, 0), Vec3(0, 2, 0)};
    layer.colors = {kRed, kBlue};
    layer.affects_bounds = false;          // even a decorational layer is used here

    Scene scene;
    scene.add_lines(layer);

    Vec3 min_b, max_b;
    scene.compute_bounding_box(min_b, max_b);

    REQUIRE(min_b.x == Approx(-1.0f));
    REQUIRE(min_b.y == Approx(-2.0f));
    REQUIRE(min_b.z == Approx(0.0f));
    REQUIRE(max_b.x == Approx(1.0f));
    REQUIRE(max_b.y == Approx(2.0f));
    REQUIRE(max_b.z == Approx(0.0f));
}

TEST_CASE("line layer bounds respect the placement transform", "[lines][scene]") {
    LineLayer layer;
    layer.from = {Vec3(0, 0, 0)};
    layer.to = {Vec3(1, 0, 0)};
    layer.colors = {kRed};

    Mat4 t(1.0f);
    t[3][0] = 10.0f;                       // translate by +10 along x
    t[3][1] = 20.0f;                       // ... and +20 along y

    Scene empty_scene;                     // no meshes: the lines define the box
    empty_scene.add_lines(layer, t);

    Vec3 min_b, max_b;
    empty_scene.compute_bounding_box(min_b, max_b);
    REQUIRE(min_b.x == Approx(10.0f));
    REQUIRE(max_b.x == Approx(11.0f));
    REQUIRE(min_b.y == Approx(20.0f));
    REQUIRE(max_b.y == Approx(20.0f));
}

TEST_CASE("set_line_affects_bounds updates the bounds of a scene", "[lines][scene]") {
    Mesh cube = make_unit_cube();

    LineLayer far_away;
    far_away.from = {Vec3(-100, -100, -100)};
    far_away.to = {Vec3(100, 100, 100)};
    far_away.colors = {kRed};

    Scene scene;
    scene.add(cube);
    scene.add_lines(far_away, Mat4(1.0f), "leader");

    REQUIRE(scene.line_affects_bounds(0));
    Vec3 min_b, max_b;
    scene.compute_bounding_box(min_b, max_b);
    REQUIRE(min_b.x == Approx(-100.0f));

    // By name, and by index (out of range indices are ignored).
    REQUIRE(scene.set_line_affects_bounds(std::string("leader"), false));
    REQUIRE_FALSE(scene.line_affects_bounds(0));
    scene.compute_bounding_box(min_b, max_b);
    REQUIRE(min_b.x == Approx(-1.0f));      // the unit cube, see make_unit_cube()

    scene.set_line_affects_bounds(0, true);
    REQUIRE(scene.line_affects_bounds(0));
    scene.set_line_affects_bounds(5, true);         // no effect
    REQUIRE(scene.line_count() == 1);
    REQUIRE_FALSE(scene.set_line_affects_bounds(std::string("nope"), false));
    REQUIRE(scene.line_affects_bounds(0));
    REQUIRE_FALSE(scene.line_affects_bounds(7));
}

TEST_CASE("LineLayer compute_bounding_box covers both endpoints", "[lines][scene]") {
    LineLayer layer;
    layer.from = {Vec3(0, 0, 0), Vec3(5, 0, 0)};
    layer.to = {Vec3(1, 2, 3), Vec3(-1, 0, 0)};

    Vec3 min_b, max_b;
    REQUIRE(layer.compute_bounding_box(min_b, max_b));
    REQUIRE(min_b.x == Approx(-1.0f));
    REQUIRE(max_b.x == Approx(5.0f));
    REQUIRE(min_b.y == Approx(0.0f));
    REQUIRE(max_b.y == Approx(2.0f));
    REQUIRE(min_b.z == Approx(0.0f));
    REQUIRE(max_b.z == Approx(3.0f));

    LineLayer empty_layer;
    REQUIRE_FALSE(empty_layer.compute_bounding_box(min_b, max_b));
}

TEST_CASE("line layer transforms and names are stored", "[lines][scene]") {
    Scene scene;
    LineLayer layer;
    layer.from = {Vec3(0, 0, 0)};
    layer.to = {Vec3(1, 0, 0)};
    layer.colors = {kRed};

    Mat4 t(1.0f);
    t[3][0] = 5.0f;    // translate by +5 along x
    scene.add_lines(layer, t, "edge");

    REQUIRE(scene.line_count() == 1);
    REQUIRE(scene.line_name(0) == "edge");
    REQUIRE(scene.line_transform(0)[3][0] == Approx(5.0f));

    const std::vector<LineNodeRef> refs = scene.line_nodes();
    REQUIRE(refs.size() == 1);
    REQUIRE(refs[0].layer == &scene.lines[0]);
    REQUIRE(refs[0].name == "edge");
}

TEST_CASE("line layer with empty geometry is skipped", "[lines][scene]") {
    RenderOptions opts = small_options(32);

    Scene scene;
    LineLayer empty_layer;      // no segments at all
    scene.add_lines(empty_layer);

    LineLayer mismatched;       // from/to of different lengths
    mismatched.from = {Vec3(0, 0, 0), Vec3(1, 0, 0)};
    mismatched.to = {Vec3(0, 1, 0)};
    mismatched.colors = {kRed};
    scene.add_lines(mismatched);

    REQUIRE(empty_layer.empty());
    REQUIRE(mismatched.size() == 1);

    Renderer renderer;
    Image img = renderer.render_scene(scene, front_camera(), opts);
    REQUIRE(img.width == 32);
}
