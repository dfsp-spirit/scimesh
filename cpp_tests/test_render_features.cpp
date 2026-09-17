// Optional renderer features driven through the public API.
//
// Texture mapping, wireframe, transparency/blending, contrast, SSAO, fog in
// both coordinate spaces, point clouds, degenerate inputs and the camera
// helpers.  These paths are all reachable from the R layer, but were largely
// untested on the C++ side.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/mesh.h>
#include <scimesh/image.h>
#include <scimesh/primitives.h>
#include <scimesh/transforms.h>
#include <scimesh/types.h>
#include <cmath>
#include <stdexcept>
#include <vector>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using scimesh_test::make_colored_cube;
using scimesh_test::make_plane;
using scimesh_test::make_icosphere;
using Catch::Approx;

namespace {

const Color BLACK(0.0f, 0.0f, 0.0f, 1.0f);

Camera front_camera(float distance = 4.0f) {
    Camera cam;
    cam.eye = Vec3(0, 0, distance);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);
    cam.fov_degrees = 45.0f;
    return cam;
}

RenderOptions base_options(int size = 64) {
    RenderOptions opts;
    opts.width = size;
    opts.height = size;
    opts.background_color = BLACK;
    opts.threads = 1;
    return opts;
}

Color pixel(const Image &img, int x, int y) {
    uint8_t r, g, b, a;
    img.get_pixel(x, y, r, g, b, a);
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

Color center_pixel(const Image &img) {
    return pixel(img, img.width / 2, img.height / 2);
}

int count_foreground(const Image &img, const Color &background = BLACK) {
    int n = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const Color c = pixel(img, x, y);
            if (std::abs(c.r - background.r) > 0.02f ||
                std::abs(c.g - background.g) > 0.02f ||
                std::abs(c.b - background.b) > 0.02f) {
                ++n;
            }
        }
    }
    return n;
}

int count_differing_pixels(const Image &a, const Image &b) {
    int n = 0;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            const Color ca = pixel(a, x, y);
            const Color cb = pixel(b, x, y);
            if (std::abs(ca.r - cb.r) > 0.02f || std::abs(ca.g - cb.g) > 0.02f ||
                std::abs(ca.b - cb.b) > 0.02f) {
                ++n;
            }
        }
    }
    return n;
}

} // namespace

TEST_CASE("Renderer validates the output size", "[render][errors]") {
    Renderer renderer;
    const Mesh cube = make_unit_cube();

    RenderOptions too_small = base_options();
    too_small.width = 0;
    REQUIRE_THROWS_AS(renderer.render_mesh(cube, front_camera(), too_small),
                      std::invalid_argument);

    RenderOptions negative_height = base_options();
    negative_height.height = -5;
    Scene scene;
    scene.meshes.push_back(cube);
    REQUIRE_THROWS_AS(renderer.render_scene(scene, front_camera(), negative_height),
                      std::invalid_argument);
}

TEST_CASE("Renderer handles empty scenes and empty meshes", "[render][edges]") {
    Renderer renderer;
    const RenderOptions opts = base_options(16);

    const Image empty_scene = renderer.render_scene(Scene{}, front_camera(), opts);
    REQUIRE(count_foreground(empty_scene) == 0);

    Mesh empty_mesh;
    const Image empty_mesh_image = renderer.render_mesh(empty_mesh, front_camera(), opts);
    REQUIRE(count_foreground(empty_mesh_image) == 0);

    // A scene mixing an empty and a valid mesh draws only the valid one.
    Scene mixed;
    mixed.meshes.push_back(Mesh{});
    mixed.meshes.push_back(make_colored_cube());
    const Image mixed_image = renderer.render_scene(mixed, front_camera(), opts);
    REQUIRE(count_foreground(mixed_image) > 0);
}

TEST_CASE("Renderer maps a texture onto a mesh with UV coordinates",
          "[render][texture]") {
    Renderer renderer;

    Mesh plain = make_colored_cube();
    const Image untextured = renderer.render_mesh(plain, front_camera(), base_options());

    // A single green texel makes the expected result exact: the (grey) vertex
    // colour is modulated by the sampled texel.
    Image texture(1, 1);
    texture.set_pixel(0, 0, 0, 255, 0, 255);

    Mesh textured = make_colored_cube();
    textured.uvs.assign(textured.vertices.size(), Vec2(0.5f, 0.5f));
    textured.texture = texture;
    REQUIRE(textured.has_uvs());
    REQUIRE(textured.has_texture());

    const Image with_texture = renderer.render_mesh(textured, front_camera(), base_options());
    REQUIRE(count_foreground(with_texture) > 0);
    REQUIRE(count_differing_pixels(untextured, with_texture) > 0);

    const Color front = center_pixel(with_texture);
    REQUIRE(front.g > front.r);
    REQUIRE(front.g > front.b);
}

TEST_CASE("Renderer samples mesh UVs in image space (v = 0 is the top)",
          "[render][texture]") {
    Renderer renderer;

    // A big quad facing the camera, with a white base color so that the texture
    // shows through (the renderer modulates the vertex color with the texel).
    Mesh quad;
    quad.vertices = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    quad.triangles = {{0, 1, 2}, {0, 2, 3}};
    quad.normals.assign(4, Vec3(0, 0, 1));
    quad.colors.assign(4, Color(1, 1, 1, 1));

    // A 1x2 texture: row 0 is red, row 1 is blue.  Row 0 is the *first* row of
    // the pixel buffer, i.e. the top row of the image.
    Image texture(1, 2);
    texture.set_pixel(0, 0, 255, 0, 0, 255);  // top of the image    -> red
    texture.set_pixel(0, 1, 0, 0, 255, 255);  // bottom of the image -> blue
    quad.texture = texture;

    // scimesh UVs are image-space: v = 0 addresses the first (top) row of the
    // texture.  The quad's lower vertices therefore get v = 1 (the blue row)
    // and its upper vertices v = 0 (the red row), so the rendered quad shows
    // red on top — the opposite of what the OBJ/OpenGL convention would do.
    quad.uvs = {Vec2(0.0f, 1.0f), Vec2(1.0f, 1.0f),   // lower left, lower right
                Vec2(1.0f, 0.0f), Vec2(0.0f, 0.0f)};  // upper right, upper left

    const auto mean_top_bottom = [&](const Image &img) {
        // Mean red/blue of the top and the bottom fifth of the drawn geometry.
        double top_r = 0, top_b = 0, bottom_r = 0, bottom_b = 0;
        int top_n = 0, bottom_n = 0;
        int min_y = img.height, max_y = -1;
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const Color c = pixel(img, x, y);
                if (c.r + c.g + c.b > 0.05f) {
                    min_y = std::min(min_y, y);
                    max_y = std::max(max_y, y);
                }
            }
        }
        REQUIRE(max_y >= 0);
        const int band = std::max(1, (max_y - min_y + 1) / 5);
        for (int y = min_y; y <= min_y + band && y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const Color c = pixel(img, x, y);
                if (c.r + c.g + c.b > 0.05f) {
                    top_r += c.r;
                    top_b += c.b;
                    ++top_n;
                }
            }
        }
        for (int y = std::max(min_y, max_y - band); y <= max_y; ++y) {
            for (int x = 0; x < img.width; ++x) {
                const Color c = pixel(img, x, y);
                if (c.r + c.g + c.b > 0.05f) {
                    bottom_r += c.r;
                    bottom_b += c.b;
                    ++bottom_n;
                }
            }
        }
        REQUIRE(top_n > 0);
        REQUIRE(bottom_n > 0);
        return std::vector<double>{top_r / top_n, top_b / top_n,
                                   bottom_r / bottom_n, bottom_b / bottom_n};
    };

    const Image rendered =
        renderer.render_mesh(quad, front_camera(4.0f), base_options(96));
    const std::vector<double> bands = mean_top_bottom(rendered);
    REQUIRE(bands[0] > bands[1]);  // top of the quad is red
    REQUIRE(bands[3] > bands[2]);  // bottom of the quad is blue

    // flip_uvs() exists for UVs that use the bottom-left origin (OBJ, PLY,
    // OpenGL, rgl, Blender): it converts them to the image space used above, so
    // flipping the quad's UVs mirrors the texture on the quad.
    Mesh mirrored = quad;
    flip_uvs(mirrored);
    REQUIRE(mirrored.uvs[0] == Vec2(0.0f, 0.0f));
    REQUIRE(mirrored.uvs[2] == Vec2(1.0f, 1.0f));

    const Image flipped_image =
        renderer.render_mesh(mirrored, front_camera(4.0f), base_options(96));
    const std::vector<double> flipped_bands = mean_top_bottom(flipped_image);
    REQUIRE(flipped_bands[1] > flipped_bands[0]);  // top of the quad is blue
    REQUIRE(flipped_bands[2] > flipped_bands[3]);  // bottom of the quad is red
}

TEST_CASE("Renderer wireframe mode draws edges instead of fills",
          "[render][wireframe]") {
    Renderer renderer;
    const Mesh sphere = make_icosphere(1);

    const Image solid = renderer.render_mesh(sphere, front_camera(), base_options());
    REQUIRE(count_foreground(solid) > 0);

    RenderOptions wire = base_options();
    wire.wireframe = true;
    wire.wireframe_color = Color(1.0f, 0.0f, 0.0f, 1.0f);
    const Image edges = renderer.render_mesh(sphere, front_camera(), wire);

    // Only the edges are drawn, so much less of the image is covered ...
    REQUIRE(count_foreground(edges) > 0);
    REQUIRE(count_foreground(edges) < count_foreground(solid));

    // ... and those pixels use the wire colour.
    bool found_wire_pixel = false;
    for (int y = 0; y < edges.height && !found_wire_pixel; ++y) {
        for (int x = 0; x < edges.width && !found_wire_pixel; ++x) {
            const Color c = pixel(edges, x, y);
            if (c.r > 0.9f && c.g < 0.1f && c.b < 0.1f) found_wire_pixel = true;
        }
    }
    REQUIRE(found_wire_pixel);
}

TEST_CASE("Renderer blends semi-transparent meshes over what is behind them",
          "[render][transparency]") {
    Renderer renderer;

    // Blue backdrop plane at z = -1, semi-transparent red plane at z = 0.
    Mesh backdrop = make_plane();
    backdrop.colors.assign(backdrop.vertices.size(), Color(0, 0, 1, 1));
    translate_mesh(backdrop, Vec3(0, 0, -1));

    Mesh overlay = make_plane();
    overlay.colors.assign(overlay.vertices.size(), Color(1, 0, 0, 0.5f));
    overlay.has_transparency = true;

    Scene scene;
    scene.meshes.push_back(backdrop);
    scene.meshes.push_back(overlay);

    const Image img = renderer.render_scene(scene, front_camera(), base_options());
    const Color mid = center_pixel(img);
    // Both contributions must be visible in the blended pixel.
    REQUIRE(mid.r > 0.2f);
    REQUIRE(mid.b > 0.2f);
}

TEST_CASE("Renderer applies the contrast adjustment", "[render][contrast]") {
    Renderer renderer;

    // A dark grey plane, fully lit (ambient 0.3 + diffuse 0.7 = 1.0).  The
    // colour has to come from the render options: they take precedence over
    // Mesh::default_color when a mesh carries no vertex colours.
    const Mesh plane = make_plane();

    RenderOptions plain = base_options();
    plain.background_color = BLACK;
    plain.default_color = Color(0.3f, 0.3f, 0.3f, 1.0f);
    const Color before = center_pixel(renderer.render_mesh(plane, front_camera(), plain));
    REQUIRE(before.r == Approx(0.3f).margin(0.05f));

    RenderOptions punchy = plain;
    punchy.contrast = 2.0f;
    const Color after = center_pixel(renderer.render_mesh(plane, front_camera(), punchy));

    // (0.3 - 0.5) * 2 + 0.5 = 0.1 - darker than the 0.3 baseline.
    REQUIRE(after.r < before.r);
    REQUIRE(after.r == Approx(0.1f).margin(0.05f));
}

TEST_CASE("Renderer applies screen-space ambient occlusion", "[render][ssao]") {
    Renderer renderer;

    // SSAO darkens where a nearby fragment is *closer* to the camera - the
    // classic case is the contact shadow an object casts on the surface behind
    // it.  A flat wall with a small cube in front of it therefore has a
    // measurable effect (a lone head-on cube does not: nothing is closer there).
    Mesh wall = make_plane();
    scale_mesh(wall, 4.0f);
    translate_mesh(wall, Vec3(0.0f, 0.0f, -1.0f));

    Mesh box = make_unit_cube();
    scale_mesh(box, 0.4f);

    Scene scene;
    scene.meshes.push_back(wall);
    scene.meshes.push_back(box);

    RenderOptions base = base_options(64);
    base.default_color = Color(0.7f, 0.7f, 0.7f, 1.0f);
    base.near_plane = 0.5f;
    base.far_plane = 20.0f;
    const Image plain = renderer.render_scene(scene, front_camera(4.0f), base);
    REQUIRE(count_foreground(plain) > 0);

    RenderOptions ssao = base;
    ssao.ssao_enabled = true;
    ssao.ssao_radius = 8;      // pixels
    ssao.ssao_intensity = 1.0f;
    const Image occluded = renderer.render_scene(scene, front_camera(4.0f), ssao);

    REQUIRE(count_foreground(occluded) > 0);
    REQUIRE(count_differing_pixels(plain, occluded) > 0);

    // Disabled SSAO changes nothing.
    RenderOptions off = base;
    off.ssao_enabled = false;
    REQUIRE(count_differing_pixels(plain, renderer.render_scene(scene, front_camera(4.0f), off)) == 0);

    // With an orthographic projection the depth mapping is linear; the pass must
    // invert it as such (and stay in its bounds).
    RenderOptions ortho = ssao;
    ortho.projection = ProjectionType::ORTHOGRAPHIC;
    Camera ortho_cam = front_camera(6.0f);
    ortho_cam.projection = ProjectionType::ORTHOGRAPHIC;
    const Image ortho_occluded = renderer.render_scene(scene, ortho_cam, ortho);
    REQUIRE(count_foreground(ortho_occluded) > 0);
}

TEST_CASE("Fog works in world units and in normalized device depth",
          "[render][fog]") {
    Renderer renderer;
    const Mesh plane = make_plane();

    RenderOptions base = base_options();
    base.default_color = Color(1.0f, 1.0f, 1.0f, 1.0f);  // white plane

    RenderOptions world_fog = base;
    world_fog.fog_enabled = true;
    world_fog.fog_space = FogSpace::WORLD;
    world_fog.fog_start = 0.1f;
    world_fog.fog_end = 0.2f;  // the plane is 4 units away -> fully fogged
    world_fog.fog_color = Color(0, 1, 0, 1);
    const Image world = renderer.render_mesh(plane, front_camera(), world_fog);
    REQUIRE(center_pixel(world).g > 0.9f);
    REQUIRE(center_pixel(world).r < 0.1f);

    // The same scene with an NDC fog band near the far plane is *not* fogged:
    // the plane sits at a depth-buffer value of about 0.95, which is outside
    // [0.99, 1.0].  That is the whole point of the two spaces - the same numbers
    // mean different things.
    RenderOptions ndc_fog = base;
    ndc_fog.fog_enabled = true;
    ndc_fog.fog_space = FogSpace::NDC;
    ndc_fog.fog_start = 0.99f;
    ndc_fog.fog_end = 1.0f;
    ndc_fog.fog_color = Color(0, 1, 0, 1);
    const Image ndc = renderer.render_mesh(plane, front_camera(), ndc_fog);
    REQUIRE(count_differing_pixels(world, ndc) > 0);
    REQUIRE(center_pixel(ndc).r > 0.9f);  // still the white plane
    REQUIRE(center_pixel(ndc).b > 0.9f);

    // An empty range must not divide by zero.
    RenderOptions degenerate = base;
    degenerate.fog_enabled = true;
    degenerate.fog_start = 1.0f;
    degenerate.fog_end = 1.0f;
    degenerate.fog_color = Color(0, 1, 0, 1);
    const Image step = renderer.render_mesh(plane, front_camera(), degenerate);
    REQUIRE(count_foreground(step) > 0);
}

TEST_CASE("Renderer can invert normals", "[render][normals]") {
    Renderer renderer;
    const Mesh sphere = make_icosphere(1);

    const Image plain = renderer.render_mesh(sphere, front_camera(), base_options());

    RenderOptions inverted = base_options();
    inverted.invert_normals = true;
    const Image flipped = renderer.render_mesh(sphere, front_camera(), inverted);

    REQUIRE(count_differing_pixels(plain, flipped) > 0);
}

TEST_CASE("Renderer draws point clouds", "[render][points]") {
    Renderer renderer;

    const std::vector<Vec3> points = {
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 0.5f, 0.0f),
        Vec3(-1.0f, -0.5f, 0.0f)};
    const std::vector<Color> colors = {
        Color(1, 0, 0, 1), Color(0, 1, 0, 1), Color(0, 0, 1, 1)};

    const Image img = renderer.render_points_raw(points, colors, 4.0f,
                                                 front_camera(), base_options());
    REQUIRE(count_foreground(img) > 0);
    REQUIRE(center_pixel(img).r > 0.5f);  // the point at the origin

    // An empty cloud renders an empty image, and degenerates do not crash.
    REQUIRE(count_foreground(renderer.render_points_raw({}, {}, 4.0f,
                                                        front_camera(),
                                                        base_options())) == 0);
    REQUIRE(count_foreground(renderer.render_points_raw({Vec3(1e6f, 1e6f, 1e6f)},
                                                        {Color(1, 1, 1, 1)}, 3.0f,
                                                        front_camera(),
                                                        base_options())) == 0);
}

TEST_CASE("Camera helpers behave for orbits and invalid parameters",
          "[render][camera]") {
    // Invalid projection parameters must be reported, not silently accepted.
    Camera cam;
    REQUIRE_THROWS_AS(cam.get_projection_matrix(0.0f, 0.1f, 100.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(cam.get_projection_matrix(1.5f, 0.0f, 100.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(cam.get_projection_matrix(1.5f, 10.0f, 5.0f), std::invalid_argument);

    // camera_orbit() moves the eye around the center and keeps the distance.
    Camera start = front_camera(5.0f);
    const Camera orbited = camera_orbit(start, Vec3(0, 1, 0), 90.0f);
    REQUIRE(glm::length(orbited.eye - orbited.center) ==
            Approx(glm::length(start.eye - start.center)).margin(1e-4f));
    REQUIRE(orbited.eye.x == Approx(5.0f).margin(1e-3f));
    REQUIRE(orbited.center.x == Approx(start.center.x));
    REQUIRE(glm::length(orbited.up) == Approx(1.0f));
    REQUIRE(count_differing_pixels(Renderer().render_mesh(make_colored_cube(), start,
                                                          base_options(32)),
                                   Renderer().render_mesh(make_colored_cube(), orbited,
                                                          base_options(32))) > 0);
}
