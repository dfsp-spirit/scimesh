/// @file spline.h
/// @brief Smooth curve sampling through ordered 3D points (splines).
///
/// These functions turn a coarse list of waypoints into a dense, smooth
/// polyline, here called a *path*.  A path is what the path-taking parts of the
/// library expect: generate_tube() sweeps a cross-section along one, and line
/// layers (render_segments(), LineLayer) draw one straight segment per
/// consecutive pair of points.  Feeding a handful of control points directly to
/// those produces a visibly faceted tube and a polygonal line; a spline is how
/// you get the smooth version.
///
/// Nothing here knows about meshes, scenes or the renderer: this is pure
/// geometry on `std::vector<Vec3>`, so it composes with primitives.h, lines.h
/// and your own code alike.
///
/// @par Example
/// @code{.cpp}
/// // A smooth tube through four waypoints:
/// std::vector<Vec3> waypoints = {{0,0,0}, {1,1,0}, {2,0,0}, {3,1,0}};
/// std::vector<Vec3> path = catmull_rom_path(waypoints, 8);
/// Mesh tube = generate_tube(path, 0.1f, 12, Color(0.8f, 0.2f, 0.2f));
/// @endcode
///
/// @see generate_tube(), generate_multi_tubes(), LineLayer
///
/// @note All sampling functions store the result as `float` (that is what Vec3
///       is), but the curve parameterization is computed in `double`.  The
///       knot spacing of a centripetal Catmull-Rom curve involves `pow()`, and
///       doing that in float would make the parameterization — not the
///       geometry — the dominant source of error on long paths.

#pragma once

#include <scimesh/types.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scimesh {

namespace detail {

/// @brief Minimum number of points that can describe a path.
///
/// A path needs a direction, so two distinct points are the absolute minimum;
/// closed (looping) curves need at least three.
constexpr size_t kMinPathPoints = 2u;

/// @brief Smallest knot interval, to keep a degenerate point pair from
///        producing a zero-length interval (and a division by zero).
constexpr double kMinKnotInterval = 1e-6;

/// @brief Tolerance used when deciding whether an arc-length sample coincides
///        with the end of a path, relative to the total path length.
constexpr double kArcLengthEpsilon = 1e-9;

/// @brief Cubic Hermite basis evaluation of a single curve segment.
///
/// Evaluates `P(s)` for one segment of a cubic Hermite curve, given the two
/// endpoint positions, the two endpoint derivatives `m0`/`m1` with respect to
/// the curve parameter `t`, and the length `h` of the parameter interval that
/// the segment spans (`h = t1 - t0`, so that the derivative is rescaled to the
/// local `s` in `[0, 1]`).
///
/// This is the single evaluation kernel behind hermite_path(),
/// catmull_rom_path() and the endpoint handling of both: the other functions
/// only differ in how they come up with `m0`/`m1` and the knot sequence.
///
/// @param p0, p1 Segment endpoint positions.
/// @param m0, m1 Segment endpoint derivatives (dP/dt).
/// @param h      Parameter interval length of the segment.
/// @param s      Local parameter in `[0, 1]`.
/// @return The point on the curve segment at `s`.
inline Vec3 hermite_segment(const Vec3 &p0, const Vec3 &p1, const Vec3 &m0,
                            const Vec3 &m1, double h, double s) {
    const double s2 = s * s;
    const double s3 = s2 * s;

    const double w00 = 2.0 * s3 - 3.0 * s2 + 1.0;
    const double w10 = s3 - 2.0 * s2 + s;
    const double w01 = -2.0 * s3 + 3.0 * s2;
    const double w11 = s3 - s2;

    const Vec3 ends = p0 * static_cast<float>(w00) + p1 * static_cast<float>(w01);
    const Vec3 tangents = m0 * static_cast<float>(w10 * h) +
                          m1 * static_cast<float>(w11 * h);
    return ends + tangents;
}

/// @brief Number of segments a sampling function has to emit.
///
/// An open curve through `n` points has `n - 1` segments; a closed one has `n`,
/// because the last point connects back to the first.
inline size_t segment_count(size_t num_points, bool closed) {
    if (num_points < kMinPathPoints) {
        return 0u;
    }
    return closed ? num_points : num_points - 1u;
}

/// @brief Index of the point after `i`, wrapping around for closed curves.
inline size_t next_index(size_t i, size_t num_points, bool closed) {
    const size_t n = i + 1u;
    if (n < num_points) {
        return n;
    }
    return closed ? 0u : num_points - 1u;
}

/// @brief Clamp a requested sample count to at least one.
inline int sample_count(int samples_per_segment) {
    return std::max(1, samples_per_segment);
}

} // namespace detail

// ---------------------------------------------------------------------------
//  Path queries
// ---------------------------------------------------------------------------

/// @brief Total length of a polyline path.
///
/// Sums the distances between consecutive points, plus the closing distance
/// from the last point back to the first for a closed path.
///
/// @param path   Points of the path.
/// @param closed Whether the path loops back to its first point.
/// @return The arc length of the path (0 for fewer than two points).
///
/// @par Example
/// @code{.cpp}
/// float len = path_length({{0,0,0}, {1,0,0}, {1,1,0}});  // 2.0f
/// @endcode
///
/// @see resample_by_arclength()
inline float path_length(const std::vector<Vec3> &path, bool closed = false) {
    const size_t num_segments = detail::segment_count(path.size(), closed);
    double total = 0.0;
    for (size_t i = 0; i < num_segments; ++i) {
        total += glm::length(path[detail::next_index(i, path.size(), closed)] -
                             path[i]);
    }
    return static_cast<float>(total);
}

/// @brief Remove points that repeat their predecessor.
///
/// Duplicate (or nearly duplicate) points carry no direction, so they make the
/// frame construction of a swept tube degenerate and waste samples of a spline.
/// Comparison is against the previously *kept* point, so a run of duplicates
/// collapses to its first point.
///
/// @param path    Points of the path.
/// @param epsilon Minimum distance for two points to count as distinct.
/// @return The cleaned path (the first point is always kept).
///
/// @see generate_tube()
inline std::vector<Vec3> remove_duplicate_points(const std::vector<Vec3> &path,
                                                 float epsilon = 1e-6f) {
    std::vector<Vec3> cleaned;
    cleaned.reserve(path.size());
    for (const Vec3 &p : path) {
        if (cleaned.empty() || glm::length(p - cleaned.back()) > epsilon) {
            cleaned.push_back(p);
        }
    }
    return cleaned;
}

/// @brief Unit tangent direction at every point of a path.
///
/// Interior points use the direction from the previous to the next point (a
/// mitered joint), the endpoints use the direction of their only adjacent
/// segment, and closed paths wrap around.  Points whose neighbourhood is
/// degenerate (coincident points) inherit the previous direction, and a path
/// that has no direction at all reports `(0, 0, 1)`.
///
/// @param path   Points of the path.
/// @param closed Whether the path loops back to its first point.
/// @return One unit vector per input point (empty for fewer than two points).
///
/// @see path_curvature()
inline std::vector<Vec3> path_tangents(const std::vector<Vec3> &path,
                                       bool closed = false) {
    std::vector<Vec3> tangents;
    const size_t n = path.size();
    if (static_cast<int>(n) < static_cast<int>(detail::kMinPathPoints)) {
        return tangents;
    }
    tangents.resize(n);

    for (size_t i = 0; i < n; ++i) {
        Vec3 dir;
        if (closed) {
            const Vec3 prev = path[(i + n - 1u) % n];
            const Vec3 next = path[(i + 1u) % n];
            dir = next - prev;
        } else if (i == 0u) {
            dir = path[1] - path[0];
        } else if (i + 1u == n) {
            dir = path[n - 1u] - path[n - 2u];
        } else {
            dir = path[i + 1u] - path[i - 1u];
        }

        const float len = glm::length(dir);
        if (len > 1e-8f) {
            tangents[i] = dir / len;
        } else if (i > 0u) {
            tangents[i] = tangents[i - 1u];
        } else {
            tangents[i] = Vec3(0.0f, 0.0f, 1.0f);
        }
    }
    return tangents;
}

/// @brief Discrete curvature at every point of a path.
///
/// Estimates the curvature with the standard formula
/// `kappa = |x' x x''| / |x'|^3`, which is invariant under reparameterization
/// (so it does not care how the points are spaced), using central differences
/// for the two derivatives.  With `v = P[i+1] - P[i-1]` and
/// `a = P[i+1] - 2*P[i] + P[i-1]` this reduces to `4 * |v x a| / |v|^3`: the
/// derivatives are `x' = v / (2h)` and `x'' = a / h^2`, so the `h`-dependence
/// cancels up to the factor that comes from the central first difference.
/// (Dropping that factor makes every curvature four times too small, which is
/// invisible on a curve whose curvature you are guessing at and very visible on
/// a circle, where the estimate has to come out as `1 / radius`.)
///
/// The endpoints of an open path report the value of their only neighbour,
/// since curvature is not defined from one side.
///
/// This is a diagnostic tool rather than a rendering input: sweeping a tube of
/// radius `r` along a curve whose curvature reaches `kappa` folds the tube
/// inside out, so a path is safe up to a radius of `1 / max(kappa)` (see
/// generate_tube()).
///
/// @param path   Points of the path.
/// @param closed Whether the path loops back to its first point.
/// @return One curvature value per input point (empty for fewer than three
///         points, where curvature is undefined).
///
/// @par Example
/// @code{.cpp}
/// // A circle of radius 2: curvature is 0.5 everywhere.
/// std::vector<float> k = path_curvature(circle_points, true);
/// @endcode
///
/// @see generate_tube(), path_tangents()
inline std::vector<float> path_curvature(const std::vector<Vec3> &path,
                                         bool closed = false) {
    const size_t n = path.size();
    if (n < 3u) {
        return std::vector<float>();
    }

    std::vector<float> curvature(n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        if (!closed && (i == 0u || i + 1u == n)) {
            continue;   // filled in below from the neighbouring interior point.
        }
        const Vec3 prev = path[closed ? (i + n - 1u) % n : i - 1u];
        const Vec3 next = path[closed ? (i + 1u) % n : i + 1u];

        const Vec3 v = next - prev;                       // ~ x' / (2h)
        const Vec3 a = next - 2.0f * path[i] + prev;      // ~ x'' / h^2
        const float v_len = glm::length(v);
        if (v_len < 1e-8f) {
            continue;
        }
        const float cross_len = glm::length(glm::cross(v, a));
        curvature[i] = (4.0f * cross_len) /
                       (v_len * v_len * v_len);
    }

    if (!closed && n >= 3u) {
        curvature[0] = curvature[1];
        curvature[n - 1u] = curvature[n - 2u];
    }
    return curvature;
}

// ---------------------------------------------------------------------------
//  Resampling
// ---------------------------------------------------------------------------

/// @brief Resample a path at a fixed arc-length step.
///
/// Walks along the polyline and emits a point every `step` units of arc length,
/// starting at the first point.  The result has (nearly) uniform point spacing
/// regardless of how the input was parameterized, which is what makes a swept
/// tube look even: generate_tube() places exactly one cross-section per path
/// point, so uneven spacing means a tube that is finely subdivided in one place
/// and faceted in another.
///
/// The sample count is driven by the length, not by the input: a long straight
/// run produces many points and a tight bend few, exactly as a constant-speed
/// traversal would.  An open path always keeps its final point (the last step
/// is shortened by whatever is left over).  A closed path ends with a copy of
/// its first point, like the closed curves of the sampling functions, and its
/// step is adjusted to the exact loop length while doing so: the remainder of
/// the division would otherwise collect into a single arbitrary gap at the
/// seam, and a short enough gap there means two overlapping cross-sections in a
/// swept tube.  The adjustment is at most half a step divided by the number of
/// samples (under 1 % from 50 samples on).
///
/// @note This is a reparameterization, not a resampling filter: the emitted
///       points lie *on* the input segments, so a corner between two input
///       segments is cut off if it does not happen to fall on a sample
///       boundary.  Resample the output of a spline (where the "corners" are
///       sampling artifacts), not a hand-written polyline whose vertices are
///       meaningful.  The spacing that is uniform is the **arc length** along
///       the path; the straight-line distances between consecutive samples are
///       slightly shorter wherever the path curves, by the usual chord-versus-arc
///       difference (0.7 % at a step of 0.25 on a curve of radius 1).
///
/// @param path   Points of the path.
/// @param step   Arc length between samples (> 0; other values return the path
///               unchanged).
/// @param closed Whether the path loops back to its first point.
/// @return The resampled path.
///
/// @par Example
/// @code{.cpp}
/// std::vector<Vec3> smooth = catmull_rom_path(waypoints, 8);
/// std::vector<Vec3> even = resample_by_arclength(smooth, 0.25f);
/// @endcode
///
/// @see path_length(), catmull_rom_path()
inline std::vector<Vec3> resample_by_arclength(const std::vector<Vec3> &path,
                                               float step,
                                               bool closed = false) {
    const size_t n = path.size();
    if (step <= 0.0f || n < 2u) {
        return path;
    }

    const double total = path_length(path, closed);
    if (total <= 0.0) {
        return std::vector<Vec3>(1u, path[0]);
    }

    // A closed loop is resampled at the step that covers it in a whole number
    // of steps, so that the seam is a regular step like every other one.
    double step_used = static_cast<double>(step);
    if (closed) {
        const double steps = std::max(
            3.0, std::round(total / static_cast<double>(step)));
        step_used = total / steps;
    }

    std::vector<Vec3> out;
    out.reserve(static_cast<size_t>(std::ceil(total / step_used)) + 2u);
    out.push_back(path[0]);

    const double kStep = step_used;
    const double eps = detail::kArcLengthEpsilon * total;
    const size_t num_segments = detail::segment_count(n, closed);

    double travelled = 0.0;   // arc length consumed before the current segment
    double next_at = kStep;   // arc length of the next sample
    for (size_t i = 0; i < num_segments; ++i) {
        const Vec3 a = path[i];
        const Vec3 b = path[detail::next_index(i, n, closed)];
        const double seg_len = glm::length(b - a);

        if (seg_len > 0.0) {
            while (next_at <= travelled + seg_len && next_at < total - eps) {
                const double u = (next_at - travelled) / seg_len;
                out.push_back(a + static_cast<float>(u) * (b - a));
                next_at += kStep;
            }
        }
        travelled += seg_len;
    }

    // The end of an open path is part of the shape, even when it does not fall
    // on a sample boundary; a closed path closes on its starting point.
    out.push_back(closed ? path[0] : path[n - 1u]);
    return out;
}

// ---------------------------------------------------------------------------
//  Curve sampling
// ---------------------------------------------------------------------------

/// @brief Sample a cubic Hermite curve with caller-supplied tangents.
///
/// The general form of every curve in this header: the curve passes through
/// every input point, and the shape between two points is a cubic that is
/// fully determined by the two points and their two tangent vectors.  Control
/// over the tangents is what lets you make a path leave a point in a direction
/// that is meaningful for your data (a heading, a surface normal, a symmetry
/// axis) rather than the one a global rule picks for you.
///
/// Tangents are given in the units of the local segment parameter, i.e. laying
/// `tangents[i]` end to end at `points[i]` points in the direction the curve
/// leaves the point.  Each segment is parameterized over `[0, 1]` independently,
/// so a segment with very different length than its neighbours will show a
/// visible change of speed — which does not matter for a swept tube (generate_tube()
/// only uses the positions), but does matter if you sample the curve for
/// constant-speed motion.
///
/// @param points   Points the curve has to pass through (at least 2).
/// @param tangents One tangent vector per point (same size as `points`).
/// @param samples_per_segment Samples per segment (clamped to at least 1).
/// @param closed   Whether the curve loops back to its first point.
/// @return The sampled path, or an empty vector if the input is unusable
///         (fewer than two points, mismatched tangent count, or a closed curve
///         with fewer than three points).
///
/// @par Example
/// @code{.cpp}
/// std::vector<Vec3> pts = {{0,0,0}, {1,0,0}, {2,0,0}};
/// std::vector<Vec3> tan = {{0,0,1}, {0,0,1}, {0,0,1}};  // leave each point "upward"
/// std::vector<Vec3> path = hermite_path(pts, tan, 8);
/// @endcode
///
/// @see catmull_rom_path()
inline std::vector<Vec3> hermite_path(const std::vector<Vec3> &points,
                                      const std::vector<Vec3> &tangents,
                                      int samples_per_segment = 8,
                                      bool closed = false) {
    std::vector<Vec3> path;
    const std::vector<Vec3> pts = remove_duplicate_points(points);
    if (pts.size() != points.size() || tangents.size() != points.size()) {
        return path;   // caller passed degenerate points or mismatched sizes.
    }
    if (pts.size() < detail::kMinPathPoints ||
        (closed && pts.size() < 3u)) {
        return path;
    }

    const int samples = detail::sample_count(samples_per_segment);
    const size_t num_segments = detail::segment_count(pts.size(), closed);
    path.reserve(num_segments * static_cast<size_t>(samples) + 1u);

    // Each segment spans one unit of the curve parameter, so h == 1 and the
    // caller's tangents are used as-is.
    for (size_t i = 0; i < num_segments; ++i) {
        const size_t j = detail::next_index(i, pts.size(), closed);
        for (int k = 0; k < samples; ++k) {
            const double s = static_cast<double>(k) / static_cast<double>(samples);
            path.push_back(detail::hermite_segment(pts[i], pts[j], tangents[i],
                                                   tangents[j], 1.0, s));
        }
    }
    if (closed) {
        path.push_back(pts[0]);   // the loop closes on the first point.
    } else {
        path.push_back(pts.back());
    }
    return path;
}

/// @brief Sample a non-uniform Catmull-Rom curve through the given points.
///
/// The interpolating spline to use when all you have is a sequence of waypoints
/// and no idea what the tangents should be, which is the usual case: the curve
/// passes through **every** input point, and each point's tangent is derived
/// from its two neighbours.
///
/// `alpha` selects the parameterization and is the one knob that matters:
/// - `alpha = 0.5` (default) — **centripetal**.  Knot intervals grow with the
///   square root of the chord length, which prevents the cusps, loops and
///   self-intersections that uniform Catmull-Rom produces when the spacing of
///   the input points is uneven (long segment followed by a short one, the norm
///   for measured data — atom traces, streamlines, digitized paths).
/// - `alpha = 1` — chord length: still free of cusps, but can overshoot more.
/// - `alpha = 0` — uniform: the textbook curve, which is exact on evenly spaced
///   points and misbehaves on everything else.
///
/// The first and last point of an open curve get a reflected phantom neighbour
/// (`2*P0 - P1`, `2*Pn - Pn-1`), so the curve starts and ends exactly at the
/// given points with a tangent along the first/last segment.  Interior points
/// are interpolated with `C1` continuity: the tangent is shared by both
/// adjacent segments, so the joint is smooth but not necessarily curvature
/// continuous.
///
/// @param points Points the curve has to pass through (at least 2; at least 3
///               for a closed curve).
/// @param samples_per_segment Samples per segment (clamped to at least 1).
/// @param closed Whether the curve loops back to its first point.
/// @param alpha  Parameterization exponent (see above).
/// @return The sampled path, or an empty vector if there are not enough
///         distinct points.
///
/// @par Example
/// @code{.cpp}
/// std::vector<Vec3> waypoints = {{0,0,0}, {1,1,0}, {2,0,0}, {3,1,0}};
/// std::vector<Vec3> smooth = catmull_rom_path(waypoints, 8);
/// Mesh tube = generate_tube(smooth, 0.1f, 12, Color(0.8f, 0.2f, 0.2f));
/// @endcode
///
/// @see hermite_path(), bspline_path(), generate_tube()
inline std::vector<Vec3> catmull_rom_path(const std::vector<Vec3> &points,
                                          int samples_per_segment = 8,
                                          bool closed = false,
                                          float alpha = 0.5f) {
    std::vector<Vec3> path;
    const std::vector<Vec3> pts = remove_duplicate_points(points);
    const size_t n = pts.size();
    if (n < detail::kMinPathPoints || (closed && n < 3u)) {
        return path;
    }

    // Knot intervals: the parameter distance between two neighbouring points.
    // For alpha = 0 every interval is 1 (the uniform case), otherwise it grows
    // with the chord length to the power of alpha.
    std::vector<double> interval(n, 1.0);
    for (size_t i = 0; i < n; ++i) {
        const size_t j = detail::next_index(i, n, closed);
        if (!closed && j == i) {
            break;   // open curve: the last point has no outgoing segment.
        }
        const double chord = glm::length(pts[j] - pts[i]);
        interval[i] = std::max(
            (alpha == 0.0f) ? 1.0 : std::pow(chord, static_cast<double>(alpha)),
            detail::kMinKnotInterval);
    }

    // Cumulative knot values along the curve.
    std::vector<double> knot(n, 0.0);
    for (size_t i = 1; i < n; ++i) {
        knot[i] = knot[i - 1u] + interval[i - 1u];
    }

    // Tangent at each control point: the central difference of its neighbours,
    // divided by their knot distance (which is what makes the formula work for
    // the non-uniform parameterizations).
    std::vector<Vec3> tangents(n);
    for (size_t i = 0; i < n; ++i) {
        Vec3 prev, next;
        double t_prev, t_next;
        if (closed) {
            prev = pts[(i + n - 1u) % n];
            next = pts[(i + 1u) % n];
            t_prev = knot[i] - interval[(i + n - 1u) % n];
            t_next = knot[i] + interval[i];
        } else if (i == 0u) {
            // Reflected phantom point: keeps the curve starting at pts[0] with
            // the tangent direction of the first segment.
            prev = 2.0f * pts[0] - pts[1];
            next = pts[1];
            t_prev = knot[0] - interval[0];
            t_next = knot[1];
        } else if (i + 1u == n) {
            next = 2.0f * pts[n - 1u] - pts[n - 2u];
            prev = pts[n - 2u];
            t_prev = knot[n - 2u];
            t_next = knot[n - 1u] + interval[n - 2u];
        } else {
            prev = pts[i - 1u];
            next = pts[i + 1u];
            t_prev = knot[i - 1u];
            t_next = knot[i + 1u];
        }

        const double span = t_next - t_prev;
        if (span > detail::kMinKnotInterval) {
            tangents[i] = (next - prev) / static_cast<float>(span);
        }
    }

    const int samples = detail::sample_count(samples_per_segment);
    const size_t num_segments = detail::segment_count(n, closed);
    path.reserve(num_segments * static_cast<size_t>(samples) + 1u);

    for (size_t i = 0; i < num_segments; ++i) {
        const size_t j = detail::next_index(i, n, closed);
        const double h = (j == 0u) ? (interval[n - 1u]) : (knot[j] - knot[i]);
        for (int k = 0; k < samples; ++k) {
            const double s = static_cast<double>(k) / static_cast<double>(samples);
            path.push_back(detail::hermite_segment(pts[i], pts[j], tangents[i],
                                                   tangents[j], h, s));
        }
    }
    path.push_back(closed ? pts[0] : pts[n - 1u]);
    return path;
}

/// @brief Sample a uniform cubic B-spline through the given points.
///
/// The **approximating** counterpart of catmull_rom_path(): the curve is
/// smoother (`C2` continuous everywhere, including at the joints) because it
/// does not have to pass through the control points, which it treats as a
/// convex-hull cage it stays inside.  Use it when the input points are noisy
/// and you want the curve to iron out the noise rather than reproduce it, or
/// when you want the guaranteed smoothness of a single polynomial piece instead
/// of a chain of cubics.
///
/// Note the consequence: the sampled path generally does **not** contain the
/// input points, and an open curve starts and ends inside the first and last
/// segment instead of at the first and last point.  Use catmull_rom_path() when
/// the waypoints are meaningful positions that the curve has to hit.
///
/// @param points Control points (at least 4).
/// @param samples_per_segment Samples per segment (clamped to at least 1).
/// @param closed Whether the curve loops back to its first point.
/// @return The sampled path, or an empty vector if there are fewer than four
///         distinct control points.
///
/// @par Example
/// @code{.cpp}
/// // A smoothed (noise-reduced) loop drawn as a tube:
/// std::vector<Vec3> path = bspline_path(noisy_points, 8, true);
/// Mesh ring = generate_tube(path, 0.05f, 10, Color(0.9f, 0.9f, 0.2f));
/// @endcode
///
/// @see catmull_rom_path()
inline std::vector<Vec3> bspline_path(const std::vector<Vec3> &points,
                                      int samples_per_segment = 8,
                                      bool closed = false) {
    std::vector<Vec3> path;
    const std::vector<Vec3> pts = remove_duplicate_points(points);
    const size_t n = pts.size();
    if (n < 4u) {
        return path;   // a cubic B-spline needs four control points per piece.
    }

    const int samples = detail::sample_count(samples_per_segment);

    // Uniform cubic B-spline basis, as a function of the local parameter s in
    // [0, 1] within one knot span.
    const auto basis = [](double s, double &b0, double &b1, double &b2, double &b3) {
        const double s2 = s * s;
        const double s3 = s2 * s;
        b0 = (1.0 - 3.0 * s + 3.0 * s2 - s3) / 6.0;
        b1 = (4.0 - 6.0 * s2 + 3.0 * s3) / 6.0;
        b2 = (1.0 + 3.0 * s + 3.0 * s2 - 3.0 * s3) / 6.0;
        b3 = s3 / 6.0;
    };

    // Segment `i` is controlled by the points i-3 .. i (wrapped for a closed
    // curve): a uniform cubic B-spline is defined on the knot span [3, n] of
    // the uniform knot vector, which is n - 3 segments for an open curve and n
    // for a closed one.  For the open curve, segment i therefore uses the
    // points i .. i+3 directly.
    const size_t num_segments = closed ? n : (n - 3u);
    path.reserve(num_segments * static_cast<size_t>(samples) + 1u);

    for (size_t i = 0; i < num_segments; ++i) {
        const Vec3 &p0 = pts[closed ? (i + n - 3u) % n : i];
        const Vec3 &p1 = pts[closed ? (i + n - 2u) % n : i + 1u];
        const Vec3 &p2 = pts[closed ? (i + n - 1u) % n : i + 2u];
        const Vec3 &p3 = pts[closed ? i : i + 3u];

        // The last segment of an open curve also emits its endpoint (s = 1),
        // so that the path ends where the curve does.
        const int emit = (!closed && i + 1u == num_segments) ? samples + 1 : samples;
        for (int k = 0; k < emit; ++k) {
            const double s = static_cast<double>(k) / static_cast<double>(samples);
            double b0, b1, b2, b3;
            basis(s, b0, b1, b2, b3);
            path.push_back(static_cast<float>(b0) * p0 +
                           static_cast<float>(b1) * p1 +
                           static_cast<float>(b2) * p2 +
                           static_cast<float>(b3) * p3);
        }
    }

    // A periodic B-spline returns to its starting point after n knot spans, so
    // the loop closes on the first sample.
    if (closed) {
        path.push_back(path[0]);
    }
    return path;
}

/// @brief Sample a Bezier curve from a control polygon.
///
/// Evaluates the Bernstein form of the curve by de Casteljau's algorithm, which
/// is numerically stable for any number of control points (unlike summing the
/// Bernstein polynomials explicitly).  Unlike the Catmull-Rom and B-spline
/// functions, the control points are **not** points on the curve: only the
/// first and last one are, and the rest pull it like a magnet.  This is the
/// function to use when your input really is a Bezier control polygon (a font
/// outline, a vector-graphics path, a designed shape).
///
/// @param control_points Control polygon (at least 2 points).
/// @param samples Number of points to emit along the curve (at least 2; the
///                samples include both endpoints).
/// @return The sampled path, or an empty vector if there are not enough
///         control points.
///
/// @par Example
/// @code{.cpp}
/// // A quadratic Bezier arc, drawn as a tube:
/// std::vector<Vec3> arc = bezier_path({{0,0,0}, {1,1,0}, {2,0,0}}, 64);
/// Mesh tube = generate_tube(arc, 0.05f, 10, Color(0.2f, 0.6f, 0.9f));
/// @endcode
///
/// @see catmull_rom_path()
inline std::vector<Vec3> bezier_path(const std::vector<Vec3> &control_points,
                                     int samples = 64) {
    std::vector<Vec3> path;
    const size_t n = control_points.size();
    if (n < 2u) {
        return path;
    }

    const int count = std::max(2, samples);
    path.reserve(static_cast<size_t>(count));

    std::vector<Vec3> work(n);
    for (int k = 0; k < count; ++k) {
        // De Casteljau: repeated linear interpolation between neighbouring
        // points converges to the curve point for the current parameter.
        const float u = static_cast<float>(k) / static_cast<float>(count - 1);
        work = control_points;
        for (size_t level = n - 1u; level > 0u; --level) {
            for (size_t i = 0; i < level; ++i) {
                work[i] = (1.0f - u) * work[i] + u * work[i + 1u];
            }
        }
        path.push_back(work[0]);
    }
    return path;
}

} // namespace scimesh
