// Tests for the fog coordinate space (RenderOptions::fog_space): world-unit
// fog (default) versus the legacy normalized-device-depth fog.
#include "catch_amalgamated.hpp"
#include <scimesh/rasterizer.h>
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/mesh.h>
#include <scimesh/image.h>
#include <cmath>

using namespace scimesh;
using Catch::Approx;

namespace {

constexpr int IMG_W = 200;
constexpr int IMG_H = 200;
constexpr float CAM_DIST = 10.0f;  // ortho half-height == camera distance

// Axis-aligned square in the XY plane at z, facing +z.
Mesh make_quad(float z, float half, const Color &color) {
    Mesh mesh;
    mesh.vertices = {Vec3(-half, -half, z), Vec3(half, -half, z),
                     Vec3(half, half, z), Vec3(-half, half, z)};
    mesh.triangles = {Triangle{0, 1, 2}, Triangle{0, 2, 3}};
    mesh.colors.assign(4, color);
    return mesh;
}

// Near quad (2 x 2 at z = 0, i.e. 10 units from the camera) in front of a
// larger backdrop (6 x 6 at z = -20, i.e. 30 units from the camera).  Both
// are red, the fog color is green.
Scene make_scene() {
    Scene scene;
    scene.meshes.push_back(make_quad(0.0f, 1.0f, Color(1, 0, 0, 1)));
    scene.meshes.push_back(make_quad(-20.0f, 3.0f, Color(1, 0, 0, 1)));
    return scene;
}

Camera make_camera() {
    Camera cam;
    cam.eye = Vec3(0.0f, 0.0f, CAM_DIST);
    cam.center = Vec3(0.0f, 0.0f, 0.0f);
    cam.up = Vec3(0.0f, 1.0f, 0.0f);
    cam.projection = ProjectionType::ORTHOGRAPHIC;
    return cam;
}

// Note: `fog_space` is deliberately left at its default value here, so that
// the tests exercise the default (world) behaviour.
RenderOptions make_options(bool fog_enabled, float near_plane, float far_plane) {
    RenderOptions opts;
    opts.width = IMG_W;
    opts.height = IMG_H;
    opts.projection = ProjectionType::ORTHOGRAPHIC;
    opts.near_plane = near_plane;
    opts.far_plane = far_plane;
    opts.background_color = Color(0, 0, 0, 1);
    opts.lights.push_back(Light{Vec3(0, 0, 1), Color(1, 1, 1, 1), 1.0f, true});
    opts.threads = 1;
    opts.fog_enabled = fog_enabled;
    opts.fog_start = 5.0f;   // world units in the default (world) space
    opts.fog_end = 15.0f;
    opts.fog_color = Color(0, 1, 0, 1);  // green
    return opts;
}

RenderOptions with_space(const RenderOptions &opts, FogSpace space) {
    RenderOptions out = opts;
    out.fog_space = space;
    return out;
}

Image render(const Scene &scene, const RenderOptions &opts) {
    Renderer renderer;
    return renderer.render_scene(scene, make_camera(), opts);
}

// The ortho projection uses half_height == distance, so world (x, y) maps
// linearly onto pixels.
void world_to_pixel(float x, float y, int &col, int &row) {
    float ndc_x = x / CAM_DIST;
    float ndc_y = y / CAM_DIST;
    col = static_cast<int>((ndc_x + 1.0f) * 0.5f * IMG_W);
    row = static_cast<int>((1.0f - ndc_y) * 0.5f * IMG_H);
}

Color pixel_at(const Image &img, float x, float y) {
    int col, row;
    world_to_pixel(x, y, col, row);
    uint8_t r, g, b, a;
    img.get_pixel(col, row, r, g, b, a);
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

int count_differing_pixels(const Image &a, const Image &b, int tolerance = 0) {
    int n = 0;
    for (int i = 0; i < IMG_W * IMG_H; ++i) {
        for (int c = 0; c < 4; ++c) {
            int d = static_cast<int>(a.pixels[i * 4 + c]) -
                    static_cast<int>(b.pixels[i * 4 + c]);
            if (d < 0) d = -d;
            if (d > tolerance) { n++; break; }
        }
    }
    return n;
}

} // namespace

TEST_CASE("fog_depth_from_ndc converts depth-buffer values to world units",
          "[fog][fog_space]") {
    Rasterizer rast(4, 4);
    rast.z_near = 1.0f;
    rast.z_far = 11.0f;

    // Perspective: -1 is the near plane, +1 the far plane.
    rast.orthographic = false;
    REQUIRE(rast.fog_depth_from_ndc(-1.0f) == Approx(1.0f).margin(1e-4f));
    REQUIRE(rast.fog_depth_from_ndc(1.0f) == Approx(11.0f).margin(1e-3f));
    float mid_persp = rast.fog_depth_from_ndc(0.0f);
    REQUIRE(mid_persp > 1.0f);
    REQUIRE(mid_persp < 11.0f);
    // Perspective depth is non-linear: most of the depth range is very close
    // to the camera.
    REQUIRE(mid_persp < 6.0f);
    REQUIRE(rast.fog_depth_from_ndc(-0.5f) < mid_persp);
    REQUIRE(rast.fog_depth_from_ndc(0.5f) > mid_persp);

    // Orthographic: depth is linear in world units.
    rast.orthographic = true;
    REQUIRE(rast.fog_depth_from_ndc(-1.0f) == Approx(1.0f));
    REQUIRE(rast.fog_depth_from_ndc(0.0f) == Approx(6.0f));
    REQUIRE(rast.fog_depth_from_ndc(1.0f) == Approx(11.0f));
}

TEST_CASE("Fog defaults to world units", "[fog][fog_space]") {
    REQUIRE(RenderOptions().fog_space == FogSpace::WORLD);

    Scene scene = make_scene();
    // `make_options` never touches fog_space, so this renders with whatever
    // the default space is; it must match an explicit world-space request.
    Image default_space = render(scene, make_options(true, 0.1f, 1000.0f));
    Image explicit_world = render(
        scene, with_space(make_options(true, 0.1f, 1000.0f), FogSpace::WORLD));
    REQUIRE(count_differing_pixels(default_space, explicit_world) == 0);
}

TEST_CASE("World-space fog fades geometry by its distance from the camera",
          "[fog][fog_space][integration]") {
    Scene scene = make_scene();
    Image unfogged = render(scene, make_options(false, 0.1f, 1000.0f));
    Image fogged = render(scene, make_options(true, 0.1f, 1000.0f));

    // Near quad: 10 units away, fog spans 5..15 world units -> factor 0.5.
    Color near_base = pixel_at(unfogged, 0.0f, 0.0f);
    Color near_fogged = pixel_at(fogged, 0.0f, 0.0f);
    REQUIRE(near_fogged.r == Approx(0.5f * near_base.r + 0.5f * 0.0f).margin(0.02f));
    REQUIRE(near_fogged.g == Approx(0.5f * near_base.g + 0.5f * 1.0f).margin(0.02f));
    REQUIRE(near_fogged.b == Approx(0.5f * near_base.b + 0.5f * 0.0f).margin(0.02f));

    // Backdrop: 30 units away, beyond fog_end -> completely fogged.
    Color far_base = pixel_at(unfogged, 2.0f, 2.0f);
    Color far_fogged = pixel_at(fogged, 2.0f, 2.0f);
    REQUIRE(far_base.r > 0.2f);  // sanity: the backdrop is drawn there
    REQUIRE(far_fogged.r == Approx(0.0f).margin(0.01f));
    REQUIRE(far_fogged.g == Approx(1.0f).margin(0.01f));
    REQUIRE(far_fogged.b == Approx(0.0f).margin(0.01f));
}

TEST_CASE("World-space fog ignores the near/far plane settings, NDC fog does not",
          "[fog][fog_space][integration]") {
    Scene scene = make_scene();

    Image world_near_far = render(scene, make_options(true, 0.1f, 1000.0f));
    Image world_short = render(scene, make_options(true, 0.1f, 60.0f));
    // Identical up to the +/-1 rounding of the fog blend (the depth inversion
    // differs by a tiny epsilon between the two far planes).
    REQUIRE(count_differing_pixels(world_near_far, world_short, 1) == 0);

    // NDC: the very same fog settings now depend on the depth range.  The
    // quads are 10 and 30 world units away, but their depth-buffer values are
    // -0.98 / -0.94 for far = 1000 and -0.67 / -0.002 for far = 60, so an NDC
    // fog band of [-0.8, 0.0] contains no geometry in the first case and
    // almost all of it in the second.
    RenderOptions long_opts = with_space(make_options(true, 0.1f, 1000.0f), FogSpace::NDC);
    long_opts.fog_start = -0.8f;
    long_opts.fog_end = 0.0f;
    RenderOptions short_opts = long_opts;
    short_opts.far_plane = 60.0f;

    Image ndc_long = render(scene, long_opts);
    Image ndc_short = render(scene, short_opts);
    REQUIRE(count_differing_pixels(ndc_long, ndc_short, 1) > 100);

    // Sanity check of the NDC interpretation: with far = 60 the distant
    // backdrop is fully fogged while the near quad is barely affected ...
    Color near_px = pixel_at(ndc_short, 0.0f, 0.0f);
    Color far_px = pixel_at(ndc_short, 2.0f, 2.0f);
    REQUIRE(near_px.r > 0.7f);
    REQUIRE(far_px.g > 0.99f);
    REQUIRE(far_px.r < 0.01f);

    // ... whereas with far = 1000 nothing is inside the fog band at all.
    Color ndc_long_backdrop = pixel_at(ndc_long, 2.0f, 2.0f);
    REQUIRE(ndc_long_backdrop.r > 0.9f);
}

TEST_CASE("Legacy NDC fog uses raw depth-buffer values",
          "[fog][fog_space][integration]") {
    Scene scene = make_scene();
    // near = 5, far = 40 (orthographic): the quads sit at z_ndc = -0.71
    // (10 world units away) and z_ndc = +0.43 (30 world units away).  With NDC
    // fog from -0.5 to 0.5 the near quad is not fogged at all, while the more
    // distant backdrop is almost completely fogged - the opposite of what the
    // same world distances produce with world-space fog.
    RenderOptions opts = with_space(make_options(true, 5.0f, 40.0f), FogSpace::NDC);
    opts.fog_start = -0.5f;
    opts.fog_end = 0.5f;
    Image img = render(scene, opts);

    Color near_px = pixel_at(img, 0.0f, 0.0f);
    REQUIRE(near_px.r > 0.5f);   // still red: not fogged
    REQUIRE(near_px.g < 0.1f);

    Color far_px = pixel_at(img, 2.0f, 2.0f);
    REQUIRE(far_px.g > 0.85f);   // almost green: fully fogged
    REQUIRE(far_px.r < 0.15f);
}
