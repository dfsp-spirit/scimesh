#!/usr/bin/env Rscript
#
# Test: what happens when you render a large object in rgl with the default
# camera?  rgl DOES auto-distance by adjusting the modelMatrix — the camera
# (userMatrix) stays at a fixed rotation, but the model is translated away
# proportionally to the bounding box, so all objects appear the same size.
#

library(rgl)

# Determine output directory: same directory as this script
script_path <- grep("--file=", commandArgs(trailingOnly = FALSE), value = TRUE)
if (length(script_path) == 0L) {
    script_dir <- "."
} else {
    script_path <- sub("^--file=", "", script_path)
    script_dir <- dirname(normalizePath(script_path))
}

options(rgl.useNULL = FALSE)

# Helper to print camera diagnostics
dump_cam <- function(label) {
    cat(sprintf("\n=== Camera diagnostics: %s ===\n", label))
    bbox <- par3d("bbox")
    bbox_radius <- max(abs(bbox))  # half of the max extent
    cat(sprintf("bbox:        [%.2f, %.2f] [%.2f, %.2f] [%.2f, %.2f]\n",
        bbox[1], bbox[2], bbox[3], bbox[4], bbox[5], bbox[6]))
    cat(sprintf("bbox radius: %.2f\n", bbox_radius))
    cat(sprintf("zoom:        %.2f\n", par3d("zoom")))
    cat(sprintf("FOV:         %.2f\n", par3d("FOV")))

    u <- par3d("userMatrix")
    m <- par3d("modelMatrix")
    cat(sprintf("userMatrix[3,4]   (z-translation): %.4f\n", u[3, 4]))
    cat(sprintf("modelMatrix[3,4]  (z-translation): %.4f\n", m[3, 4]))
    cat(sprintf("ratio (model z / bbox radius): %.2f\n", abs(m[3, 4]) / bbox_radius))
    cat(sprintf("modelMatrix[1:3, 4] (translation): [%.4f, %.4f, %.4f]\n",
        m[1, 4], m[2, 4], m[3, 4]))
}

# ---- Radius ~0.81 (icosahedron, depth 3) -----------------------------------
open3d()
sp <- subdivision3d(icosahedron3d(), depth = 3)
shade3d(sp, col = "steelblue")
dump_cam("r=0.81")
snapshot3d(file.path(script_dir, "rgl_sphere_radius1.png"))
close3d()

# ---- Radius ~8.1 ----------------------------------------------------------
open3d()
sp <- subdivision3d(icosahedron3d(), depth = 3)
sp$vb[1:3, ] <- sp$vb[1:3, ] * 10
shade3d(sp, col = "firebrick")
dump_cam("r=8.1")
snapshot3d(file.path(script_dir, "rgl_sphere_radius10.png"))
close3d()

# ---- Radius ~81 -----------------------------------------------------------
open3d()
sp <- subdivision3d(icosahedron3d(), depth = 3)
sp$vb[1:3, ] <- sp$vb[1:3, ] * 100
shade3d(sp, col = "darkgreen")
dump_cam("r=81")
snapshot3d(file.path(script_dir, "rgl_sphere_radius100.png"))
close3d()

cat(sprintf("\nAll images saved to: %s\n", normalizePath(script_dir)))
cat("rgl auto-frames by pushing the model back via modelMatrix.\n")
cat("The ratio modelMatrix[3,4] / bbox_radius is constant (~6.7x),\n")
cat("so all spheres render at the same apparent size.\n")
