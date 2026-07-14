#include "catch_amalgamated.hpp"
#include "camera.h"
#include "math_utils.h"
#include "test_meshes.h"
#include "renderer.h"
#include "render_options.h"
#include "image.h"
#include "primitives.h"
#include <glm/glm.hpp>

using namespace scimesh;
using Catch::Approx;
using scimesh_test::make_icosphere;

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

// ---- Helpers for auto-framing tests ------------------------------------------

struct ContentBBox {
    int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    int width()  const { return max_x - min_x + 1; }
    int height() const { return max_y - min_y + 1; }
    int center_x() const { return (min_x + max_x) / 2; }
    int center_y() const { return (min_y + max_y) / 2; }
};

static ContentBBox find_content_bbox(const Image &img,
                                     uint8_t bg_r, uint8_t bg_g, uint8_t bg_b) {
    ContentBBox bbox;
    bbox.min_x = img.width;  bbox.max_x = 0;
    bbox.min_y = img.height; bbox.max_y = 0;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r != bg_r || g != bg_g || b != bg_b) {
                if (x < bbox.min_x) bbox.min_x = x;
                if (x > bbox.max_x) bbox.max_x = x;
                if (y < bbox.min_y) bbox.min_y = y;
                if (y > bbox.max_y) bbox.max_y = y;
            }
        }
    }
    return bbox;
}

// Render a mesh with camera_fit_mesh, return the content bounding box
static ContentBBox render_and_measure(const Mesh &mesh,
                                      const Vec3 &direction,
                                      float margin,
                                      int img_size) {
    Camera cam = camera_fit_mesh(mesh, direction,
        Vec3(0.0f, 1.0f, 0.0f), 40.0f, margin);

    RenderOptions opts;
    opts.width  = img_size;
    opts.height = img_size;
    opts.background_color = Color(0, 0, 0, 1);
    opts.default_color    = Color(0.8f, 0.8f, 0.8f, 1);
    opts.shading = ShadingMode::FLAT;
    opts.ambient = 0.8f;

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);
    return find_content_bbox(img, 0, 0, 0);
}

// ---- Auto-framing tests -----------------------------------------------------

TEST_CASE("camera_fit_mesh auto-frames a single sphere proportionally to margin",
          "[camera][auto-view]") {
    Mesh sphere = make_icosphere(2);

    Vec3 direction = glm::normalize(Vec3(0.5f, 0.3f, -1.0f));

    const int size = 300;
    ContentBBox bbox_13 = render_and_measure(sphere, direction, 1.3f, size);
    ContentBBox bbox_20 = render_and_measure(sphere, direction, 2.0f, size);

    REQUIRE(bbox_13.width() > 0);
    REQUIRE(bbox_13.height() > 0);
    REQUIRE(bbox_20.width() > 0);
    REQUIRE(bbox_20.height() > 0);

    float fill_w_13 = static_cast<float>(bbox_13.width()) / size;
    float fill_h_13 = static_cast<float>(bbox_13.height()) / size;
    float fill_w_20 = static_cast<float>(bbox_20.width()) / size;
    float fill_h_20 = static_cast<float>(bbox_20.height()) / size;

    // margin=1.3: sphere surface fills ~40-55% (AABB corners at edges)
    REQUIRE(fill_w_13 > 0.35f);
    REQUIRE(fill_w_13 < 0.65f);
    REQUIRE(fill_h_13 > 0.35f);
    REQUIRE(fill_h_13 < 0.65f);

    // margin=2.0 should fill less than margin=1.3
    REQUIRE(fill_w_20 < fill_w_13);
    REQUIRE(fill_h_20 < fill_h_13);

    // Linear fill ratio should be approximately margin ratio
    float linear_ratio = fill_w_13 / fill_w_20;
    float expected_ratio = 2.0f / 1.3f;
    REQUIRE(linear_ratio > expected_ratio * 0.7f);
    REQUIRE(linear_ratio < expected_ratio * 1.3f);

    // Content should be roughly centered (within 5% of image center)
    int img_center = size / 2;
    REQUIRE(std::abs(bbox_13.center_x() - img_center) < size / 20);
    REQUIRE(std::abs(bbox_13.center_y() - img_center) < size / 20);
}

TEST_CASE("camera_fit_mesh auto-frames a cluster of spheres proportionally to margin",
          "[camera][auto-view]") {
    std::vector<Vec3> centers = {
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 0.2f, 0.0f),
        Vec3(-1.0f, 0.5f, 0.3f),
        Vec3(0.3f, -1.0f, -0.2f),
        Vec3(0.5f, 0.5f, 0.8f),
        Vec3(-0.5f, -0.7f, -0.6f),
    };

    std::vector<float> radii(centers.size(), 0.25f);
    std::vector<Color> colors(centers.size(), Color(0.8f, 0.8f, 0.8f, 1.0f));
    Mesh cluster = generate_multi_spheres(centers, radii, colors, 12);

    Vec3 direction = glm::normalize(Vec3(0.3f, 0.4f, -1.0f));

    const int size = 300;
    ContentBBox bbox_13 = render_and_measure(cluster, direction, 1.3f, size);
    ContentBBox bbox_20 = render_and_measure(cluster, direction, 2.0f, size);

    REQUIRE(bbox_13.width() > 0);
    REQUIRE(bbox_13.height() > 0);

    float fill_w_13 = static_cast<float>(bbox_13.width()) / size;
    float fill_h_13 = static_cast<float>(bbox_13.height()) / size;
    float fill_w_20 = static_cast<float>(bbox_20.width()) / size;
    float fill_h_20 = static_cast<float>(bbox_20.height()) / size;

    // margin=1.3: cluster surface fills ~40-60%
    REQUIRE(fill_w_13 > 0.35f);
    REQUIRE(fill_w_13 < 0.70f);
    REQUIRE(fill_h_13 > 0.35f);
    REQUIRE(fill_h_13 < 0.70f);

    // margin=2.0 should fill less than margin=1.3
    REQUIRE(fill_w_20 < fill_w_13);
    REQUIRE(fill_h_20 < fill_h_13);

    // Linear fill ratio should be approximately margin ratio
    float linear_ratio = fill_w_13 / fill_w_20;
    float expected_ratio = 2.0f / 1.3f;
    REQUIRE(linear_ratio > expected_ratio * 0.75f);
    REQUIRE(linear_ratio < expected_ratio * 1.25f);

    int img_center = size / 2;
    REQUIRE(std::abs(bbox_13.center_x() - img_center) < size / 20);
    REQUIRE(std::abs(bbox_13.center_y() - img_center) < size / 20);
}

TEST_CASE("camera_fit_mesh orthographic uses perp extent not FOV distance", "[camera]") {
    Mesh sphere = generate_sphere(Vec3(0, 0, 0), 1.0f, 16, Color(1, 0, 0));
    Vec3 direction = glm::normalize(Vec3(0, 0, -1));

    Camera persp = camera_fit_mesh(sphere, direction, Vec3(0, 1, 0), 45.0f, 1.0f,
                                    ProjectionType::PERSPECTIVE);
    Camera ortho = camera_fit_mesh(sphere, direction, Vec3(0, 1, 0), 45.0f, 1.0f,
                                    ProjectionType::ORTHOGRAPHIC);

    REQUIRE(persp.projection == ProjectionType::PERSPECTIVE);
    REQUIRE(ortho.projection == ProjectionType::ORTHOGRAPHIC);

    float persp_dist = glm::length(persp.eye - persp.center);
    float ortho_dist = glm::length(ortho.eye - ortho.center);
    REQUIRE(ortho_dist < persp_dist);

    // Unit sphere bbox is [-1,1]³, perp extent from (0,0,-1) axis ≈ sqrt(2)
    REQUIRE(ortho_dist == Approx(1.414f).margin(0.1f));
}

TEST_CASE("camera_fit_scene orthographic frames mesh correctly", "[camera]") {
    std::vector<Vec3> centers = {Vec3(-3, 0, 0), Vec3(3, 0, 0)};
    std::vector<float> radii = {1.0f, 1.0f};
    std::vector<Color> colors = {Color(1, 0, 0), Color(0, 0, 1)};
    Mesh cluster = generate_multi_spheres(centers, radii, colors, 12);

    Scene scene;
    scene.meshes.push_back(cluster);

    Vec3 direction = glm::normalize(Vec3(0, 0, -1));
    Camera ortho = camera_fit_scene(scene, direction, Vec3(0, 1, 0), 45.0f, 1.0f,
                                     ProjectionType::ORTHOGRAPHIC);

    REQUIRE(ortho.projection == ProjectionType::ORTHOGRAPHIC);
    float dist = glm::length(ortho.eye - ortho.center);
    // Bbox [-4,4]×[-1,1]×[-1,1], perp extent ≈ sqrt(4²+1²) ≈ 4.123
    REQUIRE(dist == Approx(4.123f).margin(0.2f));
}
