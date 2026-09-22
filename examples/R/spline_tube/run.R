#!/usr/bin/env Rscript
#
# scimesh Example — Splines: smooth curves through ordered 3D points
# -----------------------------------------------------------------
# Renders two scenes, one PNG each:
#
#   1. spline_tube_compare.png  the same waypoints swept as a tube twice, once
#                               through the raw polyline (faceted, grey) and
#                               once through a Catmull-Rom spline of it
#                               (smooth, blue)
#   2. spline_tube_knot.png     a closed trefoil knot, i.e. a *closed*
#                               Catmull-Rom curve through 12 waypoints,
#                               resampled to an even spacing and swept into a
#                               tube
#
# It also prints the path diagnostics: the path length, the maximum curvature,
# and the largest tube radius that does not fold the tube onto itself.
#
# Usage:
#   Rscript examples/R/spline_tube/run.R
#
# Output PNG files are written to the current directory.

library(scimesh)

studio_options <- function(width, height) {
    render_options(
        width = width, height = height,
        background_color = c(1, 1, 1, 1),
        ambient = 0.25, specular_color = c(1, 1, 1, 1), shininess = 48,
        aa_samples = 2,
        lights = list(
            list(position = c(0.5, 1.0, 1.0), color = c(1.00, 0.97, 0.90, 1),
                 intensity = 1.9),
            list(position = c(-1.0, 0.2, 0.3), color = c(0.50, 0.60, 0.80, 1),
                 intensity = 0.6),
            list(position = c(0.0, -0.4, -1.0), color = c(0.70, 0.65, 0.65, 1),
                 intensity = 0.5)
        )
    )
}

# Print the path diagnostics and return the maximum curvature, which bounds the
# tube radius that can be swept along the path without self-intersection.
report_path <- function(label, path, closed = FALSE) {
    curvature <- path_curvature(path, closed = closed)
    max_curvature <- if (length(curvature) == 0) 0 else max(curvature)
    cat(sprintf("  %s: %d points, length %.4f, max curvature %.4f\n",
                label, nrow(path), path_length(path, closed = closed),
                max_curvature))
    if (max_curvature > 0) {
        cat(sprintf("    a tube stays clean up to radius %.4f\n",
                    1 / max_curvature))
    }
    max_curvature
}

# ------------------------------------------------------------- Scene 1 ------
cat("\n=== Scene 1: polyline tube vs spline tube ===\n")

# Nine waypoints sampled from a smooth wave, coarse enough that the raw
# polyline is visibly faceted.
x <- seq(0, 6, length.out = 9)
waypoints <- cbind(x, 1.0 * sin(2 * pi * x / 3), 0.4 * cos(2 * pi * x / 4.5))

smooth <- spline_path(waypoints, samples_per_segment = 24)
even <- resample_path(smooth, step = 0.08)

invisible(report_path("waypoints", waypoints))
invisible(report_path("spline", smooth))
limit <- report_path("resampled", even)

# Sweep the tube below the curvature limit, or the tube folds onto itself on
# the inside of the tightest bend.
radius <- 0.7 / limit

# Left: the raw waypoints, swept as-is.  The tube looks like a bent pipe with
# hard joints, because the sweep only sees straight segments.
faceted <- translate_mesh(generate_tube(waypoints, radius = radius, segments = 14,
                                        color = c(0.35, 0.35, 0.38, 1)),
                          c(0, 0, -2.6))
# Right: the same sweep fed with a spline through those waypoints.
curved <- translate_mesh(generate_tube(even, radius = radius, segments = 14,
                                       color = c(0.15, 0.45, 0.85, 1)),
                         c(0, 0, 2.6))

cam <- camera_auto(list(faceted, curved), direction = c(0, 0.35, -1),
                   up = c(0, 1, 0), fov = 40, margin = 1.08)
cat("  Rendering spline_tube_compare.png...\n")
write_png(render_scene(list(faceted, curved), cam, studio_options(1000, 700)),
          "spline_tube_compare.png")

# ------------------------------------------------------------- Scene 2 ------
cat("\n=== Scene 2: closed trefoil knot ===\n")

t <- seq(0, 2 * pi, length.out = 13)[-(13)]
trefoil <- cbind(sin(t) + 2 * sin(2 * t), cos(t) - 2 * cos(2 * t), -sin(3 * t))

loop <- spline_path(trefoil, samples_per_segment = 24, closed = TRUE)
knot_path <- resample_path(loop, step = 0.06, closed = TRUE)

invisible(report_path("waypoints", trefoil, closed = TRUE))
invisible(report_path("closed spline", loop, closed = TRUE))
limit <- report_path("resampled", knot_path, closed = TRUE)

# The strands of the knot pass close to each other, so this one stays well
# below the curvature limit (which only guards against folding the tube onto
# *itself*, not against two strands touching).
knot <- generate_tube(knot_path, radius = 0.3 / limit, segments = 12,
                      color = c(0.85, 0.35, 0.15, 1),
                      cap_start = FALSE, cap_end = FALSE)

cam <- camera_auto(list(knot), direction = c(0.2, 0.7, -1), up = c(0, 0, 1),
                   fov = 40, margin = 1.1)
cat("  Rendering spline_tube_knot.png...\n")
write_png(render_scene(list(knot), cam, studio_options(900, 900)),
          "spline_tube_knot.png")

cat("\nDone.\n")
