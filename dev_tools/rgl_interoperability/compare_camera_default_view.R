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

# ---- Render in scimesh (default camera via camera_auto) --------------------
cat("Rendering cube with scimesh (default camera) ...\n")
scimesh_img <- render_mesh(cube)
out_scimesh <- file.path(script_dir, "cube_scimesh.png")
write_png(scimesh_img, out_scimesh)
cat(sprintf("  -> %s\n", out_scimesh))

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
cat("\nBoth images have been written.  Compare them side-by-side:\n")
cat(sprintf("  scimesh:  %s\n", out_scimesh))
cat(sprintf("  rgl:      %s\n", out_rgl))
cat("\nDone.\n")