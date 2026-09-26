# Smooth curves through ordered 3D points (splines).
#
# These are the R bindings of the C++ spline module (src/core/scimesh/spline.h).
# The functions are pure geometry: they take and return Nx3 point matrices and
# know nothing about meshes, scenes or the renderer, which is what makes them
# usable both for the geometry generators (generate_tube(), line_layer()) and
# for anything else that needs a smooth path (camera animation, trajectories).
#
# A "path" is an Nx3 numeric matrix of points in order, the same convention the
# generators use.  A closed path is expressed by ending on a copy of its first
# point, so a swept tube closes on itself.

#' Smooth curve through ordered 3D points
#'
#' Turns a coarse list of waypoints into a dense, smooth path, which is what the
#' path-taking geometry functions expect: \code{\link{generate_tube}} sweeps a
#' cross-section along a path, and \code{\link{line_layer}} draws one straight
#' segment per consecutive pair of points.  Handing a handful of control points
#' to those gives a visibly faceted tube and a polygonal line; this is how you
#' get the smooth version.
#'
#' Two curves are available, and they differ in a way that matters:
#' \itemize{
#'   \item \code{"catmull-rom"} (the default) **interpolates**: the curve passes
#'     through every input point, and each point's tangent is derived from its
#'     two neighbours.  This is what you want when the input points are
#'     positions that the curve has to hit (measured or digitized data).
#'   \item \code{"bspline"} **approximates**: the points act as a control cage
#'     that the curve stays inside, which irons out noise instead of reproducing
#'     it, and the curve is \eqn{C^2} continuous everywhere.  It generally does
#'     not pass through the input points.
#' }
#'
#' For the Catmull-Rom curve, \code{alpha} selects the parameterization and is
#' the knob that matters most on real data:
#' \itemize{
#'   \item \code{0.5} (default) — *centripetal*.  Knot intervals grow with the
#'     square root of the chord length, which prevents the cusps, loops and
#'     self-intersections that the uniform curve produces when the spacing of
#'     the input points is uneven (long segment followed by a short one, the
#'     norm for measured data).
#'   \item \code{1} — chord length: still free of cusps, but can overshoot more.
#'   \item \code{0} — uniform: the textbook curve, exact on evenly spaced points
#'     and misbehaving on everything else.
#' }
#'
#' @param points Nx3 numeric matrix of points the curve has to pass through (or
#'   a length-3 vector for a single point).  An open curve needs at least 2
#'   points, a closed one at least 3 (\code{"bspline"}: at least 4).
#' @param method Either \code{"catmull-rom"} (interpolating, default) or
#'   \code{"bspline"} (approximating, smoother; see the description).
#' @param samples_per_segment Number of points to generate per segment between
#'   two input points (default 8, clamped to at least 1).  This is the knob that
#'   controls how smooth the result looks when rendered.
#' @param closed Whether the curve loops back to its first point (default
#'   \code{FALSE}).  A closed path ends on a copy of its first point.
#' @param alpha Parameterization exponent of the Catmull-Rom curve (default
#'   \code{0.5}, see the description).  Ignored for \code{"bspline"}.
#' @return An Nx3 numeric matrix of path points, ready for
#'   \code{\link{generate_tube}}, \code{\link{generate_tubes}},
#'   \code{\link{line_layer}} or \code{\link{camera_auto}}.  An empty (0-row)
#'   matrix if there are not enough distinct points for the chosen curve.
#'
#' @examples
#' waypoints <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0),
#'                     ncol = 3, byrow = TRUE)
#' path <- spline_path(waypoints, samples_per_segment = 8)
#' nrow(path)  # 3 segments * 8 samples + the final point
#'
#' # A smooth tube through the waypoints, instead of a faceted one:
#' mesh <- generate_tube(waypoints, radius = 0.1)
#' smooth <- generate_tube(path, radius = 0.1)
#'
#' # An approximating (noise-reducing) closed loop:
#' loop <- spline_path(waypoints, method = "bspline", closed = TRUE)
#'
#' @seealso \code{\link{bezier_path}}, \code{\link{resample_path}},
#'   \code{\link{path_curvature}}, \code{\link{generate_tube}}
#' @export
spline_path <- function(points, method = c("catmull-rom", "bspline"),
                        samples_per_segment = 8L, closed = FALSE,
                        alpha = 0.5) {
    points <- check_points_matrix(points, "points")
    method <- match.arg(method)
    samples_per_segment <- check_sample_count(samples_per_segment)
    closed <- check_flag(closed, "closed")

    if (identical(method, "bspline")) {
        return(scimesh_bspline_path(points, samples_per_segment, closed))
    }
    scimesh_catmull_rom_path(points, samples_per_segment, closed,
                             check_alpha(alpha))
}

#' Sample a Bezier curve from a control polygon
#'
#' Evaluates a Bezier curve of any degree with de Casteljau's algorithm.  Unlike
#' \code{\link{spline_path}}, the control points are **not** points on the curve:
#' only the first and the last one are, and the rest pull the curve like a
#' magnet.  This is the function to use when the input really is a control
#' polygon (a font outline, a vector graphics path, a designed shape); use
#' \code{\link{spline_path}} when your points are positions the curve should
#' pass through.
#'
#' @param control_points Nx3 numeric matrix of control points (at least 2).
#' @param samples Number of points to emit along the curve (default 64, at least
#'   2).  Both endpoints are included in the result.
#' @return An Nx3 numeric matrix of path points, or an empty (0-row) matrix if
#'   there are fewer than two control points.
#'
#' @examples
#' # A quadratic Bezier arc, drawn as a tube:
#' arc <- bezier_path(matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE))
#' nrow(arc)
#'
#' @seealso \code{\link{spline_path}}, \code{\link{generate_tube}}
#' @export
bezier_path <- function(control_points, samples = 64L) {
    control_points <- check_points_matrix(control_points, "control_points")
    scimesh_bezier_path(control_points, check_sample_count(samples))
}

#' Resample a path at a fixed arc length step
#'
#' Walks along a path and emits a point every \code{step} units of arc length.
#' The result has (nearly) uniform point spacing regardless of how the input was
#' parameterized, which is what makes a swept tube look even:
#' \code{\link{generate_tube}} places exactly one cross-section per path point,
#' so uneven spacing means a tube that is finely subdivided in one place and
#' faceted in another.  Typical use is to even out the output of
#' \code{\link{spline_path}}, whose samples are evenly spaced in the curve
#' parameter rather than along the curve.
#'
#' The spacing that is uniform is the *arc length* along the path; the
#' straight-line distances between consecutive samples are slightly shorter
#' wherever the path curves.  An open path always keeps its final point (with a
#' possibly shorter last step); a closed path ends on a copy of its first point
#' and is covered in a whole number of steps, so its effective step can differ
#' from the requested one by a fraction of a percent.
#'
#' @param path Nx3 numeric matrix of path points (or a length-3 vector).
#' @param step Arc length between samples.  Must be a single positive number;
#'   other values return the path unchanged.
#' @param closed Whether the path loops back to its first point (default
#'   \code{FALSE}).
#' @return An Nx3 numeric matrix of resampled path points.  A path without
#'   length (or with fewer than two points) is returned as it is.
#'
#' @examples
#' waypoints <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
#' smooth <- spline_path(waypoints, samples_per_segment = 32)
#' even <- resample_path(smooth, step = 0.1)
#' nrow(even)
#'
#' @seealso \code{\link{path_length}}, \code{\link{spline_path}}
#' @export
resample_path <- function(path, step, closed = FALSE) {
    path <- check_points_matrix(path, "path")
    if (!is.numeric(step) || length(step) != 1L || !is.finite(step)) {
        stop("step must be a single finite number")
    }
    scimesh_resample_path(path, as.double(step), check_flag(closed, "closed"))
}

#' Length of a path
#'
#' Sums the distances between consecutive points, plus the closing distance from
#' the last point back to the first for a closed path.  Useful to pick a
#' sensible \code{step} for \code{\link{resample_path}}, or to derive a sampling
#' density from the size of the data.
#'
#' @param path Nx3 numeric matrix of path points (or a length-3 vector).
#' @param closed Whether the path loops back to its first point (default
#'   \code{FALSE}).
#' @return A single number: the arc length of the path (0 for fewer than two
#'   points).
#'
#' @examples
#' path_length(matrix(c(0, 0, 0, 1, 0, 0, 1, 1, 0), ncol = 3, byrow = TRUE))
#'
#' @seealso \code{\link{resample_path}}
#' @export
path_length <- function(path, closed = FALSE) {
    path <- check_points_matrix(path, "path")
    scimesh_path_length(path, check_flag(closed, "closed"))
}

#' Curvature along a path
#'
#' Estimates the curvature at every point from the neighbouring points, using
#' the standard formula \eqn{\kappa = |x' \times x''| / |x'|^3}, which does not
#' care how the points are spaced.  On a circle of radius \eqn{r} the values
#' come out as \eqn{1/r}, and on a straight line as 0.
#'
#' This is a diagnostic tool rather than a rendering input: sweeping a tube of
#' radius \eqn{r} along a curve whose curvature reaches \eqn{\kappa} folds the
#' tube inside out, so a path can be swept safely up to a radius of
#' \eqn{1 / \max(\kappa)}, which is worth checking for tight turns in measured
#' data (\code{max(path_curvature(path))}).
#'
#' @param path Nx3 numeric matrix of path points (or a length-3 vector).
#' @param closed Whether the path loops back to its first point (default
#'   \code{FALSE}).
#' @return A numeric vector with one curvature value per input point.  The
#'   endpoints of an open path report the value of their only neighbour; fewer
#'   than 3 points yield a zero-length vector, since curvature is undefined
#'   there.
#'
#' @examples
#' # Largest tube radius that does not fold this path onto itself:
#' path <- spline_path(matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0),
#'                            ncol = 3, byrow = TRUE))
#' 1 / max(path_curvature(path))
#'
#' @seealso \code{\link{spline_path}}, \code{\link{generate_tube}}
#' @export
path_curvature <- function(path, closed = FALSE) {
    path <- check_points_matrix(path, "path")
    scimesh_path_curvature(path, check_flag(closed, "closed"))
}

# ---- internal validators ----------------------------------------------------

#' Validate a sample count
#'
#' @param samples_per_segment Requested number of samples.
#' @return The value as an integer.
#' @keywords internal
check_sample_count <- function(samples_per_segment) {
    if (!is.numeric(samples_per_segment) || length(samples_per_segment) != 1L ||
        !is.finite(samples_per_segment) || samples_per_segment < 1) {
        stop("samples_per_segment must be a single number >= 1")
    }
    as.integer(samples_per_segment)
}

#' Validate a logical flag
#'
#' @param value Requested value.
#' @param arg_name Name of the argument, used in error messages.
#' @return A single logical.
#' @keywords internal
check_flag <- function(value, arg_name) {
    if (!is.logical(value) || length(value) != 1L || is.na(value)) {
        stop(arg_name, " must be TRUE or FALSE")
    }
    value
}

#' Validate a Catmull-Rom parameterization exponent
#'
#' @param alpha Requested exponent.
#' @return The value as a double.
#' @keywords internal
check_alpha <- function(alpha) {
    if (!is.numeric(alpha) || length(alpha) != 1L || !is.finite(alpha) ||
        alpha < 0) {
        stop("alpha must be a single non-negative number (0, 0.5 or 1)")
    }
    as.double(alpha)
}
