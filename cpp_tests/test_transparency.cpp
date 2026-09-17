// Per-vertex / per-face / uniform transparency (alpha blending).
//
// Transparency used to require setting Mesh::has_transparency by hand, and the
// blended pass ignored the depth buffer, so translucent geometry lying *behind*
// opaque geometry was blended on top of it.  Both are covered here.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/mesh.h>
#include <scimesh/image.h>
#include <scimesh/ply_io.h>
#include <scimesh/primitives.h>
#include <scimesh/fs_mesh_converter.h>
#include "libfs.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using Catch::Approx;

namespace {

const Color kWhite(1.0f, 1.0f, 1.0f, 1.0f);
const Color kRed(1.0f, 0.0f, 0.0f, 1.0f);

Camera front_camera(float distance = 4.0f) {
    Camera cam;
    cam.eye = Vec3(0, 0, distance);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);
    cam.fov_degrees = 45.0f;
    return cam;
}

RenderOptions white_options(int size = 64) {
    RenderOptions opts;
    opts.width = size;
    opts.height = size;
    opts.background_color = kWhite;
    opts.backface_culling = false;  // test quads are written in one winding only
    opts.threads = 1;
    return opts;
}

/// A square in the z plane, two triangles, spanning [-half, half] in x and y.
Mesh make_quad(float z, float half = 1.0f) {
    Mesh mesh;
    mesh.vertices = {
        {-half, -half, z}, {half, -half, z}, {half, half, z}, {-half, half, z}
    };
    mesh.triangles = {{0, 1, 2}, {0, 2, 3}};
    return mesh;
}

Mesh with_uniform_color(const Mesh &mesh, const Color &color) {
    Mesh out = mesh;
    out.colors.assign(out.vertices.size(), color);
    out.update_transparency();
    return out;
}

Color pixel(const Image &img, int x, int y) {
    uint8_t r, g, b, a;
    img.get_pixel(x, y, r, g, b, a);
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

Color center_pixel(const Image &img) {
    return pixel(img, img.width / 2, img.height / 2);
}

int count_differing_pixels(const Image &a, const Image &b) {
    REQUIRE(a.width == b.width);
    REQUIRE(a.height == b.height);
    int n = 0;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            const Color ca = pixel(a, x, y);
            const Color cb = pixel(b, x, y);
            if (std::abs(ca.r - cb.r) > 1e-6f || std::abs(ca.g - cb.g) > 1e-6f ||
                std::abs(ca.b - cb.b) > 1e-6f) {
                ++n;
            }
        }
    }
    return n;
}

std::string write_temp_file(const std::string &name, const std::string &content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    return path.string();
}

} // namespace

TEST_CASE("Mesh detects transparency from its colors", "[mesh][transparency]") {
    Mesh cube = make_unit_cube();
    REQUIRE_FALSE(cube.has_alpha());
    REQUIRE_FALSE(cube.is_transparent());

    // Opaque vertex colors: nothing to blend.
    cube = with_uniform_color(cube, kRed);
    REQUIRE_FALSE(cube.has_alpha());
    REQUIRE_FALSE(cube.is_transparent());

    // A single translucent vertex is enough - no manual flag required.
    cube.colors[3].a = 0.5f;
    REQUIRE(cube.has_alpha());
    REQUIRE(cube.is_transparent());
    REQUIRE_FALSE(cube.has_transparency);  // the stored flag is only a cache
    cube.update_transparency();
    REQUIRE(cube.has_transparency);
}

TEST_CASE("Mesh transparency is detected for face colors and default_color",
          "[mesh][transparency]") {
    Mesh faces = make_unit_cube();
    faces.face_colors.assign(faces.triangles.size(), kWhite);
    REQUIRE_FALSE(faces.is_transparent());
    faces.face_colors[0].a = 0.0f;  // e.g. a hole
    REQUIRE(faces.is_transparent());

    // default_color is what a mesh without any colors is drawn with.
    Mesh plain = make_unit_cube();
    plain.default_color = Color(0.7f, 0.7f, 0.7f, 0.4f);
    REQUIRE(plain.has_alpha());
    REQUIRE(plain.is_transparent());

    // A manually requested blend pass (e.g. RGBA textures) is never cleared.
    Mesh manual = make_unit_cube();
    manual.has_transparency = true;
    manual.update_transparency();
    REQUIRE(manual.is_transparent());
}

TEST_CASE("Scene::add refreshes the transparency flag", "[scene][transparency]") {
    Scene scene;
    scene.add(with_uniform_color(make_unit_cube(), kRed));
    scene.add(with_uniform_color(make_quad(0.0f), Color(1, 0, 0, 0.5f)));

    REQUIRE(scene.meshes.size() == 2);
    REQUIRE_FALSE(scene.meshes[0].has_transparency);
    REQUIRE(scene.meshes[1].has_transparency);
}

TEST_CASE("Translucent meshes blend without a manual flag", "[render][transparency]") {
    Renderer renderer;

    Scene opaque_scene;
    opaque_scene.add(with_uniform_color(make_quad(-1.0f, 3.0f), Color(0, 0, 1, 1)));
    opaque_scene.add(with_uniform_color(make_quad(0.0f), kRed));

    Scene blended_scene;
    blended_scene.add(with_uniform_color(make_quad(-1.0f, 3.0f), Color(0, 0, 1, 1)));
    blended_scene.add(with_uniform_color(make_quad(0.0f), Color(1, 0, 0, 0.5f)));

    const Image opaque = renderer.render_scene(opaque_scene, front_camera(), white_options());
    const Image blended = renderer.render_scene(blended_scene, front_camera(), white_options());

    // Half of the red is replaced by the blue plane behind it.
    REQUIRE(count_differing_pixels(opaque, blended) > 0);
    const Color center = center_pixel(blended);
    REQUIRE(center.b > center_pixel(opaque).b);   // blue shows through
    REQUIRE(center.r > 0.1f);                     // red is still there
}

TEST_CASE("A double-sided opaque plane is shaded uniformly",
          "[render][transparency]") {
    // generate_plane() builds a front *and* a back face at exactly the same
    // depth, so the depth test cannot decide between them: which one wins is a
    // floating point tie, and it is broken differently from pixel to pixel and
    // from platform to platform.  Both faces must therefore be shaded the same
    // way.  Shading the back face with its own (away pointing) normal used to
    // leave it ambient-lit, which speckled the plane with dark pixels.
    Renderer renderer;
    Mesh plane = generate_plane(Vec3(0, 0, 0), Vec3(0, 0, 1), 2, 2, kRed);

    Scene scene;
    scene.add(plane);
    const Image img = renderer.render_scene(scene, front_camera(), white_options());

    int covered = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const Color c = pixel(img, x, y);
            if (c.g > 0.5f && c.b > 0.5f) continue;  // white background
            ++covered;
            REQUIRE(c.r > 0.9f);                     // never the 0.3 ambient term
        }
    }
    REQUIRE(covered > 1000);
}

TEST_CASE("A translucent double-sided plane blends uniformly",
          "[render][transparency]") {
    // The same double-sided plane, but translucent: each of its two coplanar
    // faces contributes one blended layer, and each face consists of two
    // triangles that share a diagonal.  A pixel sitting exactly on such a
    // diagonal must be rasterized by exactly one of the two triangles -
    // otherwise it is blended twice (a dark seam) or not at all (a crack),
    // depending on the rounding of the platform (see the fill rule in
    // Rasterizer::rasterize_triangle()).  So the plane must attenuate the
    // background by exactly 0.7^2 everywhere, on every pixel.
    Renderer renderer;
    Mesh plane = generate_plane(Vec3(0, 0, 0), Vec3(0, 0, 1), 2, 2,
                                Color(1, 0, 0, 0.3f));

    Scene scene;
    scene.add(plane);
    const Image img = renderer.render_scene(scene, front_camera(), white_options());

    const float expected_green = 0.7f * 0.7f;
    int covered = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const Color c = pixel(img, x, y);
            if (c.g > 0.9f) continue;                // white background
            ++covered;
            REQUIRE(std::abs(c.g - expected_green) < 0.02f);
        }
    }
    REQUIRE(covered > 1000);
}

TEST_CASE("Translucent geometry behind an opaque mesh stays hidden",
          "[render][transparency]") {
    Renderer renderer;

    Scene cube_only;
    cube_only.add(with_uniform_color(make_unit_cube(), kRed));

    Scene cube_and_quad;
    cube_and_quad.add(with_uniform_color(make_unit_cube(), kRed));
    // A small 50% transparent quad two units behind the cube: it is completely
    // hidden by it, so it must not brighten a single pixel.
    cube_and_quad.add(with_uniform_color(make_quad(-3.0f, 0.5f), Color(0, 1, 0, 0.5f)));

    const Image without = renderer.render_scene(cube_only, front_camera(), white_options());
    const Image with = renderer.render_scene(cube_and_quad, front_camera(), white_options());

    REQUIRE(count_differing_pixels(without, with) == 0);

    // The same quad in front of the cube does change the image.
    Scene quad_in_front;
    quad_in_front.add(with_uniform_color(make_unit_cube(), kRed));
    quad_in_front.add(with_uniform_color(make_quad(3.0f, 0.5f), Color(0, 1, 0, 0.5f)));
    REQUIRE(count_differing_pixels(without,
                                   renderer.render_scene(quad_in_front, front_camera(),
                                                         white_options())) > 0);
}

TEST_CASE("Translucent triangles are blended back to front", "[render][transparency]") {
    Renderer renderer;

    const Mesh near_red = with_uniform_color(make_quad(1.0f, 2.0f), Color(1, 0, 0, 0.5f));
    const Mesh far_green = with_uniform_color(make_quad(0.0f, 2.0f), Color(0, 1, 0, 0.5f));

    Scene red_front;
    red_front.add(far_green);
    red_front.add(near_red);

    // Same two quads, added in the opposite order: the renderer sorts the
    // translucent triangles itself, so the result must be identical.
    Scene green_front;
    green_front.add(near_red);
    green_front.add(far_green);

    const Image a = renderer.render_scene(red_front, front_camera(), white_options());
    const Image b = renderer.render_scene(green_front, front_camera(), white_options());

    REQUIRE(count_differing_pixels(a, b) == 0);
    // ... and the nearer surface contributes more than the one behind it.
    REQUIRE(center_pixel(a).r > center_pixel(a).g);

    // Swapping which quad is nearer flips that, so the sort really is depth
    // based and not a fixed draw order.
    Scene green_nearer;
    green_nearer.add(with_uniform_color(make_quad(1.0f, 2.0f), Color(0, 1, 0, 0.5f)));
    green_nearer.add(with_uniform_color(make_quad(0.0f, 2.0f), Color(1, 0, 0, 0.5f)));
    const Image c = renderer.render_scene(green_nearer, front_camera(), white_options());
    REQUIRE(count_differing_pixels(a, c) > 0);
    REQUIRE(center_pixel(c).g > center_pixel(c).r);
}

TEST_CASE("Per-vertex alpha is interpolated across a triangle", "[render][transparency]") {
    Renderer renderer;

    // Red quad, alpha 0 at x = -1 and 1 at x = +1, over a white background:
    // the green channel (background show-through) must fall off smoothly.
    Mesh quad = make_quad(0.0f, 1.0f);
    quad.colors = {
        {1, 0, 0, 0.0f}, {1, 0, 0, 1.0f}, {1, 0, 0, 1.0f}, {1, 0, 0, 0.0f}
    };
    quad.update_transparency();

    Scene scene;
    scene.add(quad);
    const Image img = renderer.render_scene(scene, front_camera(), white_options(128));

    const int y = img.height / 2;
    std::vector<float> green;
    for (int x = img.width / 2 - 8; x <= img.width / 2 + 8; x += 4) {
        green.push_back(pixel(img, x, y).g);
    }
    REQUIRE(green.front() > green.back() + 0.2f);
    for (size_t i = 1; i < green.size(); ++i) {
        REQUIRE(green[i] <= green[i - 1] + 1e-6f);
    }
}

TEST_CASE("Fully transparent geometry is invisible", "[render][transparency]") {
    Renderer renderer;

    RenderOptions opts = white_options();
    const Image empty = renderer.render_mesh(Mesh(), front_camera(), opts);

    Scene scene;
    scene.add(with_uniform_color(make_quad(0.0f, 3.0f), Color(1, 1, 1, 0.0f)));
    const Image holes = renderer.render_scene(scene, front_camera(), opts);

    REQUIRE(count_differing_pixels(empty, holes) == 0);
}

TEST_CASE("Alpha values survive a PLY round-trip", "[io][transparency]") {
    // 8-bit RGBA (the usual case) ...
    const std::string uchar_file = write_temp_file("scimesh_test_rgba_uchar.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 4\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "property uchar alpha\n"
        "element face 2\n"
        "property list uchar int vertex_indices\n"
        "end_header\n"
        "-1 -1 0 255 0 0 128\n"
        "1 -1 0 255 0 0 128\n"
        "1 1 0 255 0 0 128\n"
        "-1 1 0 255 0 0 128\n"
        "3 0 1 2\n"
        "3 0 2 3\n");

    Mesh mesh = ply_io::read_ply(uchar_file);
    std::filesystem::remove(uchar_file);
    REQUIRE(mesh.colors.size() == 4);
    REQUIRE(mesh.colors[0].r == Approx(1.0f).margin(1e-6f));
    REQUIRE(mesh.colors[0].a == Approx(128.0f / 255.0f).margin(1e-6f));
    REQUIRE(mesh.has_transparency);   // self-contained: no manual flagging
    REQUIRE(mesh.is_transparent());

    // ... and RGB as uchar with a *float* alpha, which tinyply cannot read in a
    // single request (mixed property types).
    const std::string float_file = write_temp_file("scimesh_test_rgba_float.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 4\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "property float alpha\n"
        "element face 2\n"
        "property list uchar int vertex_indices\n"
        "end_header\n"
        "-1 -1 0 0 255 0 0.25\n"
        "1 -1 0 0 255 0 0.25\n"
        "1 1 0 0 255 0 0.25\n"
        "-1 1 0 0 255 0 0.25\n"
        "3 0 1 2\n"
        "3 0 2 3\n");

    Mesh float_mesh = ply_io::read_ply(float_file);
    std::filesystem::remove(float_file);
    REQUIRE(float_mesh.colors[0].g == Approx(1.0f).margin(1e-6f));
    REQUIRE(float_mesh.colors[0].a == Approx(0.25f).margin(1e-6f));
    REQUIRE(float_mesh.is_transparent());
}

TEST_CASE("PLY alpha of other integer types and alpha-only files",
          "[io][transparency]") {
    // 16-bit alpha ...
    const std::string ushort_file = write_temp_file("scimesh_test_rgba_ushort.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 3\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "property ushort alpha\n"
        "element face 1\n"
        "property list uchar int vertex_indices\n"
        "end_header\n"
        "0 0 0 255 255 255 32768\n"
        "1 0 0 255 255 255 32768\n"
        "0 1 0 255 255 255 32768\n"
        "3 0 1 2\n");
    Mesh ushort_mesh = ply_io::read_ply(ushort_file);
    std::filesystem::remove(ushort_file);
    REQUIRE(ushort_mesh.colors[0].a == Approx(32768.0f / 65535.0f).margin(1e-4f));
    REQUIRE(ushort_mesh.is_transparent());

    // ... signed 8-bit alpha ...
    const std::string char_file = write_temp_file("scimesh_test_rgba_char.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 3\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "property uchar green\n"
        "property uchar blue\n"
        "property char alpha\n"
        "element face 1\n"
        "property list uchar int vertex_indices\n"
        "end_header\n"
        "0 0 0 255 255 255 64\n"
        "1 0 0 255 255 255 64\n"
        "0 1 0 255 255 255 64\n"
        "3 0 1 2\n");
    Mesh char_mesh = ply_io::read_ply(char_file);
    std::filesystem::remove(char_file);
    REQUIRE(char_mesh.colors[0].a == Approx(64.0f / 255.0f).margin(1e-6f));

    // ... and a file that carries alpha without any color at all.
    const std::string alpha_only_file = write_temp_file("scimesh_test_alpha_only.ply",
        "ply\n"
        "format ascii 1.0\n"
        "element vertex 3\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property float alpha\n"
        "element face 1\n"
        "property list uchar int vertex_indices\n"
        "end_header\n"
        "0 0 0 0.5\n"
        "1 0 0 0.5\n"
        "0 1 0 0.5\n"
        "3 0 1 2\n");
    Mesh alpha_only = ply_io::read_ply(alpha_only_file);
    std::filesystem::remove(alpha_only_file);
    REQUIRE(alpha_only.colors.size() == 3);
    REQUIRE(alpha_only.colors[0].r == Approx(1.0f));   // white fallback
    REQUIRE(alpha_only.colors[0].a == Approx(0.5f).margin(1e-6f));
    REQUIRE(alpha_only.is_transparent());
}

TEST_CASE("FreeSurfer converter can render the medial wall translucent",
          "[fs][transparency]") {
    fs::Mesh fs_mesh;
    fs_mesh.vertices = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0};
    fs_mesh.faces = {0, 1, 2, 1, 3, 2};
    const std::vector<uint8_t> rgb(12, 200);
    const std::vector<float> morph = {0.1f,
                                      std::numeric_limits<float>::quiet_NaN(),
                                      0.2f, 0.3f};

    // Default: NaN vertices stay opaque white, as before.
    Mesh opaque = convert_fs_mesh(fs_mesh, morph, rgb);
    REQUIRE(opaque.colors.size() == 4);
    REQUIRE_FALSE(opaque.is_transparent());
    REQUIRE(opaque.colors[1].a == Approx(1.0f));

    // nan_alpha = 0.5 makes the medial wall translucent, nan_alpha = 0 holes it.
    Mesh half = convert_fs_mesh(fs_mesh, morph, rgb, 0.5f);
    REQUIRE(half.colors[1].a == Approx(0.5f));
    REQUIRE(half.colors[0].a == Approx(1.0f));
    REQUIRE(half.has_transparency);

    Mesh holes = convert_fs_mesh(fs_mesh, morph, rgb, 0.0f);
    REQUIRE(holes.colors[1].a == Approx(0.0f));
    REQUIRE(holes.is_transparent());

    // A solid-color conversion of a translucent color is detected as well.
    Mesh solid = convert_fs_mesh(fs_mesh, Color(0.5f, 0.5f, 0.5f, 0.2f));
    REQUIRE(solid.is_transparent());
    REQUIRE(solid.has_transparency);
}
