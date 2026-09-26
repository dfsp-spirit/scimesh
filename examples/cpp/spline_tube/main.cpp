/// Demo: smooth curves through ordered 3D points with the spline module.
///
/// - spline_tube_compare.png : the same waypoints swept as a tube twice, once
///   through the raw polyline (faceted, grey) and once through a Catmull-Rom
///   spline of it (smooth, blue).  Both use the same number of samples, which
///   is the point: the spline turns a coarse polyline into a curve.
/// - spline_tube_knot.png    : a closed trefoil knot, i.e. a *closed*
///   Catmull-Rom curve through 12 waypoints, resampled to an even spacing and
///   swept into a tube.
///
/// It also prints the path diagnostics (length, maximum curvature, and the
/// largest tube radius that does not fold the tube onto itself).
///
/// Build:
///   cd examples/cpp/spline_tube && mkdir -p build && cd build
///   cmake .. && make
///
/// Run (from build/):
///   ./spline_tube
///
/// Output: spline_tube_compare.png, spline_tube_knot.png

#include <scimesh/camera.h>
#include <scimesh/image.h>
#include <scimesh/primitives.h>
#include <scimesh/render_options.h>
#include <scimesh/renderer.h>
#include <scimesh/scene.h>
#include <scimesh/spline.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using scimesh::Camera;
using scimesh::Color;
using scimesh::Image;
using scimesh::Light;
using scimesh::Mesh;
using scimesh::RenderOptions;
using scimesh::Renderer;
using scimesh::Scene;
using scimesh::ShadingMode;
using scimesh::Vec3;

namespace {

/// A handful of waypoints sampled from a smooth wave, coarsely enough that the
/// raw polyline is visibly faceted.
std::vector<Vec3> wavy_waypoints() {
    const float two_pi = 2.0f * 3.14159265358979323846f;
    std::vector<Vec3> pts;
    for (int i = 0; i <= 8; ++i) {
        const float x = 0.75f * static_cast<float>(i);
        pts.push_back(Vec3(x, 1.0f * std::sin(two_pi * x / 3.0f),
                           0.4f * std::cos(two_pi * x / 4.5f)));
    }
    return pts;
}

/// Waypoints of a trefoil knot, a closed curve that is a good stress test for
/// the parallel-transport frames of the tube sweep.
std::vector<Vec3> trefoil_waypoints(int num_points) {
    std::vector<Vec3> pts;
    for (int i = 0; i < num_points; ++i) {
        const float t = 2.0f * 3.14159265358979323846f *
                        static_cast<float>(i) / static_cast<float>(num_points);
        pts.push_back(Vec3(std::sin(t) + 2.0f * std::sin(2.0f * t),
                           std::cos(t) - 2.0f * std::cos(2.0f * t),
                           -std::sin(3.0f * t)));
    }
    return pts;
}

/// Three-point lighting used by both renders.
std::vector<Light> studio_lights() {
    Light key;
    key.position = Vec3(0.5f, 1.0f, 1.0f);
    key.color = Color(1.00f, 0.97f, 0.90f);
    key.intensity = 1.9f;

    Light fill;
    fill.position = Vec3(-1.0f, 0.2f, 0.3f);
    fill.color = Color(0.50f, 0.60f, 0.80f);
    fill.intensity = 0.6f;

    Light rim;
    rim.position = Vec3(0.0f, -0.4f, -1.0f);
    rim.color = Color(0.70f, 0.65f, 0.65f);
    rim.intensity = 0.5f;

    return {key, fill, rim};
}

RenderOptions studio_options(int width, int height) {
    RenderOptions opts;
    opts.width = width;
    opts.height = height;
    opts.background_color = Color(1.0f, 1.0f, 1.0f);
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = true;
    opts.ambient = 0.25f;
    opts.lights = studio_lights();
    opts.specular_color = Color(1.0f, 1.0f, 1.0f);
    opts.shininess = 48.0f;
    opts.aa_samples = 2;
    return opts;
}

/// Print the path diagnostics and return the maximum curvature, which bounds
/// the tube radius that can be swept along the path without self-intersection.
float report_path(const char *label, const std::vector<Vec3> &path,
                  bool closed) {
    const float length = scimesh::path_length(path, closed);
    const std::vector<float> curvature = scimesh::path_curvature(path, closed);
    float max_curvature = 0.0f;
    for (float k : curvature) {
        max_curvature = std::max(max_curvature, k);
    }
    std::cout << "  " << label << ": " << path.size() << " points, length "
              << length << ", max curvature " << max_curvature;
    if (max_curvature > 0.0f) {
        std::cout << " (a tube stays clean up to radius "
                  << 1.0f / max_curvature << ")";
    }
    std::cout << "\n";
    return max_curvature;
}

/// Polyline tube (left) next to the spline tube through the same waypoints
/// (right), so that the difference is visible in a single image.
void render_comparison() {
    const std::vector<Vec3> waypoints = wavy_waypoints();
    const std::vector<Vec3> smooth = scimesh::catmull_rom_path(waypoints, 24);
    const std::vector<Vec3> even =
        scimesh::resample_by_arclength(smooth, 0.08f);

    std::cout << "\n=== spline_tube_compare ===\n";
    report_path("waypoints", waypoints, false);
    report_path("spline", smooth, false);
    const float limit = report_path("resampled", even, false);

    const float offset = 2.6f;
    const int segments = 14;

    // Sweep the tube below the curvature limit, or the tube folds onto itself
    // on the inside of the tightest bend.
    const float radius = 0.7f / limit;

    // Left: the raw waypoints, swept as-is.  The tube looks like a bent pipe
    // with hard joints, because the sweep only sees straight segments.
    Mesh faceted = scimesh::generate_tube(waypoints, radius, segments,
                                          Color(0.35f, 0.35f, 0.38f), true,
                                          true);
    // Right: the same sweep fed with a spline through those waypoints.
    Mesh curved = scimesh::generate_tube(even, radius, segments,
                                         Color(0.15f, 0.45f, 0.85f), true, true);

    Scene scene;
    scene.add(faceted, glm::translate(glm::mat4(1.0f), Vec3(0.0f, 0.0f, -offset)),
              "faceted");
    scene.add(curved, glm::translate(glm::mat4(1.0f), Vec3(0.0f, 0.0f, offset)),
              "curved");

    const Camera cam = scimesh::camera_fit_scene(
        scene, glm::normalize(Vec3(0.0f, 0.35f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f),
        40.0f, 1.08f);

    Renderer renderer;
    const Image img = renderer.render_scene(scene, cam, studio_options(1000, 700));
    img.write_png("spline_tube_compare.png");
    std::cout << "  Wrote spline_tube_compare.png (" << img.width << "x"
              << img.height << ")\n";
}

/// A closed trefoil knot as a smooth tube.
void render_knot() {
    const std::vector<Vec3> waypoints = trefoil_waypoints(12);
    const std::vector<Vec3> loop =
        scimesh::catmull_rom_path(waypoints, 24, true);
    const std::vector<Vec3> even = scimesh::resample_by_arclength(loop, 0.06f, true);

    std::cout << "\n=== spline_tube_knot ===\n";
    report_path("waypoints", waypoints, true);
    report_path("closed spline", loop, true);
    const float limit = report_path("resampled", even, true);

    // The strands of the knot pass close to each other, so this one stays well
    // below the curvature limit (which only guards against folding the tube
    // onto *itself*, not against two strands touching).
    const Mesh tube = scimesh::generate_tube(even, 0.3f / limit, 12,
                                             Color(0.85f, 0.35f, 0.15f), false,
                                             false);

    Scene scene;
    scene.add(tube, glm::mat4(1.0f), "knot");
    const Camera cam = scimesh::camera_fit_scene(
        scene, glm::normalize(Vec3(0.2f, 0.7f, -1.0f)), Vec3(0.0f, 0.0f, 1.0f),
        40.0f, 1.1f);

    Renderer renderer;
    const Image img = renderer.render_scene(scene, cam, studio_options(900, 900));
    img.write_png("spline_tube_knot.png");
    std::cout << "  Wrote spline_tube_knot.png (" << img.width << "x" << img.height
              << ")\n";
}

} // namespace

int main() {
    render_comparison();
    render_knot();
    std::cout << "\nDone.\n";
    return 0;
}
