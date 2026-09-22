#include "catch_amalgamated.hpp"
#include <scimesh/spline.h>
#include <scimesh/primitives.h>
#include <scimesh/mesh.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace scimesh;
using Catch::Approx;

namespace {

/// Waypoints with clearly uneven spacing, the case the parameterization of a
/// Catmull-Rom curve is about: a long first segment, a very short second one
/// (with a bend in it), then a long third one.
std::vector<Vec3> uneven_waypoints() {
    return {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {10.5f, 0.5f, 0.0f},
            {20.0f, 0.0f, 0.0f}};
}

std::vector<Vec3> waypoints() {
    return {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.5f}, {2.0f, 0.0f, 1.0f},
            {3.0f, 1.5f, 0.0f}, {4.0f, -0.5f, 0.5f}};
}

std::vector<Vec3> square() {
    return {{0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}};
}

std::vector<Vec3> circle(unsigned int num_points, float radius) {
    std::vector<Vec3> pts;
    pts.reserve(num_points);
    const float two_pi = 2.0f * 3.14159265358979323846f;
    for (unsigned int i = 0; i < num_points; ++i) {
        const float a = two_pi * static_cast<float>(i) / static_cast<float>(num_points);
        pts.push_back(Vec3(radius * std::cos(a), radius * std::sin(a), 0.0f));
    }
    return pts;
}

bool all_finite(const std::vector<Vec3> &path) {
    for (const Vec3 &p : path) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
            return false;
        }
    }
    return true;
}

float max_value(const std::vector<float> &values) {
    float best = 0.0f;
    for (float v : values) {
        best = std::max(best, v);
    }
    return best;
}

/// Largest turn angle between two consecutive sampling steps of a path, in
/// radians.  A cusp or a loop shows up here as a large value.
float max_turn_angle(const std::vector<Vec3> &path) {
    float worst = 0.0f;
    for (size_t i = 1; i + 1u < path.size(); ++i) {
        const Vec3 a = path[i] - path[i - 1u];
        const Vec3 b = path[i + 1u] - path[i];
        const float la = glm::length(a);
        const float lb = glm::length(b);
        if (la < 1e-9f || lb < 1e-9f) {
            continue;
        }
        const float c = glm::dot(a, b) / (la * lb);
        worst = std::max(worst, std::acos(std::max(-1.0f, std::min(1.0f, c))));
    }
    return worst;
}

bool inside_bounding_box(const std::vector<Vec3> &path,
                         const std::vector<Vec3> &points, float margin) {
    Vec3 lo = points[0];
    Vec3 hi = points[0];
    for (const Vec3 &p : points) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    for (const Vec3 &p : path) {
        if (p.x < lo.x - margin || p.y < lo.y - margin || p.z < lo.z - margin ||
            p.x > hi.x + margin || p.y > hi.y + margin || p.z > hi.z + margin) {
            return false;
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
//  Catmull-Rom
// ---------------------------------------------------------------------------

TEST_CASE("catmull_rom_path is empty for unusable input", "[spline]") {
    REQUIRE(catmull_rom_path({}).empty());
    REQUIRE(catmull_rom_path({{1.0f, 2.0f, 3.0f}}).empty());
    // A closed curve needs three distinct points.
    REQUIRE(catmull_rom_path({{0, 0, 0}, {1, 0, 0}}, 8, true).empty());
    // Consecutive duplicates collapse, so this is a single point.
    REQUIRE(catmull_rom_path({{1, 1, 1}, {1, 1, 1}, {1, 1, 1}}).empty());
}

TEST_CASE("catmull_rom_path passes through every input point", "[spline]") {
    const std::vector<Vec3> pts = waypoints();
    const int samples = 8;
    const std::vector<Vec3> path = catmull_rom_path(pts, samples);

    REQUIRE(path.size() == (pts.size() - 1u) * static_cast<size_t>(samples) + 1u);
    REQUIRE(all_finite(path));

    // The first sample of every segment is the control point itself, so the
    // interpolation property can be checked exactly, without a tolerance.
    for (size_t i = 0; i + 1u < pts.size(); ++i) {
        const Vec3 &got = path[i * static_cast<size_t>(samples)];
        REQUIRE(got.x == pts[i].x);
        REQUIRE(got.y == pts[i].y);
        REQUIRE(got.z == pts[i].z);
    }
    REQUIRE(path.back().x == pts.back().x);
    REQUIRE(path.back().y == pts.back().y);
    REQUIRE(path.back().z == pts.back().z);
}

TEST_CASE("catmull_rom_path starts and ends along the outer segments",
          "[spline]") {
    const std::vector<Vec3> pts = waypoints();
    const std::vector<Vec3> path = catmull_rom_path(pts, 200);

    const Vec3 start_dir = glm::normalize(path[1] - path[0]);
    const Vec3 expected_start = glm::normalize(pts[1] - pts[0]);
    REQUIRE(glm::dot(start_dir, expected_start) == Approx(1.0f).margin(1e-2f));

    const Vec3 end_dir = glm::normalize(path[path.size() - 1u] - path[path.size() - 2u]);
    const Vec3 expected_end = glm::normalize(pts[pts.size() - 1u] - pts[pts.size() - 2u]);
    REQUIRE(glm::dot(end_dir, expected_end) == Approx(1.0f).margin(1e-2f));
}

TEST_CASE("catmull_rom_path with alpha = 0 is the uniform curve", "[spline]") {
    // Segment 1 of the textbook uniform Catmull-Rom curve at t = 0.5 is
    // (-P0 + 9*P1 + 9*P2 - P3) / 16, which for this square-ish polygon is
    // (1.125, 0.5, 0).
    const std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const std::vector<Vec3> path = catmull_rom_path(pts, 2, false, 0.0f);

    REQUIRE(path.size() == (pts.size() - 1u) * 2u + 1u);   // 7 samples
    const Vec3 mid = path[3];                              // segment 1, s = 0.5
    REQUIRE(mid.x == Approx(1.125f).margin(1e-4f));
    REQUIRE(mid.y == Approx(0.5f).margin(1e-4f));
    REQUIRE(mid.z == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("catmull_rom_path keeps collinear points on the line", "[spline]") {
    const std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    const std::vector<Vec3> path = catmull_rom_path(pts, 16);

    for (const Vec3 &p : path) {
        REQUIRE(p.y == Approx(0.0f).margin(1e-5f));
        REQUIRE(p.z == Approx(0.0f).margin(1e-5f));
        REQUIRE(p.x >= -1e-5f);
        REQUIRE(p.x <= 3.0f + 1e-5f);
    }
}

TEST_CASE("centripetal sampling avoids the cusps of uniform sampling",
          "[spline]") {
    const std::vector<Vec3> pts = uneven_waypoints();

    const std::vector<Vec3> centripetal = catmull_rom_path(pts, 16, false, 0.5f);
    const std::vector<Vec3> uniform = catmull_rom_path(pts, 16, false, 0.0f);
    REQUIRE(all_finite(centripetal));
    REQUIRE(all_finite(uniform));

    // The uniform curve overshoots violently around the short middle segment
    // (its tangent at P1 is derived from the 10 unit long first segment but
    // has to fit into the 0.7 unit long second one), which shows up as a
    // curvature spike that the centripetal parameterization does not have.
    const float centripetal_kink = max_value(path_curvature(centripetal));
    const float uniform_kink = max_value(path_curvature(uniform));
    REQUIRE(centripetal_kink > 0.0f);
    CHECK(uniform_kink > 2.0f * centripetal_kink);

    // A cusp reverses the direction of travel, i.e. turns by pi, which is what
    // the centripetal parameterization exists to prevent.
    CHECK(max_turn_angle(centripetal) < 1.5f);
}

TEST_CASE("catmull_rom_path interpolates the waypoints exactly", "[spline]") {
    // Cross-check the interpolation property for a parameterization where the
    // knot intervals are all different.
    const std::vector<Vec3> pts = uneven_waypoints();
    const int samples = 10;
    const std::vector<Vec3> path = catmull_rom_path(pts, samples);

    for (size_t i = 0; i < pts.size(); ++i) {
        const Vec3 &got = path[i * static_cast<size_t>(samples)];
        REQUIRE(got.x == Approx(pts[i].x).margin(1e-4f));
        REQUIRE(got.y == Approx(pts[i].y).margin(1e-4f));
        REQUIRE(got.z == Approx(pts[i].z).margin(1e-4f));
    }
}

TEST_CASE("closed catmull_rom_path loops back to its first point", "[spline]") {
    const std::vector<Vec3> pts = square();
    const int samples = 8;
    const std::vector<Vec3> path = catmull_rom_path(pts, samples, true);

    REQUIRE(path.size() == pts.size() * static_cast<size_t>(samples) + 1u);
    REQUIRE(all_finite(path));

    // The loop closes on the first point ...
    REQUIRE(path.front().x == path.back().x);
    REQUIRE(path.front().y == path.back().y);
    REQUIRE(path.front().z == path.back().z);

    // ... and every control point is hit (the closed curve uses its neighbours
    // across the seam instead of a reflected phantom point).
    for (size_t i = 0; i < pts.size(); ++i) {
        const Vec3 &got = path[i * static_cast<size_t>(samples)];
        REQUIRE(got.x == Approx(pts[i].x).margin(1e-5f));
        REQUIRE(got.y == Approx(pts[i].y).margin(1e-5f));
        REQUIRE(got.z == Approx(pts[i].z).margin(1e-5f));
    }
}

TEST_CASE("closed catmull_rom_path is periodic", "[spline]") {
    // Rotating the waypoint list must rotate the sampled path, not change it:
    // with a reflected phantom point instead of the wrapped neighbours, the
    // point that happens to be first would behave differently from the others.
    const std::vector<Vec3> pts = square();
    const std::vector<Vec3> rotated = {pts[1], pts[2], pts[3], pts[0]};
    const int samples = 8;

    const std::vector<Vec3> a = catmull_rom_path(pts, samples, true);
    const std::vector<Vec3> b = catmull_rom_path(rotated, samples, true);
    REQUIRE(a.size() == b.size());

    const size_t shift = static_cast<size_t>(samples);
    for (size_t i = 0; i + shift < a.size(); ++i) {
        REQUIRE(glm::length(a[i + shift] - b[i]) < 1e-5f);
    }
}

TEST_CASE("catmull_rom_path removes consecutive duplicate points", "[spline]") {
    const std::vector<Vec3> pts = waypoints();
    std::vector<Vec3> with_duplicates;
    for (const Vec3 &p : pts) {
        with_duplicates.push_back(p);
        with_duplicates.push_back(p);   // every point repeated once
    }

    const std::vector<Vec3> a = catmull_rom_path(pts, 8);
    const std::vector<Vec3> b = catmull_rom_path(with_duplicates, 8);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].x == b[i].x);
        REQUIRE(a[i].y == b[i].y);
        REQUIRE(a[i].z == b[i].z);
    }
}

// ---------------------------------------------------------------------------
//  Hermite
// ---------------------------------------------------------------------------

TEST_CASE("hermite_path honours explicit tangents", "[spline]") {
    // A symmetric "up then flat" tangent setup, where the midpoint of the
    // single segment is a hand-computable value: h10(0.5) == 0.125, so the
    // y coordinate is exactly the tangent's y times 0.125.
    const std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}};
    const std::vector<Vec3> tan = {{0, 1, 0}, {0, 0, 0}};
    const std::vector<Vec3> path = hermite_path(pts, tan, 2);

    REQUIRE(path.size() == 3u);
    REQUIRE(path[0].x == Approx(0.0f));
    REQUIRE(path[2].x == Approx(1.0f));
    REQUIRE(path[1].x == Approx(0.5f).margin(1e-6f));
    REQUIRE(path[1].y == Approx(0.125f).margin(1e-6f));
}

TEST_CASE("hermite_path handles a closed curve", "[spline]") {
    const std::vector<Vec3> pts = square();
    std::vector<Vec3> tan(pts.size(), Vec3(0.0f, 0.0f, 1.0f));   // all "up"
    const std::vector<Vec3> path = hermite_path(pts, tan, 4, true);

    REQUIRE(path.size() == pts.size() * 4u + 1u);
    REQUIRE(all_finite(path));
    REQUIRE(path.front().x == path.back().x);
    REQUIRE(path.front().y == path.back().y);
}

TEST_CASE("hermite_path is empty for unusable input", "[spline]") {
    const std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};

    REQUIRE(hermite_path({}, {}).empty());
    REQUIRE(hermite_path({{0, 0, 0}}, {{0, 0, 1}}).empty());
    // A tangent for every point is required ...
    REQUIRE(hermite_path(pts, {{0, 0, 1}, {0, 0, 1}}).empty());
    // ... and duplicate points make the parameterization degenerate.
    REQUIRE(hermite_path({{0, 0, 0}, {0, 0, 0}, {1, 0, 0}},
                         {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}})
                .empty());
}

// ---------------------------------------------------------------------------
//  B-spline
// ---------------------------------------------------------------------------

TEST_CASE("bspline_path approximates instead of interpolating", "[spline]") {
    const std::vector<Vec3> pts = square();
    const std::vector<Vec3> path = bspline_path(pts, 2);

    REQUIRE_FALSE(path.empty());
    REQUIRE(all_finite(path));

    // The convex hull property: a B-spline never leaves the bounding box of its
    // control points.
    REQUIRE(inside_bounding_box(path, pts, 1e-4f));

    // ... but it does not pass through the interior control points.
    float closest = std::numeric_limits<float>::max();
    for (const Vec3 &p : path) {
        closest = std::min(closest, glm::length(p - pts[1]));
    }
    REQUIRE(closest > 0.05f);
}

TEST_CASE("bspline_path sample counts and closure", "[spline]") {
    const std::vector<Vec3> pts = waypoints();   // 5 control points

    // An open uniform cubic B-spline has n - 3 segments, the last of which
    // also emits its endpoint.
    const std::vector<Vec3> open = bspline_path(pts, 4);
    REQUIRE(open.size() == (pts.size() - 3u) * 4u + 1u);

    const std::vector<Vec3> closed = bspline_path(pts, 4, true);
    REQUIRE(closed.size() == pts.size() * 4u + 1u);
    REQUIRE(closed.front().x == closed.back().x);
    REQUIRE(closed.front().y == closed.back().y);
    REQUIRE(closed.front().z == closed.back().z);
}

TEST_CASE("bspline_path is empty for fewer than four points", "[spline]") {
    REQUIRE(bspline_path({}).empty());
    REQUIRE(bspline_path({{0, 0, 0}}).empty());
    REQUIRE(bspline_path({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}).empty());
    REQUIRE(bspline_path({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}}, 8).size() >
            0u);
}

TEST_CASE("bspline_path stays on the line for collinear points", "[spline]") {
    const std::vector<Vec3> pts = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {3, 3, 3}};
    const std::vector<Vec3> path = bspline_path(pts, 8);

    for (const Vec3 &p : path) {
        // All three coordinates stay equal on the diagonal.
        REQUIRE(p.x == Approx(p.y).margin(1e-5f));
        REQUIRE(p.y == Approx(p.z).margin(1e-5f));
        REQUIRE(p.x >= Approx(0.0f));
        REQUIRE(p.x <= Approx(3.0f));
    }
}

// ---------------------------------------------------------------------------
//  Bezier
// ---------------------------------------------------------------------------

TEST_CASE("bezier_path hits both endpoints", "[spline]") {
    const std::vector<Vec3> ctrl = {{0, 0, 0}, {1, 2, 0}, {3, 1, 0}, {4, 0, 0}};
    const std::vector<Vec3> path = bezier_path(ctrl, 32);

    REQUIRE(path.size() == 32u);
    REQUIRE(all_finite(path));
    REQUIRE(path.front().x == Approx(ctrl.front().x).margin(1e-5f));
    REQUIRE(path.front().y == Approx(ctrl.front().y).margin(1e-5f));
    REQUIRE(path.back().x == Approx(ctrl.back().x).margin(1e-5f));
    REQUIRE(path.back().y == Approx(ctrl.back().y).margin(1e-5f));
}

TEST_CASE("bezier_path of a quadratic curve matches the closed form",
          "[spline]") {
    // B(0.5) = (P0 + 2*P1 + P2) / 4 for a quadratic Bezier curve.
    const std::vector<Vec3> ctrl = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}};
    const std::vector<Vec3> path = bezier_path(ctrl, 3);

    REQUIRE(path.size() == 3u);
    REQUIRE(path[1].x == Approx((0.0f + 2.0f * 1.0f + 2.0f) / 4.0f).margin(1e-5f));
    REQUIRE(path[1].y == Approx((0.0f + 2.0f * 1.0f + 0.0f) / 4.0f).margin(1e-5f));
}

TEST_CASE("bezier_path degenerates gracefully", "[spline]") {
    REQUIRE(bezier_path({}).empty());
    REQUIRE(bezier_path({{1, 2, 3}}).empty());
    // A single sample count is raised to two, so both endpoints are emitted.
    const std::vector<Vec3> path = bezier_path({{0, 0, 0}, {1, 1, 0}}, 1);
    REQUIRE(path.size() == 2u);
}

// ---------------------------------------------------------------------------
//  Path queries and resampling
// ---------------------------------------------------------------------------

TEST_CASE("path_length measures the polyline", "[spline]") {
    const std::vector<Vec3> right_triangle = {{0, 0, 0}, {3, 0, 0}, {3, 4, 0}};
    REQUIRE(path_length(right_triangle) == Approx(7.0f));
    REQUIRE(path_length(right_triangle, true) == Approx(12.0f));   // + hypotenuse

    REQUIRE(path_length({}) == Approx(0.0f));
    REQUIRE(path_length({{1, 1, 1}}) == Approx(0.0f));
    REQUIRE(path_length({{0, 0, 0}, {0, 3, 4}}) == Approx(5.0f));
}

TEST_CASE("path_tangents points along the path", "[spline]") {
    const std::vector<Vec3> line = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    for (const Vec3 &t : path_tangents(line)) {
        REQUIRE(t.x == Approx(1.0f));
        REQUIRE(t.y == Approx(0.0f));
        REQUIRE(t.z == Approx(0.0f));
    }

    // On a closed square each corner tangents bisect the two edges.
    const std::vector<Vec3> tangents = path_tangents(square(), true);
    REQUIRE(tangents.size() == 4u);
    REQUIRE(tangents[1].x == Approx(std::sqrt(0.5f)).margin(1e-5f));
    REQUIRE(tangents[1].y == Approx(std::sqrt(0.5f)).margin(1e-5f));
    for (const Vec3 &t : tangents) {
        REQUIRE(glm::length(t) == Approx(1.0f).margin(1e-5f));
    }

    REQUIRE(path_tangents({{0, 0, 0}}).empty());
}

TEST_CASE("path_curvature is zero on a straight path", "[spline]") {
    const std::vector<Vec3> line = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0},
                                    {3, 0, 0}, {4, 0, 0}};
    for (float k : path_curvature(line)) {
        REQUIRE(k == Approx(0.0f).margin(1e-6f));
    }

    // Uneven spacing must not show up as curvature.
    const std::vector<Vec3> uneven = {{0, 0, 0}, {0.1f, 0, 0}, {9.0f, 0, 0},
                                      {9.2f, 0, 0}};
    for (float k : path_curvature(uneven)) {
        REQUIRE(k == Approx(0.0f).margin(1e-5f));
    }

    REQUIRE(path_curvature({{0, 0, 0}, {1, 0, 0}}).empty());
}

TEST_CASE("path_curvature of a circle is the reciprocal radius", "[spline]") {
    const float radius = 2.0f;
    const std::vector<Vec3> pts = circle(64u, radius);
    const std::vector<float> k = path_curvature(pts, true);

    REQUIRE(k.size() == 64u);
    for (float value : k) {
        REQUIRE(value == Approx(1.0f / radius).margin(1e-2f));
    }

    // A straight line has no curvature at all, a circle has plenty, so the
    // estimator has to be able to tell the two apart.
    REQUIRE(max_value(k) > 0.4f);
}

TEST_CASE("path_curvature of an open path copies the endpoints", "[spline]") {
    const std::vector<Vec3> pts = circle(32u, 1.0f);
    const std::vector<float> k = path_curvature(pts, false);

    REQUIRE(k.size() == 32u);
    REQUIRE(k[0] == Approx(k[1]));
    REQUIRE(k[31] == Approx(k[30]));
    REQUIRE(k[0] == Approx(1.0f).margin(2e-2f));
}

TEST_CASE("resample_by_arclength gives evenly spaced points", "[spline]") {
    const float step = 0.25f;

    // A collinear path with wildly uneven segment lengths (10, 0.02, 9.98) is
    // the clean case: there are no corners to cut, so the straight-line
    // distance between two samples *is* the arc length between them, and the
    // spacing can be checked tightly.
    const std::vector<Vec3> uneven_line = {
        {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {10.02f, 0.0f, 0.0f},
        {20.0f, 0.0f, 0.0f}};
    const std::vector<Vec3> straight_even =
        resample_by_arclength(uneven_line, step);

    REQUIRE(straight_even.size() > 80u);
    REQUIRE(all_finite(straight_even));
    REQUIRE(straight_even.front().x == Approx(uneven_line.front().x));
    REQUIRE(straight_even.back().x == Approx(uneven_line.back().x));
    for (size_t i = 1; i + 1u < straight_even.size(); ++i) {
        const float d = glm::length(straight_even[i] - straight_even[i - 1u]);
        REQUIRE(d == Approx(step).margin(1e-3f));
    }

    // The uneven input really was uneven, so even spacing is not a trivial
    // statement: sampling a path like this by segment would inherit the 10 to
    // 0.02 spread.
    float min_input = std::numeric_limits<float>::max();
    float max_input = 0.0f;
    for (size_t i = 1; i < uneven_line.size(); ++i) {
        const float d = glm::length(uneven_line[i] - uneven_line[i - 1u]);
        min_input = std::min(min_input, d);
        max_input = std::max(max_input, d);
    }
    REQUIRE(max_input > 1.5f * min_input);

    // On a curved path the uniform quantity is the arc length: the chord
    // between two samples cuts the corner, so it comes out slightly *shorter*
    // than the step (0.26 % at a radius of 1 and a step of 0.25).  A closed
    // loop is covered in a whole number of steps, so its effective step can
    // differ from the requested one by a fraction of a percent as well.
    const std::vector<Vec3> loop_in = catmull_rom_path(circle(8u, 1.0f), 16, true);
    const std::vector<Vec3> loop = resample_by_arclength(loop_in, step, true);

    // ~2*pi length at a step of 0.25 is 25 steps, so 25 samples plus closure.
    REQUIRE(loop.size() == 26u);
    REQUIRE(all_finite(loop));
    REQUIRE(glm::length(loop.front() - loop.back()) < 1e-5f);
    for (size_t i = 1; i < loop.size(); ++i) {
        const float d = glm::length(loop[i] - loop[i - 1u]);
        REQUIRE(d <= 1.025f * step);
        REQUIRE(d >= 0.975f * step);
    }
}

TEST_CASE("resample_by_arclength does not duplicate the endpoints",
          "[spline]") {
    // A straight line of length 1, sampled at an exact divisor of its length:
    // the result is the five points 0, 0.25, 0.5, 0.75, 1.
    const std::vector<Vec3> line = {{0, 0, 0}, {1, 0, 0}};
    const std::vector<Vec3> even = resample_by_arclength(line, 0.25f);

    REQUIRE(even.size() == 5u);
    for (size_t i = 0; i < even.size(); ++i) {
        REQUIRE(even[i].x == Approx(0.25f * static_cast<float>(i)).margin(1e-5f));
    }

    // A closed path is expressed by ending on a copy of its first point, the
    // same convention the sampling functions use, so the swept shape closes.
    const std::vector<Vec3> loop = resample_by_arclength(square(), 0.5f, true);
    REQUIRE(loop.size() == 9u);   // 8 samples plus the closing copy
    REQUIRE(glm::length(loop.back() - loop.front()) < 1e-6f);
    for (size_t i = 1; i < loop.size(); ++i) {
        REQUIRE(glm::length(loop[i] - loop[i - 1u]) == Approx(0.5f).margin(1e-5f));
    }
}

TEST_CASE("resample_by_arclength handles degenerate input", "[spline]") {
    const std::vector<Vec3> line = {{0, 0, 0}, {1, 0, 0}};

    // A non-positive step means "no resampling".
    REQUIRE(resample_by_arclength(line, 0.0f).size() == line.size());
    REQUIRE(resample_by_arclength(line, -1.0f).size() == line.size());

    // Fewer than two points, or a path without length, cannot be resampled.
    REQUIRE(resample_by_arclength({}, 0.5f).empty());
    const std::vector<Vec3> single = resample_by_arclength({{1, 2, 3}}, 0.5f);
    REQUIRE(single.size() == 1u);
    const std::vector<Vec3> collapsed =
        resample_by_arclength({{1, 2, 3}, {1, 2, 3}}, 0.5f);
    REQUIRE(collapsed.size() == 1u);

    // A step longer than the path keeps both ends of an open path.
    const std::vector<Vec3> coarse = resample_by_arclength(line, 10.0f);
    REQUIRE(coarse.size() == 2u);
    REQUIRE(coarse.front().x == Approx(0.0f));
    REQUIRE(coarse.back().x == Approx(1.0f));
}

TEST_CASE("remove_duplicate_points collapses runs of duplicates", "[spline]") {
    const std::vector<Vec3> pts = {{0, 0, 0}, {0, 0, 0}, {1, 0, 0},
                                   {1, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    const std::vector<Vec3> cleaned = remove_duplicate_points(pts);

    REQUIRE(cleaned.size() == 3u);
    REQUIRE(cleaned[0].x == Approx(0.0f));
    REQUIRE(cleaned[1].x == Approx(1.0f));
    REQUIRE(cleaned[2].x == Approx(2.0f));

    REQUIRE(remove_duplicate_points({}).empty());
    REQUIRE(remove_duplicate_points({{1, 1, 1}, {1, 1, 1}}).size() == 1u);
}

// ---------------------------------------------------------------------------
//  Integration with the geometry generators
// ---------------------------------------------------------------------------

TEST_CASE("a spline path feeds generate_tube", "[spline]") {
    const std::vector<Vec3> path = catmull_rom_path(waypoints(), 8);
    const int segments = 12;
    const Mesh tube = generate_tube(path, 0.1f, segments, Color(0.8f, 0.2f, 0.2f));

    REQUIRE_FALSE(tube.empty());
    REQUIRE(tube.is_valid());
    REQUIRE(tube.has_normals());

    // One ring per path point, plus a cap at each end.
    REQUIRE(tube.vertices.size() ==
            path.size() * static_cast<size_t>(segments) + 2u * (1u + static_cast<size_t>(segments)));
    for (const Vec3 &n : tube.normals) {
        REQUIRE(glm::length(n) == Approx(1.0f).margin(1e-3f));
    }
    for (const Vec3 &v : tube.vertices) {
        REQUIRE(std::isfinite(v.x));
        REQUIRE(std::isfinite(v.y));
        REQUIRE(std::isfinite(v.z));
    }
}

TEST_CASE("a closed spline loop feeds generate_tube", "[spline]") {
    const std::vector<Vec3> path =
        resample_by_arclength(catmull_rom_path(square(), 8, true), 0.1f, true);
    const Mesh tube = generate_tube(path, 0.05f, 8, Color(0.2f, 0.7f, 0.9f),
                                    false, false);

    REQUIRE_FALSE(tube.empty());
    REQUIRE(tube.is_valid());
    REQUIRE(tube.vertices.size() == path.size() * 8u);

    // The loop closes: the first and the last ring sit at the same place, so
    // the swept shape has no gap (it is not welded, but it is not open either).
    const Vec3 first_center = path.front();
    const Vec3 last_center = path.back();
    REQUIRE(glm::length(first_center - last_center) < 1e-5f);
}

TEST_CASE("paths generated from degenerate input produce empty meshes",
          "[spline]") {
    const std::vector<Vec3> empty_path = catmull_rom_path({});
    REQUIRE(generate_tube(empty_path, 0.1f, 8, Color(1, 1, 1)).empty());

    const std::vector<Vec3> one_point = catmull_rom_path({{1, 1, 1}});
    REQUIRE(generate_tube(one_point, 0.1f, 8, Color(1, 1, 1)).empty());

    // Sampling density is clamped to at least one per segment, so a caller
    // passing zero still gets a usable (coarse) curve instead of nothing.
    const std::vector<Vec3> coarse = catmull_rom_path(waypoints(), 0);
    REQUIRE(coarse.size() == waypoints().size());
}
