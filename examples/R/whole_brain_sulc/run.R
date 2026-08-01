#!/usr/bin/env Rscript
#
# scimesh Example — Whole-brain sulcal depth rendering
# ----------------------------------------------------
# Loads both brain hemisphere white-matter surfaces from FreeSurfer
# files, maps sulcal depth to vertex colours via apply_colormap(),
# masks out non-cortex (medial wall) vertices, and renders the
# whole-brain scene to a PNG image.
#
# This demonstrates the high-level colormap API: apply_colormap()
# handles multi-dataset mapping, pooled range, winsorizing, and NaN
# colours in a single call.
#
# Usage:
#   Rscript examples/R/whole_brain_sulc/run.R
#
# Output PNG file is written to the current directory:
#   scimesh_whole_brain_sulc.png
#
# Dependencies: scimesh, freesurferformats

library(scimesh)

test_data_dir <- Sys.getenv("SCIMESH_TEST_DATA_DIR",
    unset = file.path(
        dirname(sub("^--file=", "", grep("^--file=", commandArgs(FALSE), value = TRUE)[1])),
        "..", "..", "test_data"
    )
)

sjd <- file.path(test_data_dir, "freesurfer/subjects_dir")
sj  <- "subject1"

cat(sprintf("Subjects dir : %s\n", sjd))
cat(sprintf("Subject      : %s\n\n", sj))

# ---- helper: load mesh geometry and sulc data for one hemisphere ----
build_hemi_data <- function(sjd, sj, hemi) {
    surf_file  <- file.path(sjd, sj, "surf",  paste0(hemi, ".white"))
    sulc_file  <- file.path(sjd, sj, "surf",  paste0(hemi, ".sulc"))
    label_file <- file.path(sjd, sj, "label", paste0(hemi, ".cortex.label"))

    for (f in c(surf_file, sulc_file, label_file)) {
        if (!file.exists(f)) stop("File not found: ", f)
    }

    cat(sprintf("=== Loading %s hemisphere ===\n", hemi))

    cat(sprintf("  Surface: %s\n", surf_file))
    surf <- freesurferformats::read.fs.surface(surf_file)
    nv <- nrow(surf$vertices)
    nf <- nrow(surf$faces)
    cat(sprintf("  Loaded %d vertices, %d faces.\n", nv, nf))

    cat(sprintf("  Sulc data: %s\n", sulc_file))
    sulc <- freesurferformats::read.fs.morph(sulc_file)
    if (length(sulc) != nv) {
        stop(sprintf("  ERROR: sulc count (%d) != vertex count (%d)",
                     length(sulc), nv))
    }

    cat(sprintf("  Label: %s\n", label_file))
    cortex_verts <- freesurferformats::read.fs.label(label_file)
    in_cortex <- rep(FALSE, nv)
    in_cortex[cortex_verts] <- TRUE
    n_cortex <- sum(in_cortex)
    n_medial <- nv - n_cortex
    cat(sprintf("  Cortex label: %d in cortex, %d medial wall.\n",
                n_cortex, n_medial))

    # Mask medial wall → NA
    sulc[!in_cortex] <- NA_real_

    list(
        vertices  = surf$vertices,
        triangles = surf$faces,
        sulc      = sulc
    )
}

# ---- Phase 1: Load both hemispheres (geometry + data) ----
lh <- build_hemi_data(sjd, sj, "lh")
cat("  Done with lh.\n\n")

rh <- build_hemi_data(sjd, sj, "rh")
cat("  Done with rh.\n\n")

# ---- Phase 2: Apply colormap with pooled range + winsorizing ----
cat("=== Applying colormap ===\n")

colors <- apply_colormap(
    list(lh$sulc, rh$sulc),
    colormap = viridis_colormap(256L),
    limits = "global",
    nan_color = c(1, 1, 1, 1),        # white for medial wall
    winsor_percentiles = c(0.02, 0.98) # clip outliers
)

cat(sprintf("  Pooled data range: %.3f to %.3f\n",
            attr(colors, "pooled_data_min"),
            attr(colors, "pooled_data_max")))

# Assign colours to mesh descriptors
lh_mesh <- list(vertices = lh$vertices, triangles = lh$triangles,
                colors = colors[[1]])
rh_mesh <- list(vertices = rh$vertices, triangles = rh$triangles,
                colors = colors[[2]])

# ---- Phase 3: Render ----
cat("=== Computing camera ===\n")
cam <- camera_auto(list(lh_mesh, rh_mesh),
    direction = c(-1.0, 0.3, 0.4),
    up = c(0.0, 0.0, 1.0),
    fov = 45, margin = 1.1)
cat(sprintf("  eye    = (%.2f, %.2f, %.2f)\n",
            cam$eye[1], cam$eye[2], cam$eye[3]))
cat(sprintf("  center = (%.2f, %.2f, %.2f)\n",
            cam$center[1], cam$center[2], cam$center[3]))

cat("  Rendering at 1200x900...\n")

opts <- render_options(
    width = 1200L, height = 900L,
    shading = "smooth",
    backface_culling = FALSE,
    background_color = c(1, 1, 1, 1)
)

t_start <- proc.time()
img <- render_scene(list(lh_mesh, rh_mesh), cam, opts)
t_sec <- (proc.time() - t_start)[["elapsed"]]

out_file <- "scimesh_whole_brain_sulc.png"
write_png(img, out_file)
cat(sprintf("  -> %s (%dx%d)\n", out_file, img$width, img$height))
cat(sprintf("Render time: %.1f s\n", t_sec))
cat("Done.\n")
