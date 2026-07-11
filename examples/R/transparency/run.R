#!/usr/bin/env Rscript
#
# scimesh Example — Semi-transparent surface rendering
# ------------------------------------------------------
# Renders the left hemisphere white matter surface (opaque gray) with
# the pial surface overlaid in semi-transparent red, demonstrating
# alpha blending and depth-sorted transparency.
#
# Usage:
#   Rscript examples/R/transparency/run.R
#
# Output PNG file is written to the current directory:
#   scimesh_transparency_lh.png

library(scimesh)

sjd <- "test_data/freesurfer/subjects_dir"
sj  <- "subject1"

# ---- Load surfaces ----
cat("Loading white matter surface...\n")
white <- freesurferformats::read.fs.surface(
    file.path(sjd, sj, "surf", "lh.white"))

cat("Loading pial surface...\n")
pial <- freesurferformats::read.fs.surface(
    file.path(sjd, sj, "surf", "lh.pial"))

nv <- nrow(white$vertices)
cat(sprintf("  White: %d vertices, %d faces\n", nv, nrow(white$faces)))
cat(sprintf("  Pial:  %d vertices, %d faces\n", nrow(pial$vertices), nrow(pial$faces)))

# ---- Build coloured meshes ----
# White matter: opaque light gray
white_colors <- matrix(c(0.7, 0.7, 0.7, 1.0), nrow = nv, ncol = 4, byrow = TRUE)

# Pial: semi-transparent red (35% opaque)
pial_colors <- matrix(c(0.9, 0.3, 0.2, 0.35), nrow = nrow(pial$vertices),
                      ncol = 4, byrow = TRUE)

white_mesh <- list(
    vertices  = white$vertices,
    triangles = white$faces,
    colors    = white_colors
)

pial_mesh <- list(
    vertices  = pial$vertices,
    triangles = pial$faces,
    colors    = pial_colors
)

# ---- Camera ----
cam <- camera_auto(pial_mesh,
    direction = c(-1, 0, 0.2),  # lateral view, slightly tilted
    up = c(0, 0, 1))

# ---- Render ----
cat("Rendering...\n")
img <- render_scene(list(white_mesh, pial_mesh), cam,
    render_options(
        width = 1200L, height = 900L,
        backface_culling = FALSE,
        background_color = c(1, 1, 1, 1)
    ))

write_png(img, "scimesh_transparency_lh.png")
cat(sprintf("  -> scimesh_transparency_lh.png  (%dx%d)\n",
    img$width, img$height))
cat("Done.\n")
