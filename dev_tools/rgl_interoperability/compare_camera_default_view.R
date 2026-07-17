#!/usr/bin/env Rscript
#
# Render simple mesh in both rgl and scimesh, and compare the camera default view.
#
# This requires rgl to be installed.
#

library(scimesh)
library(rgl)

# ---- Create the same cube in both libraries --------------------------------

# rgl: axis-aligned cube centered at origin, side length 2
rgl_cube <- as.tmesh3d(cube3d())

# scimesh: same dimensions
scimesh_cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))

# ---- Define a common camera for a fair comparison --------------------------
# Use a perspective camera looking along the negative-Z axis, slightly elevated
# so we can see three faces of the cube.
eye    <- c(1.5, 1.2, 2.5)
center <- c(0, 0, 0)
up     <- c(0, 1, 0)
fov    <- 45

# ---- Render in scimesh -----------------------------------------------------
cat("Rendering cube with scimesh ...\n")
scimesh_cam <- camera(eye = eye, center = center, up = up,
                      projection = "perspective", fov = fov)
scimesh_img <- render_mesh(scimesh_cube, camera = scimesh_cam)

# Determine output directory: same directory as this script
script_path <- grep("--file=", commandArgs(trailingOnly = FALSE), value = TRUE)
if (length(script_path) == 0L) {
    script_dir <- "."
} else {
    script_path <- sub("^--file=", "", script_path)
    script_dir <- dirname(normalizePath(script_path))
}
out_scimesh <- file.path(script_dir, "cube_scimesh.png")
write_png(scimesh_img, out_scimesh)
cat(sprintf("  -> %s\n", out_scimesh))

# ---- Render in rgl ---------------------------------------------------------
cat("Rendering cube with rgl ...\n")
options(rgl.useNULL = TRUE)
open3d()
shade3d(rgl_cube, col = "grey70")

# Build a lookAt matrix matching the scimesh camera and apply it to rgl.
# rgl uses column-major 4x4 userMatrix (modelview).
lookat <- function(eye, center, up) {
    f <- center - eye
    f <- f / sqrt(sum(f^2))
    u <- up / sqrt(sum(up^2))
    s <- cross_prod_3d(f, u)
    s <- s / sqrt(sum(s^2))
    u <- cross_prod_3d(s, f)
    m <- diag(4)
    m[1:3, 1] <- s
    m[1:3, 2] <- u
    m[1:3, 3] <- -f
    m[1:3, 4] <- eye
    m
}
cross_prod_3d <- function(a, b) {
    c(a[2]*b[3] - a[3]*b[2],
      a[3]*b[1] - a[1]*b[3],
      a[1]*b[2] - a[2]*b[1])
}
rgl_user <- lookat(eye, center, up)
par3d(userMatrix = rgl_user, FOV = fov)

out_rgl <- file.path(script_dir, "cube_rgl.png")
snapshot3d(out_rgl)
close3d()
cat(sprintf("  -> %s\n", out_rgl))

# ---- Visual comparison -----------------------------------------------------
cat("\nBoth images have been written.  Compare them side-by-side:\n")
cat(sprintf("  scimesh:  %s\n", out_scimesh))
cat(sprintf("  rgl:      %s\n", out_rgl))
cat("\nDone.\n")