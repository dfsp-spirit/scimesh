#!/usr/bin/env Rscript
#
# scimesh Example — R high-level neuroimaging visualisation
# -----------------------------------------------------------
# Renders our FreeSurfer subject1 from three different view angle sets:
#   t4  – 2x2 tile (lateral & medial, both hemispheres)
#   t9  – 3x3 tile (all cardinal anatomical directions)
#   lateral_lh – single left-hemisphere lateral view with colorbar
#
# All output uses a transparent background, suitable for compositing
# into publication figures.
#
# Usage:
#   Rscript examples/R/vis_subject_morph_native/run.R
#
# Output PNG files are written to the current directory:
#   scimesh_subject1_sulc_t4.png
#   scimesh_subject1_sulc_t9.png
#   scimesh_subject1_sulc_lateral_lh.png

library(scimesh)

sjd <- "test_data/freesurfer/subjects_dir"
sj  <- "subject1"

# ---- t4: 2×2 (lateral & medial, both hemispheres) ---------------------------
cat("Rendering t4 ...\n")
img_t4 <- vis.subject.morph.native(
    sjd, sj, "sulc",
    views        = "t4",
    cortex_only  = TRUE,
    width        = 600L,
    height       = 450L,
    background   = c(0, 0, 0, 0)   # transparent RGBA
)
write_png(img_t4, "scimesh_subject1_sulc_t4.png")
cat("  -> scimesh_subject1_sulc_t4.png  (", img_t4$width, "x", img_t4$height, ")\n", sep = "")

# ---- t9: 3×3 (all cardinal anatomical directions) ---------------------------
cat("Rendering t9 ...\n")
img_t9 <- vis.subject.morph.native(
    sjd, sj, "sulc",
    views        = "t9",
    cortex_only  = TRUE,
    width        = 400L,
    height       = 300L,
    background   = c(0, 0, 0, 0)
)
write_png(img_t9, "scimesh_subject1_sulc_t9.png")
cat("  -> scimesh_subject1_sulc_t9.png  (", img_t9$width, "x", img_t9$height, ")\n", sep = "")

# ---- lateral_lh: single left-hemi lateral view + colorbar --------------------
cat("Rendering lateral_lh + colorbar ...\n")
img_lat <- vis.subject.morph.native(
    sjd, sj, "sulc",
    views           = "lateral_lh",
    cortex_only     = TRUE,
    draw_colorbar   = TRUE,
    width           = 800L,
    height          = 600L,
    background      = c(0, 0, 0, 0)
)
write_png(img_lat, "scimesh_subject1_sulc_lateral_lh.png")
cat("  -> scimesh_subject1_sulc_lateral_lh.png  (", img_lat$width, "x", img_lat$height, ")\n", sep = "")

cat("\nDone.\n")
