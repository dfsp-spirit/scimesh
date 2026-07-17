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
cube <- as.tmesh3d(cube3d())

# Determine output directory: same directory as this script
script_path <- grep("--file=", commandArgs(trailingOnly = FALSE), value = TRUE)
if (length(script_path) == 0L) {
    script_dir <- "."
} else {
    script_path <- sub("^--file=", "", script_path)
    script_dir <- dirname(normalizePath(script_path))
}

# ---- Render in scimesh (default: straight front, FOV=45°) ------------------
cat("Rendering cube with scimesh (default) ...\n")
scimesh_img <- render_mesh(cube)
out_default <- file.path(script_dir, "cube_scimesh_default.png")
write_png(scimesh_img, out_default)
cat(sprintf("  -> %s\n", out_default))

# ---- Render in scimesh (rgl_compat: elevated, FOV=30°, sphere distance) ----
cat("Rendering cube with scimesh (rgl_compat) ...\n")
cam_rgl <- camera_auto(cube, rgl_compat = TRUE)
scimesh_rgl_img <- render_mesh(cube, camera = cam_rgl)
out_rglcompat <- file.path(script_dir, "cube_scimesh_rglcompat.png")
write_png(scimesh_rgl_img, out_rglcompat)
cat(sprintf("  -> %s\n", out_rglcompat))

# ---- Render in rgl (default view, no camera modification) ------------------
cat("Rendering cube with rgl (default view) ...\n")
options(rgl.useNULL = FALSE)
open3d()
shade3d(cube, col = "grey70")
out_rgl <- file.path(script_dir, "cube_rgl.png")
snapshot3d(out_rgl)
close3d()
cat(sprintf("  -> %s\n", out_rgl))

# ---- Visual comparison -----------------------------------------------------
cat("\nAll three images have been written.  Compare them side-by-side:\n")
cat(sprintf("  scimesh default:    %s\n", out_default))
cat(sprintf("  scimesh rgl_compat: %s\n", out_rglcompat))
cat(sprintf("  rgl default:        %s\n", out_rgl))
cat("\nDone.\n")