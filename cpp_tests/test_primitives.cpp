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

TEST_CASE("generate_sphere caps face outward", "[primitives]") {
    Vec3 center(10, 20, 30);
    Mesh s = generate_sphere(center, 2.0f, 32, Color(1, 1, 1));

    int north_cap_tris = 32;
    for (int i = 0; i < north_cap_tris; i++) {
        const auto &t = s.triangles[i];
        Vec3 v0 = s.vertices[t.v0];
        Vec3 v1 = s.vertices[t.v1];
        Vec3 v2 = s.vertices[t.v2];

        Vec3 normal = glm::cross(v1 - v0, v2 - v0);
        REQUIRE(glm::length(normal) > 0.001f);

        Vec3 ctr = (v0 + v1 + v2) / 3.0f;
        float dot = glm::dot(glm::normalize(normal),
                             glm::normalize(ctr - center));
        REQUIRE(dot > 0.0f);
    }

    int south_start = static_cast<int>(s.triangles.size()) - 32;
    for (int i = south_start; i < static_cast<int>(s.triangles.size()); i++) {
        const auto &t = s.triangles[i];
        Vec3 v0 = s.vertices[t.v0];
        Vec3 v1 = s.vertices[t.v1];
        Vec3 v2 = s.vertices[t.v2];

        Vec3 normal = glm::cross(v1 - v0, v2 - v0);
        REQUIRE(glm::length(normal) > 0.001f);

        Vec3 ctr = (v0 + v1 + v2) / 3.0f;
        float dot = glm::dot(glm::normalize(normal),
                             glm::normalize(ctr - center));
        REQUIRE(dot > 0.0f);
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

TEST_CASE("generate_cuboid produces correct vertex and face counts", "[primitives]") {
    Mesh c = generate_cuboid(Vec3(1, 2, 3), Vec3(0.5f, 1.0f, 1.5f),
                             Color(0.2f, 0.6f, 1.0f, 1.0f));

    REQUIRE(c.vertices.size() == 24);
    REQUIRE(c.triangles.size() == 12);
    REQUIRE(c.colors.size() == 24);
    REQUIRE(c.is_valid());
    REQUIRE_FALSE(c.empty());
}

TEST_CASE("generate_cuboid bounding box matches extents", "[primitives]") {
    Vec3 center(1, 2, 3);
    Vec3 half(2, 3, 4);
    Mesh c = generate_cuboid(center, half, Color(1, 1, 1));

    Vec3 bmin, bmax;
    c.compute_bounding_box(bmin, bmax);

    REQUIRE(bmin.x == Approx(center.x - half.x));
    REQUIRE(bmin.y == Approx(center.y - half.y));
    REQUIRE(bmin.z == Approx(center.z - half.z));
    REQUIRE(bmax.x == Approx(center.x + half.x));
    REQUIRE(bmax.y == Approx(center.y + half.y));
    REQUIRE(bmax.z == Approx(center.z + half.z));
}

TEST_CASE("generate_cuboid face normals point outward", "[primitives]") {
    Vec3 center(29, -7, 14);
    Vec3 half(2, 5, 3);
    Mesh c = generate_cuboid(center, half, Color(1, 1, 1));

    for (size_t i = 0; i < c.triangles.size(); i++) {
        const auto &t = c.triangles[i];
        const Vec3 &v0 = c.vertices[t.v0];
        const Vec3 &v1 = c.vertices[t.v1];
        const Vec3 &v2 = c.vertices[t.v2];

        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;
        Vec3 normal = glm::cross(edge1, edge2);
        REQUIRE(glm::length(normal) > 0.01f);

        Vec3 centroid = (v0 + v1 + v2) / 3.0f;
        Vec3 outward = centroid - center;
        float dot = glm::dot(glm::normalize(normal), glm::normalize(outward));
        REQUIRE(dot > 0.0f);
    }
}

TEST_CASE("generate_pyramid has correct structure", "[primitives]") {
    Mesh p = generate_pyramid(Vec3(0, 0, 0), Vec3(0, 2, 0), 1.5f,
                              Color(1, 0, 0));

    REQUIRE(p.vertices.size() == 16);
    REQUIRE(p.triangles.size() == 6);
    REQUIRE(p.colors.size() == 16);
    REQUIRE(p.is_valid());
    REQUIRE_FALSE(p.empty());
}

TEST_CASE("generate_pyramid base normals point downward", "[primitives]") {
    Mesh p = generate_pyramid(Vec3(1, 0, 2), Vec3(1, 3, 2), 2.0f,
                              Color(1, 1, 1));

    // Base triangles are indices 4 and 5 (last two)
    for (int idx = 4; idx <= 5; idx++) {
        const auto &t = p.triangles[idx];
        const Vec3 &v0 = p.vertices[t.v0];
        const Vec3 &v1 = p.vertices[t.v1];
        const Vec3 &v2 = p.vertices[t.v2];

        Vec3 normal = glm::cross(v1 - v0, v2 - v0);
        REQUIRE(glm::length(normal) > 0.01f);
        // Pyramid base at y=0, apex at y>0, normal should point down (-y)
        REQUIRE(normal.y < 0.0f);
    }
}

TEST_CASE("generate_tetrahedron has correct structure", "[primitives]") {
    Vec3 p0(0, 0, 0), p1(2, 0, 0), p2(1, 0, 2), p3(1, 2, 1);
    Mesh t = generate_tetrahedron(p0, p1, p2, p3, Color(1, 1, 1));

    REQUIRE(t.vertices.size() == 12);
    REQUIRE(t.triangles.size() == 4);
    REQUIRE(t.colors.size() == 12);
    REQUIRE(t.is_valid());
    REQUIRE_FALSE(t.empty());
}

TEST_CASE("generate_tetrahedron faces point outward", "[primitives]") {
    Vec3 p0(0, 0, 0), p1(2, 0, 0), p2(1, 0, 2), p3(1, 2, 1);
    Mesh t = generate_tetrahedron(p0, p1, p2, p3, Color(1, 1, 1));

    Vec3 centroid = (p0 + p1 + p2 + p3) / 4.0f;

    for (size_t i = 0; i < t.triangles.size(); i++) {
        const auto &tri = t.triangles[i];
        Vec3 v0 = t.vertices[tri.v0];
        Vec3 v1 = t.vertices[tri.v1];
        Vec3 v2 = t.vertices[tri.v2];

        Vec3 normal = glm::cross(v1 - v0, v2 - v0);
        REQUIRE(glm::length(normal) > 0.01f);

        Vec3 face_ctr = (v0 + v1 + v2) / 3.0f;
        float dot = glm::dot(glm::normalize(normal),
                             glm::normalize(face_ctr - centroid));
        REQUIRE(dot > 0.0f);
    }
}
