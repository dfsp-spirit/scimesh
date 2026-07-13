#!/usr/bin/env Rscript
#
# scimesh Example — All Primitives Gallery
# -----------------------------------------
# Renders every procedural geometry primitive in both flat-shaded and
# wireframe mode, side by side, stacked vertically into a single image.
#
# Usage:
#   Rscript examples/R/primitives/run.R
#
# Output PNG file is written to the current directory:
#   scimesh_primitives.png

library(scimesh)

opts_shaded <- render_options(width = 300L, height = 300L, shading = "flat",
                              background_color = c(0.92, 0.93, 0.95, 1))
opts_wire   <- render_options(width = 300L, height = 300L, wireframe = TRUE,
                              wireframe_color = c(0, 0, 0, 1),
                              background_color = c(0.92, 0.93, 0.95, 1))

render_pair <- function(mesh, cam_dir = c(1.2, 0.8, 1), label = "") {
    cam <- camera_auto(mesh, direction = cam_dir, fov = 40, margin = 1.15)
    shaded <- render_mesh(mesh$vertices, mesh$triangles,
                          colors = mesh$colors, camera = cam,
                          options = opts_shaded)
    wire   <- render_mesh(mesh$vertices, mesh$triangles,
                          colors = mesh$colors, camera = cam,
                          options = opts_wire)
    pair <- stack_horizontal(shaded, wire)
    cat(sprintf("  %-12s rendered\n", label))
    pair
}

.msh <- function(raw) list(vertices = raw$vertices, triangles = raw$triangles,
                           colors = raw$colors)

cat("Generating primitives:\n")

cuboid <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.2, 0.6, 1.0, 1.0))

sphere <- .msh(scimesh:::scimesh_generate_multi_spheres(
    rbind(c(0, 0, 0)), 1.2, matrix(c(0.9, 0.3, 0.2, 1.0), nrow = 1), 32L))

cyl <- .msh(scimesh:::scimesh_generate_multi_cylinders(
    rbind(c(0, -1, 0)), rbind(c(0, 1, 0)), 0.5,
    matrix(c(0.1, 0.7, 0.3, 1.0), nrow = 1), 32L))

cone <- .msh(scimesh:::scimesh_generate_multi_cylinders(
    rbind(c(0, -1.2, 0)), rbind(c(0, 1.2, 0)), 0.01,
    matrix(c(0.9, 0.7, 0.1, 1.0), nrow = 1), 32L))

pyramid <- generate_pyramid(c(0, 0, 0), c(0, 1.5, 0), 1,
                            c(0.7, 0.2, 0.8, 1.0))

tetra <- generate_tetrahedron(c(-1, -0.5, -1), c(1, -0.5, -1),
                               c(0, -0.5, 1), c(0, 1.2, 0),
                               c(0.2, 0.8, 0.8, 1.0))

torus <- generate_torus(c(0, 0, 0), 1.0, 0.35, 24, 12,
                        c(0.6, 0.4, 0.2, 1.0))

plane_m <- generate_plane(c(0, 0, 0), c(0, 1, 0), 1.2, 0.8,
                          c(0.5, 0.5, 0.5, 1.0))

rows <- list(
    render_pair(cuboid,  c(1.2, 0.8, 1),    "cuboid"),
    render_pair(sphere,  c(1.2, 0.8, 1),    "sphere"),
    render_pair(cyl,     c(1.2, 0.8, 0.5),  "cylinder"),
    render_pair(cone,    c(1.2, 0.5, 0.8),  "cone"),
    render_pair(pyramid, c(1.2, 0.8, 1),    "pyramid"),
    render_pair(tetra,   c(1.2, 0.8, 0.6),  "tetrahedron"),
    render_pair(torus,   c(0, 0.5, 1.2),    "torus"),
    render_pair(plane_m, c(0.3, 0.8, 1),    "plane")
)

cat("Composing...\n")
gallery <- do.call(stack_vertical, rows)
out_file <- "scimesh_primitives.png"
write_png(gallery, out_file)
cat(sprintf("  -> %s (%dx%d)\n", out_file, gallery$width, gallery$height))
cat("Done.\n")
