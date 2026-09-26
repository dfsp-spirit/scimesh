#include "catch_amalgamated.hpp"
#include <scimesh/primitives.h>
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/image.h>
#include <scimesh/math_utils.h>

#include <filesystem>
#include <cmath>
#include <map>
#include <utility>
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

TEST_CASE("generate_cylinder vertex/triangle counts match the documented formulas",
          "[primitives][counts]") {
    // These counts are what generate_multi_cylinders() uses to reserve() its
    // arrays up front, so a mismatch here means over/under-allocation there.
    for (int segments : {3, 4, 8, 16, 32}) {
        const size_t s = static_cast<size_t>(segments);

        Mesh closed = generate_cylinder(Vec3(0, 0, 0), Vec3(0, 3, 0), 1.0f,
                                        segments, Color(1, 1, 1), true);
        REQUIRE(closed.vertices.size() == 4u * s + 2u);
        REQUIRE(closed.triangles.size() == 4u * s);
        REQUIRE(closed.normals.size() == closed.vertices.size());
        REQUIRE(closed.colors.size() == closed.vertices.size());

        Mesh open = generate_cylinder(Vec3(0, 0, 0), Vec3(0, 3, 0), 1.0f,
                                      segments, Color(1, 1, 1), false);
        REQUIRE(open.vertices.size() == 2u * s);
        REQUIRE(open.triangles.size() == 2u * s);
        REQUIRE(open.normals.size() == open.vertices.size());
        REQUIRE(open.colors.size() == open.vertices.size());
    }
}

TEST_CASE("generate_cylinder caps=false removes only the caps",
          "[primitives][caps]") {
    const int segments = 8;
    const Vec3 start(0, 0, 0);
    const Vec3 end(0, 5, 0);
    const float radius = 0.5f;

    Mesh closed = generate_cylinder(start, end, radius, segments, Color(1, 1, 1), true);
    Mesh open = generate_cylinder(start, end, radius, segments, Color(1, 1, 1), false);

    // The body is the first 2 * segments vertices of the closed mesh, in the
    // same order, so the open mesh must be a prefix of the closed one.
    REQUIRE(open.vertices.size() == 2u * static_cast<size_t>(segments));
    for (size_t i = 0; i < open.vertices.size(); ++i) {
        REQUIRE(open.vertices[i] == closed.vertices[i]);
        REQUIRE(open.normals[i] == closed.normals[i]);
    }
    for (size_t i = 0; i < open.triangles.size(); ++i) {
        REQUIRE(open.triangles[i].v0 == closed.triangles[i].v0);
        REQUIRE(open.triangles[i].v1 == closed.triangles[i].v1);
        REQUIRE(open.triangles[i].v2 == closed.triangles[i].v2);
    }

    // Without caps every vertex lies on the lateral surface, i.e. the distance
    // from the axis (here: the Y axis) equals the radius.  With caps there are
    // the two center vertices, which lie on the axis instead.
    for (const auto &v : open.vertices) {
        const float axial_dist = std::sqrt(v.x * v.x + v.z * v.z);
        REQUIRE(axial_dist == Approx(radius).margin(1e-5f));
    }

    bool has_axis_vertex = false;
    for (const auto &v : closed.vertices) {
        if (std::sqrt(v.x * v.x + v.z * v.z) < 1e-6f) {
            has_axis_vertex = true;
        }
    }
    REQUIRE(has_axis_vertex);

    // Saving is roughly 50 percent of the geometry.
    REQUIRE(open.vertices.size() < closed.vertices.size());
    REQUIRE(open.triangles.size() < closed.triangles.size());
}

TEST_CASE("generate_cylinder caps=false keeps the mesh valid",
          "[primitives][caps]") {
    for (int segments : {3, 8, 16}) {
        Mesh open = generate_cylinder(Vec3(1, -2, 3), Vec3(4, 2, -1), 0.25f,
                                      segments, Color(0.5f, 0.5f, 0.5f), false);
        REQUIRE_FALSE(open.empty());
        REQUIRE(open.is_valid());
        REQUIRE(open.has_normals());
        for (const auto &n : open.normals) {
            REQUIRE(glm::length(n) == Approx(1.0f).margin(0.01f));
        }
    }
}

TEST_CASE("generate_multi_cylinders matches merging the singles",
          "[primitives][batch]") {
    const int segments = 6;
    std::vector<Vec3> starts = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    std::vector<Vec3> ends = {{0, 3, 0}, {1, 3, 0}, {2, 3, 0}};
    std::vector<float> radii = {0.1f, 0.2f, 0.3f};
    std::vector<Color> colors = {Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1)};

    for (bool caps : {true, false}) {
        Mesh merged;
        for (size_t i = 0; i < starts.size(); ++i) {
            merge_mesh(merged, generate_cylinder(starts[i], ends[i], radii[i],
                                                 segments, colors[i], caps));
        }
        Mesh batched = generate_multi_cylinders(starts, ends, radii, colors,
                                                segments, caps);

        REQUIRE(batched.vertices.size() == merged.vertices.size());
        REQUIRE(batched.triangles.size() == merged.triangles.size());
        REQUIRE(batched.normals.size() == merged.normals.size());
        REQUIRE(batched.colors.size() == merged.colors.size());
        for (size_t i = 0; i < merged.vertices.size(); ++i) {
            REQUIRE(batched.vertices[i] == merged.vertices[i]);
            REQUIRE(batched.normals[i] == merged.normals[i]);
        }
        for (size_t i = 0; i < merged.triangles.size(); ++i) {
            REQUIRE(batched.triangles[i].v0 == merged.triangles[i].v0);
            REQUIRE(batched.triangles[i].v1 == merged.triangles[i].v1);
            REQUIRE(batched.triangles[i].v2 == merged.triangles[i].v2);
        }
    }
}

TEST_CASE("generate_multi_cylinders caps=false batches open tubes",
          "[primitives][batch][caps]") {
    const size_t n = 5;
    const int segments = 12;
    std::vector<Vec3> starts(n, Vec3(0, 0, 0));
    std::vector<Vec3> ends(n, Vec3(0, 2, 0));
    std::vector<float> radii(n, 0.05f);
    std::vector<Color> colors(n, Color(1, 1, 1));

    Mesh open = generate_multi_cylinders(starts, ends, radii, colors, segments, false);
    Mesh closed = generate_multi_cylinders(starts, ends, radii, colors, segments, true);

    REQUIRE(open.vertices.size() == n * 2u * static_cast<size_t>(segments));
    REQUIRE(open.triangles.size() == n * 2u * static_cast<size_t>(segments));
    REQUIRE(closed.vertices.size() == n * (4u * static_cast<size_t>(segments) + 2u));
    REQUIRE(closed.triangles.size() == n * 4u * static_cast<size_t>(segments));
    REQUIRE(open.is_valid());
    REQUIRE(closed.is_valid());
}

TEST_CASE("generate_multi_spheres vertex/triangle counts are exact",
          "[primitives][counts][batch]") {
    for (int segments : {3, 8, 16}) {
        const size_t n = 4;
        const size_t s = static_cast<size_t>(segments);
        std::vector<Vec3> centers(n, Vec3(0, 0, 0));
        std::vector<float> radii(n, 1.0f);
        std::vector<Color> colors(n, Color(1, 1, 1));

        Mesh m = generate_multi_spheres(centers, radii, colors, segments);
        REQUIRE(m.vertices.size() == n * (s * (s - 1u) + 2u));
        REQUIRE(m.triangles.size() == n * 2u * s * (s - 1u));
        REQUIRE(m.normals.size() == m.vertices.size());
        REQUIRE(m.colors.size() == m.vertices.size());
        REQUIRE(m.is_valid());
    }
}

TEST_CASE("batched generators handle empty input", "[primitives][batch]") {
    std::vector<Vec3> empty_vec3;
    std::vector<float> empty_float;
    std::vector<Color> empty_color;

    Mesh spheres = generate_multi_spheres(empty_vec3, empty_float, empty_color, 16);
    Mesh cylinders = generate_multi_cylinders(empty_vec3, empty_vec3, empty_float,
                                              empty_color, 12);
    REQUIRE(spheres.empty());
    REQUIRE(cylinders.empty());
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

// ---------------------------------------------------------------------------
//  Tubes (generalized cylinders along a polyline path)
// ---------------------------------------------------------------------------

TEST_CASE("generate_tube with a 2-point path matches generate_cylinder",
          "[primitives][tube]") {
    // A straight path is the degenerate case of a tube, and must produce the
    // exact same geometry as the cylinder generator.
    const Vec3 start(1, -2, 3);
    const Vec3 end(4, 2, -1);
    const float radius = 0.4f;
    const int segments = 12;
    const Color color(0.2f, 0.6f, 0.9f);

    std::vector<Vec3> path = {start, end};
    for (bool caps : {true, false}) {
        Mesh tube = generate_tube(path, radius, segments, color, caps, caps);
        Mesh cylinder = generate_cylinder(start, end, radius, segments, color, caps);

        REQUIRE(tube.vertices.size() == cylinder.vertices.size());
        REQUIRE(tube.triangles.size() == cylinder.triangles.size());
        for (size_t i = 0; i < cylinder.vertices.size(); ++i) {
            REQUIRE(tube.vertices[i] == cylinder.vertices[i]);
            REQUIRE(tube.normals[i] == cylinder.normals[i]);
        }
        for (size_t i = 0; i < cylinder.triangles.size(); ++i) {
            REQUIRE(tube.triangles[i].v0 == cylinder.triangles[i].v0);
            REQUIRE(tube.triangles[i].v1 == cylinder.triangles[i].v1);
            REQUIRE(tube.triangles[i].v2 == cylinder.triangles[i].v2);
        }
    }
}

TEST_CASE("generate_tube vertex/triangle counts match the documented formulas",
          "[primitives][tube][counts]") {
    const std::vector<Vec3> path = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}, {3, 1, 0}};
    const size_t k = path.size();

    for (int segments : {3, 8, 16}) {
        const size_t s = static_cast<size_t>(segments);

        Mesh closed = generate_tube(path, 0.1f, segments, Color(1, 1, 1), true, true);
        REQUIRE(closed.vertices.size() == k * s + 2u * (1u + s));
        REQUIRE(closed.triangles.size() == (k - 1u) * 2u * s + 2u * s);

        Mesh open = generate_tube(path, 0.1f, segments, Color(1, 1, 1), false, false);
        REQUIRE(open.vertices.size() == k * s);
        REQUIRE(open.triangles.size() == (k - 1u) * 2u * s);

        Mesh half = generate_tube(path, 0.1f, segments, Color(1, 1, 1), true, false);
        REQUIRE(half.vertices.size() == k * s + 1u + s);
        REQUIRE(half.triangles.size() == (k - 1u) * 2u * s + s);

        for (const Mesh *m : {&closed, &open, &half}) {
            REQUIRE(m->is_valid());
            REQUIRE(m->normals.size() == m->vertices.size());
            REQUIRE(m->colors.size() == m->vertices.size());
        }
    }
}

TEST_CASE("generate_tube rings are perpendicular to the path and at the radius",
          "[primitives][tube]") {
    const std::vector<Vec3> path = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}, {3, 1, 1}};
    const float radius = 0.35f;
    const int segments = 8;
    const size_t s = static_cast<size_t>(segments);

    Mesh tube = generate_tube(path, radius, segments, Color(1, 1, 1), false, false);
    REQUIRE(tube.vertices.size() == path.size() * s);

    for (size_t i = 0; i < path.size(); ++i) {
        for (size_t j = 0; j < s; ++j) {
            const Vec3 v = tube.vertices[i * s + j];
            // Every side vertex lies on a circle of `radius` around its path point.
            REQUIRE(glm::length(v - path[i]) == Approx(radius).margin(1e-5f));

            // ... and the ring is perpendicular to the path direction, so the
            // offset has no component along the tangent.
            const Vec3 tangent = glm::normalize(
                (i == 0) ? (path[1] - path[0])
                         : ((i + 1 == path.size()) ? (path[i] - path[i - 1])
                                                   : (path[i + 1] - path[i - 1])));
            REQUIRE(glm::dot(glm::normalize(v - path[i]), tangent) ==
                    Approx(0.0f).margin(1e-4f));

            // Normals are the (unit) radial vectors.
            REQUIRE(glm::length(tube.normals[i * s + j]) == Approx(1.0f).margin(0.01f));
            const float radial_dot =
                glm::dot(tube.normals[i * s + j], glm::normalize(v - path[i]));
            REQUIRE(radial_dot == Approx(1.0f).margin(1e-3f));
        }
    }
}

TEST_CASE("generate_tube does not twist around its own axis",
          "[primitives][tube]") {
    // Half circle in the XY plane: parallel transport must move each ring
    // vertex by roughly one segment length, without ever jumping to the
    // opposite side of the tube (which would happen for a twisting frame).
    std::vector<Vec3> path;
    const int num_points = 17;
    for (int i = 0; i < num_points; ++i) {
        const float angle = glm::pi<float>() * static_cast<float>(i) /
                            static_cast<float>(num_points - 1);
        path.push_back(Vec3(std::cos(angle), std::sin(angle), 0.0f));
    }

    const float radius = 0.2f;
    const int segments = 8;
    Mesh tube = generate_tube(path, radius, segments, Color(1, 1, 1), false, false);

    for (size_t i = 0; i + 1u < path.size(); ++i) {
        const float seg_len = glm::length(path[i + 1] - path[i]);
        for (int j = 0; j < segments; ++j) {
            const Vec3 a = tube.vertices[i * static_cast<size_t>(segments) +
                                         static_cast<size_t>(j)];
            const Vec3 b = tube.vertices[(i + 1u) * static_cast<size_t>(segments) +
                                         static_cast<size_t>(j)];
            // A frame flip would give ~2 * radius, a correct transport ~seg_len.
            REQUIRE(glm::length(b - a) < seg_len + 0.25f * radius);
        }
    }
}

TEST_CASE("generate_tube handles degenerate and reversed paths",
          "[primitives][tube]") {
    // Empty path and single point: no direction, no geometry.
    REQUIRE(generate_tube({}, 0.1f, 8, Color(1, 1, 1)).empty());
    REQUIRE(generate_tube({{1, 1, 1}}, 0.1f, 8, Color(1, 1, 1)).empty());

    // Duplicate points are removed, so the result equals the cleaned path.
    std::vector<Vec3> with_duplicates = {{0, 0, 0}, {0, 0, 0}, {0, 0, 1},
                                         {0, 0, 1}, {0, 0, 2}};
    Mesh dedup = generate_tube(with_duplicates, 0.1f, 8, Color(1, 1, 1),
                               false, false);
    Mesh cleaned = generate_tube({{0, 0, 0}, {0, 0, 1}, {0, 0, 2}}, 0.1f, 8,
                                 Color(1, 1, 1), false, false);
    REQUIRE(dedup.vertices.size() == cleaned.vertices.size());
    REQUIRE(dedup.vertices.size() == 3u * 8u);

    // A path that doubles back on itself exercises the anti-parallel branch of
    // the frame transport.  The result must still be a valid, finite mesh.
    std::vector<Vec3> reversing = {{0, 0, 0}, {0, 0, 1}, {0, 0, 0.5}, {0, 0, -1}};
    Mesh rev = generate_tube(reversing, 0.1f, 8, Color(1, 1, 1), true, true);
    REQUIRE_FALSE(rev.empty());
    REQUIRE(rev.is_valid());
    for (const auto &v : rev.vertices) {
        REQUIRE(std::isfinite(v.x));
        REQUIRE(std::isfinite(v.y));
        REQUIRE(std::isfinite(v.z));
    }
}

TEST_CASE("generate_tube open ends are manifold", "[primitives][tube]") {
    const std::vector<Vec3> path = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}};
    const int segments = 8;
    const size_t s = static_cast<size_t>(segments);
    Mesh open = generate_tube(path, 0.1f, segments, Color(1, 1, 1), false, false);

    // Count how often every (undirected, sorted) vertex pair occurs.
    std::map<std::pair<uint32_t, uint32_t>, int> edge_counts;
    for (const auto &tri : open.triangles) {
        const uint32_t idx[3] = {tri.v0, tri.v1, tri.v2};
        for (int e = 0; e < 3; ++e) {
            uint32_t a = idx[e];
            uint32_t b = idx[(e + 1) % 3];
            if (a > b) std::swap(a, b);
            edge_counts[{a, b}]++;
        }
    }

    // An open tube is a manifold *with boundary*: every edge is shared by two
    // triangles, except the ring edges of the first and last ring, which form
    // the two open ends and are used by a single triangle each.
    const auto on_boundary_ring = [&](uint32_t v) {
        const size_t ring = v / s;
        return ring == 0u || ring + 1u == path.size();
    };
    for (const auto &entry : edge_counts) {
        const bool at_open_end = on_boundary_ring(entry.first.first) &&
                                 on_boundary_ring(entry.first.second);
        REQUIRE(entry.second == (at_open_end ? 1 : 2));
    }
}

TEST_CASE("generate_multi_tubes matches merging the singles",
          "[primitives][tube][batch]") {
    const int segments = 6;
    std::vector<std::vector<Vec3>> paths = {
        {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}},
        {{0, 0, 2}, {1, 1, 2}},
        {{0, 0, 4}, {0, 1, 4}, {1, 1, 4}, {1, 0, 4}}};
    std::vector<float> radii = {0.1f, 0.2f, 0.05f};
    std::vector<Color> colors = {Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1)};

    for (bool caps : {true, false}) {
        Mesh merged;
        for (size_t i = 0; i < paths.size(); ++i) {
            merge_mesh(merged, generate_tube(paths[i], radii[i], segments,
                                             colors[i], caps, caps));
        }
        Mesh batched = generate_multi_tubes(paths, radii, colors, segments, caps);

        REQUIRE(batched.vertices.size() == merged.vertices.size());
        REQUIRE(batched.triangles.size() == merged.triangles.size());
        for (size_t i = 0; i < merged.vertices.size(); ++i) {
            REQUIRE(batched.vertices[i] == merged.vertices[i]);
        }
        for (size_t i = 0; i < merged.triangles.size(); ++i) {
            REQUIRE(batched.triangles[i].v0 == merged.triangles[i].v0);
            REQUIRE(batched.triangles[i].v1 == merged.triangles[i].v1);
            REQUIRE(batched.triangles[i].v2 == merged.triangles[i].v2);
        }
        REQUIRE(batched.is_valid());
    }
}

TEST_CASE("generate_multi_tubes skips degenerate paths",
          "[primitives][tube][batch]") {
    std::vector<std::vector<Vec3>> paths = {
        {{0, 0, 0}, {0, 0, 0}},          // no direction
        {{0, 0, 0}, {0, 0, 1}},          // fine
        {{5, 5, 5}},                     // single point
        {}};                             // empty
    std::vector<float> radii = {0.1f, 0.1f, 0.1f, 0.1f};
    std::vector<Color> colors = {Color(1, 1, 1), Color(1, 1, 1), Color(1, 1, 1),
                                 Color(1, 1, 1)};

    Mesh batched = generate_multi_tubes(paths, radii, colors, 8, false);
    Mesh single = generate_tube({{0, 0, 0}, {0, 0, 1}}, 0.1f, 8,
                                Color(1, 1, 1), false, false);

    REQUIRE(batched.vertices.size() == single.vertices.size());
    REQUIRE(batched.triangles.size() == single.triangles.size());

    std::vector<std::vector<Vec3>> all_degenerate = {{{1, 1, 1}}, {}};
    REQUIRE(generate_multi_tubes(all_degenerate, {}, {}, 8, false).empty());

    // Missing radii/colors are recycled (and an empty array means "use the
    // default"), so short or empty arrays must never read out of bounds.
    Mesh short_arrays = generate_multi_tubes(paths, {0.1f}, {Color(1, 0, 0)}, 8, false);
    REQUIRE(short_arrays.vertices.size() == batched.vertices.size());
    Mesh empty_arrays = generate_multi_tubes(paths, {}, {}, 8, false);
    REQUIRE(empty_arrays.vertices.size() == batched.vertices.size());
    REQUIRE(empty_arrays.is_valid());
}
