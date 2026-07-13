#!/usr/bin/env Rscript
#
# scimesh Example — Whole-brain sulcal depth rendering
# ----------------------------------------------------
# Loads both brain hemisphere white-matter surfaces from FreeSurfer
# files, maps sulcal depth to vertex colours via the viridis colormap,
# masks out non-cortex (medial wall) vertices, and renders the
# whole-brain scene to a PNG image.
#
# This demonstrates the full low-level pipeline: FreeSurfer mesh
# loading via freesurferformats, per-vertex data colouring with
# cortex masking, and headless rendering with scimesh.
#
# Usage:
#   Rscript examples/R/whole_brain_sulc_single_image/run.R
#
# Output PNG file is written to the current directory:
#   scimesh_whole_brain_sulc.png
#
# Dependencies: scimesh, freesurferformats

library(scimesh)

sjd <- "test_data/freesurfer/subjects_dir"
sj  <- "subject1"

cat(sprintf("Subjects dir : %s\n", sjd))
cat(sprintf("Subject      : %s\n\n", sj))

# ---- helper: build a mesh descriptor for one hemisphere ----
build_hemi_mesh <- function(sjd, sj, hemi) {
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
    finite_sulc <- sulc[!is.na(sulc)]
    cat(sprintf("  Sulc range: %.3f to %.3f\n",
                min(finite_sulc), max(finite_sulc)))

    cat(sprintf("  Label: %s\n", label_file))
    cortex_verts <- freesurferformats::read.fs.label(label_file)
    n_labeled <- length(cortex_verts)
    in_cortex <- rep(FALSE, nv)
    in_cortex[cortex_verts] <- TRUE
    n_cortex <- sum(in_cortex)
    n_medial <- nv - n_cortex
    cat(sprintf("  Cortex label: %d labeled vertices, %d in cortex, %d medial wall.\n",
                n_labeled, n_cortex, n_medial))

    sulc[!in_cortex] <- NA_real_

    cat("  Mapping sulc -> viridis colors...\n")

    finite_vals <- sulc[!is.na(sulc)]
    lo <- min(finite_vals)
    hi <- max(finite_vals)
    rng <- hi - lo
    if (rng == 0) rng <- 1

    palette <- viridis_colormap(256L)

    colors <- character(nv)
    for (i in seq_len(nv)) {
        if (is.na(sulc[i])) {
            colors[i] <- "#FFFFFFFF"
        } else {
            t <- (sulc[i] - lo) / rng
            t <- min(max(t, 0), 1)
            idx <- as.integer(t * 255L) + 1L
            if (idx < 1L) idx <- 1L
            if (idx > 256L) idx <- 256L
            colors[i] <- palette[idx]
        }
    }

    clean <- sub("^#", "", colors)
    r <- as.integer(paste0("0x", substr(clean, 1L, 2L))) / 255
    g <- as.integer(paste0("0x", substr(clean, 3L, 4L))) / 255
    b <- as.integer(paste0("0x", substr(clean, 5L, 6L))) / 255

    if (all(nchar(clean) >= 8L)) {
        a <- as.integer(paste0("0x", substr(clean, 7L, 8L))) / 255
    } else {
        a <- rep(1.0, nv)
    }

    list(
        vertices  = surf$vertices,
        triangles = surf$faces,
        colors    = cbind(r, g, b, a)
    )
}

lh_mesh <- build_hemi_mesh(sjd, sj, "lh")
cat("  Done with lh.\n\n")

rh_mesh <- build_hemi_mesh(sjd, sj, "rh")
cat("  Done with rh.\n\n")

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
