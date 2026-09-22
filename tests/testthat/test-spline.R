# Splines: smooth curves through ordered 3D points (spline_path(), bezier_path(),
# resample_path(), path_length(), path_curvature()).

# Helper: a path with wildly uneven segment lengths (10, 0.02, 9.98 units).
uneven_line <- function() {
    matrix(c(0, 0, 0, 10, 0, 0, 10.02, 0, 0, 20, 0, 0), ncol = 3, byrow = TRUE)
}

# Helper: `num_points` points on a circle of the given radius in the XY plane.
circle_points <- function(num_points, radius) {
    angles <- seq(0, 2 * pi, length.out = num_points + 1L)[-(num_points + 1L)]
    cbind(radius * cos(angles), radius * sin(angles), 0)
}

test_that("spline_path interpolates the input points with Catmull-Rom", {
    waypoints <- matrix(c(0, 0, 0, 1, 1, 0.5, 2, 0, 1, 3, 1.5, 0),
                        ncol = 3, byrow = TRUE)

    path <- spline_path(waypoints, samples_per_segment = 8)

    expect_true(is.matrix(path))
    expect_equal(ncol(path), 3L)
    # One sample per segment boundary is the control point itself, plus the
    # final point at the end.
    expect_equal(nrow(path), 3L * 8L + 1L)

    for (i in seq_len(nrow(waypoints))) {
        expect_equal(path[(i - 1L) * 8L + 1L, ], waypoints[i, ])
    }
    expect_equal(path[nrow(path), ], waypoints[nrow(waypoints), ])
})

test_that("spline_path keeps a straight line straight", {
    line <- matrix(c(0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0), ncol = 3, byrow = TRUE)
    path <- spline_path(line, samples_per_segment = 16)

    expect_equal(path[, 2], rep(0, nrow(path)), tolerance = 1e-5)
    expect_equal(path[, 3], rep(0, nrow(path)), tolerance = 1e-5)
    expect_true(all(path[, 1] >= -1e-5))
    expect_true(all(path[, 1] <= 3 + 1e-5))
})

test_that("spline_path accepts a single point vector and an empty matrix", {
    expect_equal(nrow(spline_path(c(0, 0, 0))), 0L)
    expect_equal(nrow(spline_path(matrix(numeric(0), nrow = 0L, ncol = 3L))), 0L)
    # Two points are enough for an open curve (a single straight segment).
    expect_equal(nrow(spline_path(matrix(c(0, 0, 0, 1, 0, 0), ncol = 3,
                                        byrow = TRUE), samples_per_segment = 4)),
                 5L)
})

test_that("spline_path closes a loop on its first point", {
    square <- matrix(c(0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0), ncol = 3, byrow = TRUE)

    open <- spline_path(square, samples_per_segment = 4)
    closed <- spline_path(square, samples_per_segment = 4, closed = TRUE)

    expect_false(isTRUE(all.equal(open[1, ], open[nrow(open), ])))
    expect_equal(closed[1, ], closed[nrow(closed), ])
    expect_equal(nrow(closed), 4L * 4L + 1L)

    # A closed curve needs three distinct points.
    line <- matrix(c(0, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE)
    expect_equal(nrow(spline_path(line, closed = TRUE)), 0L)
})

test_that("spline_path can approximate instead of interpolate", {
    square <- matrix(c(0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0), ncol = 3, byrow = TRUE)

    path <- spline_path(square, method = "bspline", samples_per_segment = 8)

    expect_true(nrow(path) > 0L)
    # The convex hull property: the curve stays inside the bounding box of its
    # control points ...
    expect_true(all(path >= -1e-4))
    expect_true(all(path <= 1 + 1e-4))
    # ... but does not pass through the interior control points.
    distances <- apply(path, 1L, function(p) sqrt(sum((p - square[2, ])^2)))
    expect_true(min(distances) > 0.05)

    # Four control points is the minimum for a cubic B-spline.
    expect_equal(nrow(spline_path(square[1:3, ], method = "bspline")), 0L)
})

test_that("spline_path validates its arguments", {
    points <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE)

    expect_error(spline_path(NULL), "must not be NULL")
    expect_error(spline_path(matrix(1:6, ncol = 2L)), "Nx3 numeric matrix")
    expect_error(spline_path(points, method = "nope"), "should be one of")
    expect_error(spline_path(points, samples_per_segment = 0), ">= 1")
    expect_error(spline_path(points, samples_per_segment = NA), ">= 1")
    expect_error(spline_path(points, closed = NA), "TRUE or FALSE")
    expect_error(spline_path(points, alpha = -1), "non-negative")
    expect_error(spline_path(points, alpha = c(0.5, 0.5)), "non-negative")
})

test_that("bezier_path samples a control polygon", {
    control <- matrix(c(0, 0, 0, 1, 2, 0, 3, 1, 0, 4, 0, 0), ncol = 3, byrow = TRUE)

    path <- bezier_path(control, samples = 32)

    expect_equal(dim(path), c(32L, 3L))
    expect_equal(path[1, ], control[1, ])
    expect_equal(path[32, ], control[4, ])

    # A quadratic curve hits (P0 + 2*P1 + P2) / 4 at its midpoint.
    quadratic <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
    mid <- bezier_path(quadratic, samples = 3L)[2, ]
    expect_equal(mid, c(1, 0.5, 0), tolerance = 1e-5)

    expect_equal(nrow(bezier_path(matrix(c(1, 2, 3), ncol = 3))), 0L)
    expect_error(bezier_path(control, samples = 0), ">= 1")
})

test_that("resample_path evens out unevenly spaced points", {
    even <- resample_path(uneven_line(), step = 0.25)

    expect_true(nrow(even) > 80L)
    expect_equal(even[1, ], c(0, 0, 0))
    expect_equal(even[nrow(even), ], c(20, 0, 0))

    steps <- sqrt(rowSums((even[-1, ] - even[-nrow(even), ])^2))
    # The straight input has no corners to cut, so the spacing is exact (the
    # last step is the leftover up to the endpoint and is not checked).
    expect_true(all(abs(steps[-length(steps)] - 0.25) < 1e-3))

    # The input really was uneven, so this is not a trivial statement.
    input_steps <- sqrt(rowSums((uneven_line()[-1, ] -
                                 uneven_line()[-nrow(uneven_line()), ])^2))
    expect_true(max(input_steps) > 100 * min(input_steps))
})

test_that("resample_path closes a loop evenly", {
    # Resample the smooth curve, not the raw polygon: cutting the corners of a
    # polygon shortens the chords between samples, which is documented.
    smooth <- spline_path(circle_points(8L, 1), samples_per_segment = 16,
                          closed = TRUE)
    loop <- resample_path(smooth, step = 0.25, closed = TRUE)

    expect_equal(loop[1, ], loop[nrow(loop), ])
    steps <- sqrt(rowSums((loop[-1, ] - loop[-nrow(loop), ])^2))
    expect_true(all(abs(steps - 0.25) < 0.01))
})

test_that("resample_path handles degenerate input", {
    line <- matrix(c(0, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE)

    # A non-positive step means "no resampling".
    expect_equal(resample_path(line, step = 0), line)
    expect_equal(resample_path(line, step = -1), line)

    expect_equal(nrow(resample_path(c(0, 0, 0), step = 0.5)), 1L)
    expect_equal(nrow(resample_path(matrix(numeric(0), nrow = 0L, ncol = 3L),
                                    step = 0.5)), 0L)
    expect_error(resample_path(line, step = NA), "single finite number")
    expect_error(resample_path(line, step = "a"), "single finite number")
})

test_that("path_length measures a path", {
    triangle <- matrix(c(0, 0, 0, 3, 0, 0, 3, 4, 0), ncol = 3, byrow = TRUE)

    expect_equal(path_length(triangle), 7)
    expect_equal(path_length(triangle, closed = TRUE), 12)
    expect_equal(path_length(c(1, 2, 3)), 0)
    expect_equal(path_length(matrix(numeric(0), nrow = 0L, ncol = 3L)), 0)
})

test_that("path_curvature reports the curvature of a circle", {
    radius <- 2
    points <- circle_points(64L, radius)

    k <- path_curvature(points, closed = TRUE)

    expect_equal(length(k), 64L)
    expect_true(all(abs(k - 1 / radius) < 0.01))

    # A straight line has none, and the endpoints of an open path report the
    # value of their neighbour.
    line <- matrix(c(0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0), ncol = 3, byrow = TRUE)
    expect_true(all(path_curvature(line) < 1e-6))

    open <- path_curvature(points)
    expect_equal(open[1], open[2])
    expect_equal(length(path_curvature(line[1:2, ])), 0L)
})

test_that("spline paths feed the geometry generators", {
    waypoints <- matrix(c(0, 0, 0, 1, 1, 0.5, 2, 0, 1, 3, 1.5, 0),
                        ncol = 3, byrow = TRUE)
    path <- spline_path(waypoints, samples_per_segment = 8)
    segments <- 12L

    tube <- generate_tube(path, radius = 0.1, segments = segments)

    # One ring per path point, plus a cap at each end.
    expect_equal(nrow(tube$vertices), nrow(path) * segments + 2L * (1L + segments))
    expect_true(all(is.finite(tube$vertices)))

    # Unevenly spaced path points give an evenly spaced tube ring pattern.
    even <- resample_path(path, step = 0.25)
    expect_true(nrow(generate_tube(even, radius = 0.05)$vertices) > 0L)

    # And the spline output works as a line layer, too.
    layer <- line_layer(path[-nrow(path), ], path[-1, ])
    expect_s3_class(layer, "scimesh_lines")
    expect_equal(nrow(layer$from), nrow(path) - 1L)
})
