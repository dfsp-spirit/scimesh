#include "catch_amalgamated.hpp"
#include "primitives.h"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include "math_utils.h"

#include <filesystem>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace scimesh;
using Catch::Approx;

TEST_CASE("generate_sphere produces sane mesh", "[primitives]") {
    Mesh s = generate_sphere(Vec3(0, 0, 0), 2.0f, 16, Color(1, 0, 0));

    REQUIRE_FALSE(s.empty());
    REQUIRE(s.is_valid());
    REQUIRE(s.has_colors());
    REQUIRE(s.has_normals());
    REQUIRE(s.vertices.size() > 50);
    REQUIRE(s.triangles.size() > 100);

    for (const auto &n : s.normals) {
        float len = glm::length(n);
        REQUIRE(len == Approx(1.0f).margin(0.01f));
    }
}

TEST_CASE("generate_sphere normals point outward", "[primitives]") {
    Vec3 center(5, 0, -3);
    Mesh s = generate_sphere(center, 1.0f, 16, Color(1, 1, 1));

    for (size_t i = 0; i < s.vertices.size(); ++i) {
        Vec3 d = glm::normalize(s.vertices[i] - center);
        float dp = glm::dot(d, s.normals[i]);
        REQUIRE(dp == Approx(1.0f).margin(0.01f));
    }
}

TEST_CASE("generate_cylinder produces sane mesh", "[primitives]") {
    Mesh c = generate_cylinder(Vec3(0, 0, 0), Vec3(0, 3, 0), 1.0f, 16,
                               Color(0, 1, 0));

    REQUIRE_FALSE(c.empty());
    REQUIRE(c.is_valid());
    REQUIRE(c.has_colors());
    REQUIRE(c.has_normals());
    REQUIRE(c.vertices.size() > 20);
    REQUIRE(c.triangles.size() > 20);
}

TEST_CASE("generate_cone produces sane mesh", "[primitives]") {
    Mesh k = generate_cone(Vec3(0, 0, 0), Vec3(0, 3, 0), 1.0f, 16,
                           Color(0, 0, 1));

    REQUIRE_FALSE(k.empty());
    REQUIRE(k.is_valid());
    REQUIRE(k.has_colors());
    REQUIRE(k.has_normals());

    Vec3 tip_max(0, 0, 0);
    for (const auto &v : k.vertices) {
        if (v.y > tip_max.y)
            tip_max = v;
    }
    REQUIRE(tip_max.y == Approx(3.0f).margin(0.01f));
}

TEST_CASE("generate_arrow produces sane mesh", "[primitives]") {
    Mesh a = generate_arrow(Vec3(-2, 0, 0), Vec3(2, 0, 0),
                            0.1f, 0.25f, 0.8f, 16, Color(1, 1, 0));

    REQUIRE_FALSE(a.empty());
    REQUIRE(a.is_valid());

    float min_x = 0, max_x = 0;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (i == 0 || a.vertices[i].x < min_x)
            min_x = a.vertices[i].x;
        if (i == 0 || a.vertices[i].x > max_x)
            max_x = a.vertices[i].x;
    }
    REQUIRE(min_x <= -2.0f);
    REQUIRE(max_x >= 2.0f - 0.01f);
}

TEST_CASE("merge_mesh offsets triangle indices", "[primitives]") {
    Mesh a = generate_sphere(Vec3(0, 0, 0), 0.5f, 8, Color(1, 0, 0));
    Mesh b = generate_sphere(Vec3(2, 0, 0), 0.5f, 8, Color(0, 1, 0));

    size_t a_count = a.vertices.size();
    size_t b_count = b.vertices.size();

    merge_mesh(a, b);

    REQUIRE(a.vertices.size() == a_count + b_count);
    REQUIRE(a.is_valid());
}

TEST_CASE("Visual test: all primitives", "[primitives]") {
    Scene scene;

    scene.meshes.push_back(
        generate_sphere(Vec3(-3, 0, 0), 1.5f, 32, Color(0.9f, 0.2f, 0.2f)));

    scene.meshes.push_back(
        generate_cylinder(Vec3(1, -1.5f, 0), Vec3(1, 1.5f, 0), 0.6f, 32,
                          Color(0.2f, 0.9f, 0.2f)));

    scene.meshes.push_back(
        generate_cone(Vec3(4, -1.5f, 0), Vec3(4, 1.5f, 0), 0.8f, 32,
                      Color(0.2f, 0.2f, 0.9f)));

    scene.meshes.push_back(
        generate_arrow(Vec3(-3, 3, 0), Vec3(1, 3, 0),
                       0.12f, 0.3f, 0.6f, 24, Color(0.9f, 0.9f, 0.1f)));

    Camera cam = camera_fit_scene(scene, Vec3(0, 0, 1), Vec3(0, 1, 0), 45.0f, 1.2f);

    RenderOptions opts;
    opts.width = 800;
    opts.height = 600;
    opts.background_color = Color(0.1f, 0.1f, 0.15f, 1.0f);
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = true;

    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    int colored = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r > 20 || g > 20 || b > 20)
                colored++;
        }
    }
    REQUIRE(colored > 1000);

    std::filesystem::create_directories("reference_images");
    stbi_write_png("reference_images/primitives_all.png",
                   img.width, img.height, 4, img.pixels.data(),
                   img.width * 4);
    REQUIRE(std::filesystem::exists("reference_images/primitives_all.png"));
}

TEST_CASE("Visual test: arrow directions", "[primitives]") {
    Color red(0.9f, 0.2f, 0.2f);
    Color green(0.2f, 0.9f, 0.2f);
    Color blue(0.2f, 0.2f, 0.9f);

    Scene scene;
    scene.meshes.push_back(
        generate_arrow(Vec3(0, 0, 0), Vec3(3, 0, 0), 0.08f, 0.2f, 0.6f, 16, red));
    scene.meshes.push_back(
        generate_arrow(Vec3(0, 0, 0), Vec3(0, 3, 0), 0.08f, 0.2f, 0.6f, 16, green));
    scene.meshes.push_back(
        generate_arrow(Vec3(0, 0, 0), Vec3(0, 0, 3), 0.08f, 0.2f, 0.6f, 16, blue));

    Camera cam = camera_fit_scene(scene, Vec3(1, 1, 1), Vec3(0, 1, 0), 45.0f);

    RenderOptions opts;
    opts.width = 600;
    opts.height = 600;
    opts.background_color = Color(0.1f, 0.1f, 0.15f, 1.0f);
    opts.shading = ShadingMode::SMOOTH;

    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    int colored = 0;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r > 20 || g > 20 || b > 20)
                colored++;
        }
    REQUIRE(colored > 500);

    stbi_write_png("reference_images/arrows_xyz.png",
                   img.width, img.height, 4, img.pixels.data(),
                   img.width * 4);
}
