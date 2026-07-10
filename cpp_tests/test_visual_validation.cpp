#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "mesh.h"
#include "scene.h"
#include "image.h"
#include <filesystem>
#include <cmath>

using namespace scimesh;
using scimesh_test::make_colored_cube;
using scimesh_test::make_unit_cube;
using scimesh_test::make_tetrahedron;
using scimesh_test::make_icosphere;

static const char *REF_DIR = "reference_images";

static void ensure_ref_dir() {
    std::filesystem::create_directories(REF_DIR);
}

// Count non-background pixels and compute centroid.
static void image_stats(const Image &img, Color bg, int &count, double &cx, double &cy) {
    count = 0;
    double sx = 0.0, sy = 0.0;
    uint8_t bgr = uint8_t(std::clamp(bg.r, 0.0f, 1.0f) * 255.0f);
    uint8_t bgg = uint8_t(std::clamp(bg.g, 0.0f, 1.0f) * 255.0f);
    uint8_t bgb = uint8_t(std::clamp(bg.b, 0.0f, 1.0f) * 255.0f);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r != bgr || g != bgg || b != bgb) {
                ++count;
                sx += x;
                sy += y;
            }
        }
    }
    if (count > 0) {
        cx = sx / count;
        cy = sy / count;
    } else {
        cx = cy = 0.0;
    }
}

// --- 5.3: Generate reference images for manual visual inspection ---

TEST_CASE("Generate reference images for visual inspection", "[visual]") {
    ensure_ref_dir();
    Renderer renderer;

    // Colored cube, perspective
    {
        Mesh m = make_colored_cube();
        Camera cam; cam.eye = Vec3(3, 3, 5); cam.fov_degrees = 45.0f;
        RenderOptions opts; opts.width = 400; opts.height = 400;
        opts.background_color = Color(1, 1, 1, 1);
        Image img = renderer.render_mesh(m, cam, opts);
        REQUIRE(img.write_ppm(std::string(REF_DIR) + "/cube_colored_perspective.ppm"));
    }

    // Flat-shaded cube
    {
        Mesh m = make_unit_cube();
        m.colors.assign(8, Color(0.8f, 0.2f, 0.2f, 1.0f));
        Camera cam; cam.eye = Vec3(3, 3, 5);
        RenderOptions opts; opts.width = 400; opts.height = 400;
        opts.background_color = Color(1, 1, 1, 1);
        opts.shading = ShadingMode::FLAT;
        Image img = renderer.render_mesh(m, cam, opts);
        REQUIRE(img.write_ppm(std::string(REF_DIR) + "/cube_flat.ppm"));
    }

    // Smooth-shaded icosphere
    {
        Mesh m = make_icosphere(3);
        m.colors.assign(m.vertices.size(), Color(0.7f, 0.7f, 0.9f, 1.0f));
        Camera cam; cam.eye = Vec3(0, 0, 3);
        RenderOptions opts; opts.width = 400; opts.height = 400;
        opts.background_color = Color(1, 1, 1, 1);
        opts.shading = ShadingMode::SMOOTH;
        Image img = renderer.render_mesh(m, cam, opts);
        REQUIRE(img.write_ppm(std::string(REF_DIR) + "/icosphere_smooth.ppm"));
    }

    // Tetrahedron, smooth
    {
        Mesh m = make_tetrahedron();
        m.colors = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}};
        Camera cam; cam.eye = Vec3(0, 0, 5);
        RenderOptions opts; opts.width = 400; opts.height = 400;
        opts.background_color = Color(1, 1, 1, 1);
        Image img = renderer.render_mesh(m, cam, opts);
        REQUIRE(img.write_ppm(std::string(REF_DIR) + "/tetrahedron_smooth.ppm"));
    }

    // Multi-mesh scene (two cubes side by side)
    {
        Scene scene;
        Mesh m1 = make_colored_cube();
        Mesh m2 = make_colored_cube();
        for (auto &v : m2.vertices) v += Vec3(3.5f, 0, 0);
        scene.meshes = {m1, m2};
        Camera cam; cam.eye = Vec3(1.75f, 0, 9); cam.center = Vec3(1.75f, 0, 0); cam.fov_degrees = 45.0f;
        RenderOptions opts; opts.width = 500; opts.height = 400;
        opts.background_color = Color(0.1f, 0.1f, 0.12f, 1);
        Image img = renderer.render_scene(scene, cam, opts);
        REQUIRE(img.write_ppm(std::string(REF_DIR) + "/scene_two_cubes.ppm"));
    }
}

// --- 5.3: Determinism — same inputs must produce bit-for-bit identical output ---

TEST_CASE("Renderer is deterministic: identical inputs produce identical output", "[visual]") {
    Mesh m = make_icosphere(2);
    m.colors.assign(m.vertices.size(), Color(0.7f, 0.7f, 0.9f, 1.0f));

    Camera cam; cam.eye = Vec3(2, 1, 3);
    RenderOptions opts; opts.width = 200; opts.height = 200;
    opts.background_color = Color(0, 0, 0, 1);

    Renderer renderer;
    Image a = renderer.render_mesh(m, cam, opts);
    Image b = renderer.render_mesh(m, cam, opts);

    REQUIRE(a.width == b.width);
    REQUIRE(a.height == b.height);
    REQUIRE(a.pixels == b.pixels); // std::vector<uint8_t> == compares element-wise
}

// --- 5.3: Spatial structure — icosphere fills a centered, radially symmetric region ---

TEST_CASE("Icosphere render is roughly centered and radially symmetric", "[visual]") {
    Mesh m = make_icosphere(3);
    m.colors.assign(m.vertices.size(), Color(0.7f, 0.7f, 0.9f, 1.0f));

    Camera cam; cam.eye = Vec3(0, 0, 3);
    Color bg(1, 1, 1, 1);
    RenderOptions opts; opts.width = 200; opts.height = 200;
    opts.background_color = bg;

    Renderer renderer;
    Image img = renderer.render_mesh(m, cam, opts);

    int count;
    double cx, cy;
    image_stats(img, bg, count, cx, cy);

    // The sphere must occupy a meaningful area.
    REQUIRE(count > 5000);

    // Centroid must be near the image center (within ~10% of half-width).
    REQUIRE(std::abs(cx - 99.5) < 20.0);
    REQUIRE(std::abs(cy - 99.5) < 20.0);

    // Radial symmetry: compare foreground pixel counts in left/right and top/bottom halves.
    int left = 0, right = 0, top = 0, bottom = 0;
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 200; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r > 250 && g > 250 && b > 250)
                continue; // background (white)
            if (x < 100) ++left; else ++right;
            if (y < 100) ++top; else ++bottom;
        }
    }
    // Counts should be balanced to within ~10%.
    int lr_diff = std::abs(left - right);
    int tb_diff = std::abs(top - bottom);
    REQUIRE(lr_diff <= std::max(1, (left + right) / 20));
    REQUIRE(tb_diff <= std::max(1, (top + bottom) / 20));
}

TEST_CASE("Perspective vs orthographic projection both render a visible mesh", "[visual]") {
    Mesh m = make_unit_cube();
    m.colors.assign(8, Color(0.2f, 0.6f, 0.9f, 1.0f));

    RenderOptions opts; opts.width = 150; opts.height = 150;
    opts.background_color = Color(0, 0, 0, 1);

    Renderer renderer;

    Camera pcam; pcam.eye = Vec3(0, 0, 6); pcam.projection = ProjectionType::PERSPECTIVE;
    Image pimg = renderer.render_mesh(m, pcam, opts);

    Camera ocam; ocam.eye = Vec3(0, 0, 6); ocam.projection = ProjectionType::ORTHOGRAPHIC;
    Image oimg = renderer.render_mesh(m, ocam, opts);

    int pcount = 0, ocount = 0;
    for (int y = 0; y < 150; ++y) {
        for (int x = 0; x < 150; ++x) {
            uint8_t pr, pg, pb, pa, orr, og, ob, oa;
            pimg.get_pixel(x, y, pr, pg, pb, pa);
            oimg.get_pixel(x, y, orr, og, ob, oa);
            if (pr + pg + pb > 0) ++pcount;
            if (orr + og + ob > 0) ++ocount;
        }
    }
    REQUIRE(pcount > 0);
    REQUIRE(ocount > 0);
}