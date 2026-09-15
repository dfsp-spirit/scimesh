// Tests for user clip planes (RenderOptions::clip_planes): the coordinate
// space handling (ClipPlane::space, world vs eye) and the view-space
// conversion done by clip_plane_to_view_space().
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/clipping.h>
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/image.h>
#include <scimesh/math_utils.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

using namespace scimesh;
using scimesh_test::make_colored_cube;
using Catch::Approx;

namespace {

constexpr int IMG_W = 200;
constexpr int IMG_H = 200;
// Camera sits at (0, 10, eye_z) and looks straight down -Y, with up = -Z,
// using an orthographic projection.  Ortho uses half_height == distance, so
// image rows map linearly onto world z.  That makes the clip plane's cut
// position directly measurable in world units.
constexpr float CAM_DIST = 10.0f;

// World-space z for the center of image row `row`.
float row_to_world_z(int row, float eye_z) {
    float ndc_y = 1.0f - 2.0f * (static_cast<float>(row) + 0.5f) / IMG_H;
    return eye_z - ndc_y * CAM_DIST;
}

struct ZSpan {
    bool saw_foreground = false;
    float zmin = 0.0f;
    float zmax = 0.0f;
    int count = 0;
};

// Measure which part of the scene is visible, in world z, by scanning the
// rendered image for non-background pixels (background is white).
ZSpan measure_visible_z(const Image &img, float eye_z) {
    ZSpan span;
    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            uint8_t r, g, b, a;
            img.get_pixel(col, row, r, g, b, a);
            if (r > 250 && g > 250 && b > 250) continue;  // background
            float z = row_to_world_z(row, eye_z);
            if (!span.saw_foreground) {
                span.saw_foreground = true;
                span.zmin = span.zmax = z;
            } else {
                span.zmin = std::min(span.zmin, z);
                span.zmax = std::max(span.zmax, z);
            }
            span.count++;
        }
    }
    return span;
}

Image render_cube(const std::vector<ClipPlane> &planes, float eye_z) {
    Renderer renderer;
    RenderOptions opts;
    opts.width = IMG_W;
    opts.height = IMG_H;
    opts.backface_culling = false;
    opts.projection = ProjectionType::ORTHOGRAPHIC;
    opts.clip_planes = planes;

    Camera cam;
    cam.eye = Vec3(0.0f, CAM_DIST, eye_z);
    cam.center = Vec3(0.0f, 0.0f, eye_z);
    cam.up = Vec3(0.0f, 0.0f, -1.0f);
    cam.projection = ProjectionType::ORTHOGRAPHIC;

    Scene scene;
    scene.meshes.push_back(make_colored_cube());
    return renderer.render_scene(scene, cam, opts);
}

} // namespace

// ---------------------------------------------------------------------------
//  ClipPlane defaults and the view-space conversion
// ---------------------------------------------------------------------------

TEST_CASE("ClipPlane defaults to world space", "[clipping][clip_plane_space]") {
    ClipPlane plane{Vec3(0, 0, 1), 0.0f};
    REQUIRE(plane.space == PlaneSpace::WORLD);
    REQUIRE(RenderOptions().clip_planes.empty());
}

TEST_CASE("clip_plane_to_view_space() keeps world planes fixed in the world",
          "[clipping][clip_plane_space]") {
    const Vec3 eye(0.0f, 0.0f, 4.0f);
    Mat4 view = glm::lookAt(eye, Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f));

    ClipPlane world_plane{Vec3(0, 0, 1), 0.0f};  // default space: WORLD
    ClipPlane view_plane = clip_plane_to_view_space(world_plane, eye, view);

    // dot(n, eye) + offset == 4 for the camera at z = 4.
    REQUIRE(view_plane.offset == Approx(4.0f));
    REQUIRE(glm::length(view_plane.normal) == Approx(1.0f));

    // The converted plane must classify points exactly like the world-space
    // plane does.
    for (float z : {-5.0f, -1.0f, 0.0f, 2.0f, 6.0f}) {
        Vec3 p(0.3f, -0.7f, z);
        Vec3 p_view = transform_point(view, p);
        bool kept_world = glm::dot(world_plane.normal, p) + world_plane.offset >= 0.0f;
        bool kept_view = glm::dot(p_view, view_plane.normal) + view_plane.offset >= 0.0f;
        REQUIRE(kept_view == kept_world);
    }
}

TEST_CASE("clip_plane_to_view_space() leaves eye planes camera-relative",
          "[clipping][clip_plane_space]") {
    const Vec3 eye(0.0f, 0.0f, 4.0f);
    Mat4 view = glm::lookAt(eye, Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f));

    // Keep only geometry at least 2 units in front of the camera.
    ClipPlane eye_plane{Vec3(0, 0, -1), -2.0f, PlaneSpace::EYE};
    ClipPlane view_plane = clip_plane_to_view_space(eye_plane, eye, view);

    REQUIRE(view_plane.offset == Approx(-2.0f));  // unchanged
    REQUIRE(view_plane.space == PlaneSpace::EYE);

    // Camera at z = 4 looks toward -z: world z = 3 is 1 unit in front of it
    // (clipped), world z = 1 is 3 units in front (kept).
    Vec3 one_unit = transform_point(view, Vec3(0.0f, 0.0f, 3.0f));
    Vec3 three_units = transform_point(view, Vec3(0.0f, 0.0f, 1.0f));
    REQUIRE_FALSE(glm::dot(one_unit, view_plane.normal) + view_plane.offset >= 0.0f);
    REQUIRE(glm::dot(three_units, view_plane.normal) + view_plane.offset >= 0.0f);
}

TEST_CASE("clip_plane_to_view_space() normalizes normals and ignores zero normals",
          "[clipping][clip_plane_space]") {
    Mat4 identity(1.0f);
    const Vec3 origin(0.0f);

    // Scaling the normal must not move the plane: the offset is a distance.
    ClipPlane scaled{Vec3(0, 0, 10), 2.0f};
    ClipPlane scaled_view = clip_plane_to_view_space(scaled, origin, identity);
    REQUIRE(scaled_view.normal.z == Approx(1.0f));
    REQUIRE(scaled_view.offset == Approx(2.0f));

    // A zero-length normal is neutralized (clips nothing) instead of
    // discarding the entire scene.
    ClipPlane zero{Vec3(0, 0, 0), 3.0f};
    ClipPlane zero_view = clip_plane_to_view_space(zero, origin, identity);
    REQUIRE(zero_view.normal == Vec3(0.0f));
    REQUIRE(zero_view.offset == Approx(0.0f));
    REQUIRE(glm::dot(Vec3(5.0f, -3.0f, -100.0f), zero_view.normal) + zero_view.offset >= 0.0f);
}

// ---------------------------------------------------------------------------
//  World vs eye space at render level
// ---------------------------------------------------------------------------

TEST_CASE("World-space clip planes cut the same place for any camera position",
          "[clipping][clip_plane_space][integration]") {
    // Keep the half of the world with z >= 0.
    ClipPlane plane{Vec3(0, 0, 1), 0.0f};
    REQUIRE(plane.space == PlaneSpace::WORLD);

    ZSpan at_origin = measure_visible_z(render_cube({plane}, 0.0f), 0.0f);
    ZSpan shifted = measure_visible_z(render_cube({plane}, 4.0f), 4.0f);

    // Cube spans z in [-1, 1]; the cut must be at world z = 0.
    REQUIRE(at_origin.saw_foreground);
    REQUIRE(at_origin.zmin >= Approx(0.0f).margin(0.15f));
    REQUIRE(at_origin.zmin <= Approx(0.0f).margin(0.25f));
    REQUIRE(at_origin.zmax == Approx(0.95f).margin(0.15f));

    // Moving the camera along the plane normal must not move the cut.
    REQUIRE(shifted.saw_foreground);
    REQUIRE(shifted.zmin == Approx(at_origin.zmin).margin(0.1f));
    REQUIRE(shifted.zmax == Approx(at_origin.zmax).margin(0.1f));
}

TEST_CASE("Eye-space clip planes are camera-relative (explicit opt-in)",
          "[clipping][clip_plane_space][integration]") {
    // Same plane definition as above, but in eye space: at eye_z = 0 the cut
    // happens to sit at world z = 0, at eye_z = 4 the plane has travelled
    // with the camera and everything in front of it is gone.
    ClipPlane plane{Vec3(0, 0, 1), 0.0f, PlaneSpace::EYE};

    ZSpan at_origin = measure_visible_z(render_cube({plane}, 0.0f), 0.0f);
    ZSpan shifted = measure_visible_z(render_cube({plane}, 4.0f), 4.0f);

    REQUIRE(at_origin.saw_foreground);   // cuts at world z = 0 here
    REQUIRE(at_origin.zmin >= Approx(0.0f).margin(0.15f));
    REQUIRE_FALSE(shifted.saw_foreground);  // nothing left in front of z = 4
}

TEST_CASE("Multiple clip planes are combined with a logical AND",
          "[clipping][clip_plane_space][integration]") {
    // Keep -0.5 <= z <= 0.5: a slab through the middle of the cube.
    ClipPlane lower{Vec3(0, 0, 1), 0.5f};
    ClipPlane upper{Vec3(0, 0, -1), 0.5f};
    ZSpan slab = measure_visible_z(render_cube({lower, upper}, 0.0f), 0.0f);

    REQUIRE(slab.saw_foreground);
    REQUIRE(slab.zmin == Approx(-0.45f).margin(0.2f));
    REQUIRE(slab.zmax == Approx(0.45f).margin(0.2f));

    // Each plane alone keeps about half of the cube.
    ZSpan lower_only = measure_visible_z(render_cube({lower}, 0.0f), 0.0f);
    REQUIRE(lower_only.count > slab.count);
}

TEST_CASE("A zero-length clip plane normal does not blank out the scene",
          "[clipping][clip_plane_space][integration]") {
    ZSpan no_clip = measure_visible_z(render_cube({}, 0.0f), 0.0f);
    ZSpan zero_normal = measure_visible_z(
        render_cube({ClipPlane{Vec3(0, 0, 0), 5.0f}}, 0.0f), 0.0f);

    REQUIRE(no_clip.count > 0);
    REQUIRE(zero_normal.count == no_clip.count);
}
