#!/usr/bin/env Rscript
#
# scimesh Example — Stanford Dragon (PLY)
# ---------------------------------------
# Renders the Stanford Dragon with smooth shading, three-point
# Blinn-Phong lighting, and 4x anti-aliasing, producing an image
# identical in appearance to the C++ dragon demo.
#
# Usage:
#   Rscript examples/R/dragon/run.R
#
# Output PNG file is written to the current directory:
#   scimesh_dragon.png
#
# Dependencies: scimesh

library(scimesh)

test_data_dir <- Sys.getenv("SCIMESH_TEST_DATA_DIR",
    unset = file.path(
        dirname(sub("^--file=", "", grep("^--file=", commandArgs(FALSE), value = TRUE)[1])),
        "..", "..", "test_data"
    )
)

ply_path <- file.path(test_data_dir, "stanford_3d_scanning_repo/dragon/dragon_vrip.ply")

cat("Loading", ply_path, "...\n")
mesh <- read_ply(ply_path)
nv <- nrow(mesh$vertices)
nt <- nrow(mesh$triangles)
cat(sprintf("Mesh: %d vertices, %d triangles\n", nv, nt))

mesh$colors <- matrix(c(0.78, 0.75, 0.68, 1.0),
                      nrow = nv, ncol = 4, byrow = TRUE)

cat("Setting up camera and lights...\n")
cam <- camera_auto(mesh,
    direction = c(0.9, 0.5, 1.0),
    up = c(0.0, 1.0, 0.0),
    fov = 40, margin = 1.01)

opts <- render_options(
    width  = 1980L,
    height = 1020L,
    shading = "smooth",
    backface_culling = TRUE,
    background_color = c(0.92, 0.93, 0.95, 1),
    ambient = 0.22,
    specular_color = c(0.25, 0.25, 0.25, 1),
    shininess = 40,
    aa_samples = 4L,
    threads = 0L,
    ssao_enabled = FALSE,
    lights = list(
        list(position = c(1.2, 1.5, 1.0),
             color = c(1.00, 0.95, 0.88, 1),
             intensity = 1.8),
        list(position = c(-1.5, 0.4, 0.5),
             color = c(0.42, 0.50, 0.78, 1),
             intensity = 0.55),
        list(position = c(-0.3, -0.5, -1.3),
             color = c(0.55, 0.55, 0.55, 1),
             intensity = 0.5)
    )
)

cat(sprintf("Rendering at %dx%d with %dx AA ...\n",
    opts$width, opts$height, opts$aa_samples))

t_start <- proc.time()
img <- render_mesh(
    vertices  = mesh$vertices,
    triangles = mesh$triangles,
    colors    = mesh$colors,
    camera    = cam,
    options   = opts
)
t_sec <- (proc.time() - t_start)[["elapsed"]]

out_file <- "scimesh_dragon.png"
write_png(img, out_file)
cat(sprintf("  -> %s (%dx%d)\n", out_file, img$width, img$height))
cat(sprintf("Render time: %.1f s\n", t_sec))
cat("Done.\n")
