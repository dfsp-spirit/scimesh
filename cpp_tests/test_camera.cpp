#include "catch_amalgamated.hpp"
#include "camera.h"
#include "math_utils.h"
#include <glm/glm.hpp>

using namespace scimesh;
using Catch::Approx;

TEST_CASE("Camera view matrix translates eye to origin", "[camera]") {
    Camera cam;
    cam.eye = Vec3(0, 0, 10);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);

    Mat4 view = cam.get_view_matrix();
    Vec3 transformed = transform_point(view, Vec3(0, 0, 0));
    REQUIRE(transformed.z == Approx(-10.0f).margin(1e-5f));
}

TEST_CASE("Camera perspective projection maps unit cube correctly", "[camera]") {
    Camera cam;
    cam.projection = ProjectionType::PERSPECTIVE;
    cam.fov_degrees = 90.0f;
    cam.eye = Vec3(0, 0, 5);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);

    Mat4 proj = cam.get_projection_matrix(1.0f, 0.1f, 100.0f);
    // A point at origin should be in front of the camera
    Vec4 clip = proj * Vec4(0, 0, -5.0f, 1.0f); // z=-5 in view space (in front of camera at z=5 looking at origin)
    // After perspective divide, should be within [-1, 1]
    Vec3 ndc = perspective_divide(clip);
    REQUIRE(ndc.x == Approx(0.0f).margin(1e-4f));
    REQUIRE(ndc.y == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Camera orthographic projection preserves distances", "[camera]") {
    Camera cam;
    cam.projection = ProjectionType::ORTHOGRAPHIC;
    cam.eye = Vec3(0, 0, 5);
    cam.center = Vec3(0, 0, 0);
    cam.up = Vec3(0, 1, 0);

    Mat4 proj = cam.get_projection_matrix(1.0f, 0.1f, 100.0f);
    // In ortho, NDC z should map near to -1 and far to 1
    // A point at z=-5 in view space (distance 5 from camera) should be somewhere in middle
    Vec4 clip_near = proj * Vec4(0, 0, -0.1f, 1.0f);
    Vec3 ndc_near = perspective_divide(clip_near);
    REQUIRE(ndc_near.z == Approx(-1.0f).margin(1e-4f));
}

TEST_CASE("Camera default values are reasonable", "[camera]") {
    Camera cam;
    REQUIRE(cam.projection == ProjectionType::PERSPECTIVE);
    REQUIRE(cam.fov_degrees == Approx(45.0f));
    REQUIRE(cam.up.y == Approx(1.0f));
}
