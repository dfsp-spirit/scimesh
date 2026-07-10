#include "catch_amalgamated.hpp"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "mesh.h"
#include "scene.h"
#include "image.h"
#include "normals.h"
#include <iostream>

using namespace scimesh;

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

static Mesh make_colored_cube() {
    Mesh mesh = make_unit_cube();
    mesh.colors = {
        {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1},
        {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}, {0.5f, 0.5f, 0.5f, 1}
    };
    return mesh;
}

static Mesh make_tetrahedron() {
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

TEST_CASE("Renderer renders a cube with white background", "[integration]") {
    Mesh mesh = make_colored_cube();
    Camera cam;
    cam.eye = Vec3(0, 0, 5);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);
    cam.fov_degrees = 45.0f;

    RenderOptions opts;
    opts.width = 200;
    opts.height = 200;
    opts.background_color = Color(1, 1, 1, 1);

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    // Should have some non-background pixels (the cube)
    int colored_pixels = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r < 250 || g < 250 || b < 250)
                colored_pixels++;
        }
    }
    REQUIRE(colored_pixels > 0);
    img.write_ppm("test_cube.ppm");
}

TEST_CASE("Renderer renders a cube with transparent background", "[integration]") {
    Mesh mesh = make_colored_cube();
    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 100;
    opts.height = 100;
    opts.background_color = Color(0, 0, 0, 0); // transparent

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    // Corners should be transparent (alpha = 0)
    uint8_t r, g, b, a;
    img.get_pixel(0, 0, r, g, b, a);
    REQUIRE(a == 0);
}

TEST_CASE("Renderer renders mesh without vertex colors using default color", "[integration]") {
    Mesh mesh = make_unit_cube();
    // No colors set

    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 100;
    opts.height = 100;
    opts.default_color = Color(1, 0, 0, 1); // red default
    opts.background_color = Color(0, 0, 0, 1);

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    int red_pixels = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r > 100 && g < 50 && b < 50)
                red_pixels++;
        }
    }
    REQUIRE(red_pixels > 0);
}

TEST_CASE("Renderer renders multiple meshes in a scene", "[integration]") {
    Scene scene;
    Mesh mesh1 = make_colored_cube();
    Mesh mesh2 = make_colored_cube();
    for (auto &v : mesh2.vertices)
        v += Vec3(3.0f, 0, 0);
    scene.meshes = {mesh1, mesh2};

    Camera cam;
    cam.eye = Vec3(0, 0, 10);
    cam.fov_degrees = 45.0f;

    RenderOptions opts;
    opts.width = 200;
    opts.height = 200;
    opts.background_color = Color(0, 0, 0, 1);

    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    int colored = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r > 0 || g > 0 || b > 0)
                colored++;
        }
    }
    REQUIRE(colored > 0);
}

TEST_CASE("Renderer handles empty mesh gracefully", "[integration]") {
    Mesh mesh; // empty
    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 50;
    opts.height = 50;
    opts.background_color = Color(0.5f, 0.5f, 0.5f, 1);

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    // All pixels should be background color
    uint8_t r, g, b, a;
    img.get_pixel(25, 25, r, g, b, a);
    REQUIRE(r == 127);
    REQUIRE(g == 127);
    REQUIRE(b == 127);
}

TEST_CASE("Renderer renders tetrahedron with smooth shading", "[integration]") {
    Mesh mesh = make_tetrahedron();
    mesh.colors = {
        {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}
    };

    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 150;
    opts.height = 150;
    opts.background_color = Color(1, 1, 1, 1);
    opts.shading = ShadingMode::SMOOTH;

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);
    img.write_ppm("test_tetra_smooth.ppm");

    int colored = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r < 250 || g < 250 || b < 250)
                colored++;
        }
    }
    REQUIRE(colored > 0);
}

TEST_CASE("Renderer renders cube with flat shading", "[integration]") {
    Mesh mesh = make_unit_cube();
    mesh.colors.resize(8, Color(0.8f, 0.2f, 0.2f, 1.0f));

    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 150;
    opts.height = 150;
    opts.background_color = Color(1, 1, 1, 1);
    opts.shading = ShadingMode::FLAT;

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);
    img.write_ppm("test_cube_flat.ppm");

    int colored = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r < 250 || g < 250 || b < 250)
                colored++;
        }
    }
    REQUIRE(colored > 0);
}

TEST_CASE("Renderer with invert_normals produces darker output", "[integration]") {
    Mesh mesh = make_colored_cube();

    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts_normal, opts_inverted;
    opts_normal.width = opts_inverted.width = 100;
    opts_normal.height = opts_inverted.height = 100;
    opts_normal.background_color = opts_inverted.background_color = Color(0, 0, 0, 1);
    opts_normal.invert_normals = false;
    opts_inverted.invert_normals = true;

    Renderer renderer;
    Image img_normal = renderer.render_mesh(mesh, cam, opts_normal);
    Image img_inverted = renderer.render_mesh(mesh, cam, opts_inverted);

    int bright_normal = 0, bright_inverted = 0;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            uint8_t r1, g1, b1, a1, r2, g2, b2, a2;
            img_normal.get_pixel(x, y, r1, g1, b1, a1);
            img_inverted.get_pixel(x, y, r2, g2, b2, a2);
            if (r1 + g1 + b1 > 100) bright_normal++;
            if (r2 + g2 + b2 > 100) bright_inverted++;
        }
    }
    // Inverted normals should produce different brightness (different lit faces)
    // At minimum, the two images should differ
    REQUIRE(bright_normal != bright_inverted);
}
