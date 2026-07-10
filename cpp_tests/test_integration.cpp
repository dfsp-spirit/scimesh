#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "mesh.h"
#include "scene.h"
#include "image.h"
#include "normals.h"
#include <iostream>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using scimesh_test::make_colored_cube;
using scimesh_test::make_tetrahedron;
using scimesh_test::make_plane;
using scimesh_test::make_icosphere;

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

    // Inverted normals flip which faces are lit by the headlight, so the total
    // brightness (sum over all pixels) must differ. With correct normals, the
    // front-facing cube faces (toward camera) are fully lit; with inverted
    // normals they fall to ambient-only, so the normal image is brighter.
    long bright_normal = 0, bright_inverted = 0;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            uint8_t r1, g1, b1, a1, r2, g2, b2, a2;
            img_normal.get_pixel(x, y, r1, g1, b1, a1);
            img_inverted.get_pixel(x, y, r2, g2, b2, a2);
            bright_normal += r1 + g1 + b1;
            bright_inverted += r2 + g2 + b2;
        }
    }
    REQUIRE(bright_normal > bright_inverted);
}

// --- 5.2: Depth testing across multiple meshes in a scene ---

TEST_CASE("Depth buffer occludes farther mesh behind nearer mesh", "[integration]") {
    // Two cubes at the same screen position but different depths.
    // The front one (red, closer to camera) should fully occlude the back one (green).
    Scene scene;

    Mesh back_cube = make_unit_cube();
    back_cube.colors.assign(8, Color(0, 1, 0, 1)); // green
    for (auto &v : back_cube.vertices)
        v += Vec3(0, 0, -3.0f); // push behind

    Mesh front_cube = make_unit_cube();
    front_cube.colors.assign(8, Color(1, 0, 0, 1)); // red

    scene.meshes = {back_cube, front_cube};

    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 100;
    opts.height = 100;
    opts.background_color = Color(0, 0, 0, 1);
    opts.backface_culling = false; // render all faces so cube is solid

    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    // Sample center pixels — should be red (front cube) not green (back cube)
    int red_pixels = 0, green_pixels = 0;
    for (int y = 30; y < 70; ++y) {
        for (int x = 30; x < 70; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r > 100 && g < 50)
                red_pixels++;
            if (g > 100 && r < 50)
                green_pixels++;
        }
    }
    REQUIRE(red_pixels > 0);
    REQUIRE(green_pixels == 0); // back cube fully occluded in center
}

// --- 5.2: Backface culling integration test ---

TEST_CASE("Backface culling hides interior faces when camera is inside mesh", "[integration]") {
    // When the camera is INSIDE a closed convex mesh, all outward-facing normals
    // point away from the camera → all triangles are back-facing.
    // With culling ON: nothing renders. With culling OFF: interior is visible.
    Mesh mesh = make_colored_cube();

    Camera cam;
    cam.eye = Vec3(0, 0, 0);   // at the cube center
    cam.center = Vec3(0, 0, -1); // looking at the back face

    RenderOptions opts_on, opts_off;
    opts_on.width = opts_off.width = 100;
    opts_on.height = opts_off.height = 100;
    opts_on.background_color = opts_off.background_color = Color(0, 0, 0, 1);
    opts_on.near_plane = opts_off.near_plane = 0.01f;

    opts_on.backface_culling = true;
    opts_off.backface_culling = false;

    Renderer renderer;
    Image img_on = renderer.render_mesh(mesh, cam, opts_on);
    Image img_off = renderer.render_mesh(mesh, cam, opts_off);

    int colored_on = 0, colored_off = 0;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            uint8_t r1, g1, b1, a1, r2, g2, b2, a2;
            img_on.get_pixel(x, y, r1, g1, b1, a1);
            img_off.get_pixel(x, y, r2, g2, b2, a2);
            if (r1 + g1 + b1 > 0) colored_on++;
            if (r2 + g2 + b2 > 0) colored_off++;
        }
    }

    // With culling ON, all faces are back-facing from inside → almost nothing.
    // With culling OFF, the interior is visible.
    REQUIRE(colored_off > colored_on);
    REQUIRE(colored_off > 100);
}

// --- 5.2: Near-plane clipping integration test (camera inside mesh) ---

TEST_CASE("Near-plane clipping handles camera positioned inside mesh", "[integration]") {
    // Position the camera inside the cube so the near plane cuts through it.
    Mesh mesh = make_unit_cube();
    mesh.colors.assign(8, Color(0.8f, 0.2f, 0.2f, 1.0f));

    Camera cam;
    cam.eye = Vec3(0, 0, 0.5f);  // inside the cube (cube spans z=-1..+1)
    cam.center = Vec3(0, 0, -1);  // looking at back face

    RenderOptions opts;
    opts.width = 100;
    opts.height = 100;
    opts.background_color = Color(0, 0, 0, 1);
    opts.near_plane = 0.1f;
    opts.backface_culling = false; // see interior faces

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    // Should render without crashing and produce some visible pixels
    // (triangles intersecting the near plane are clipped, not discarded)
    int colored = 0;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r + g + b > 0)
                colored++;
        }
    }
    REQUIRE(colored > 0);
}

// --- 5.1: Icosphere and plane test meshes ---

TEST_CASE("Plane mesh renders correctly", "[integration]") {
    Mesh mesh = make_plane();
    mesh.colors.assign(4, Color(0.2f, 0.8f, 0.2f, 1.0f));

    Camera cam;
    cam.eye = Vec3(0, 0, 5);

    RenderOptions opts;
    opts.width = 100;
    opts.height = 100;
    opts.background_color = Color(0, 0, 0, 1);

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    int green_pixels = 0;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (g > r && g > 100)
                green_pixels++;
        }
    }
    REQUIRE(green_pixels > 0);
}

TEST_CASE("Icosphere renders with smooth shading", "[integration]") {
    Mesh mesh = make_icosphere(2); // 2 subdivisions → 162 vertices
    mesh.colors.assign(mesh.vertices.size(), Color(0.7f, 0.7f, 0.9f, 1.0f));

    REQUIRE(mesh.vertices.size() >= 12);
    REQUIRE(mesh.triangles.size() >= 20);

    Camera cam;
    cam.eye = Vec3(0, 0, 3);

    RenderOptions opts;
    opts.width = 150;
    opts.height = 150;
    opts.background_color = Color(1, 1, 1, 1);
    opts.shading = ShadingMode::SMOOTH;

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);
    img.write_ppm("test_icosphere_smooth.ppm");

    // Should fill a roughly circular region in the center
    int colored = 0;
    for (int y = 0; y < 150; ++y) {
        for (int x = 0; x < 150; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r < 250 || g < 250 || b < 250)
                colored++;
        }
    }
    REQUIRE(colored > 100);

    // Smooth shading should produce a gradient (not uniform color).
    // Sample two points along a radial line, they should differ in brightness.
    uint8_t center_r, center_g, center_b, center_a;
    uint8_t edge_r, edge_g, edge_b, edge_a;
    img.get_pixel(75, 75, center_r, center_g, center_b, center_a);
    img.get_pixel(75, 50, edge_r, edge_g, edge_b, edge_a);
    int center_bright = center_r + center_g + center_b;
    int edge_bright = edge_r + edge_g + edge_b;
    // The center (facing light) should generally be brighter than the edge (angled).
    // At minimum they should differ due to smooth shading gradient.
    REQUIRE(center_bright != edge_bright);
}
