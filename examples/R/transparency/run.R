#!/usr/bin/env Rscript
#
# scimesh Example — Transparency (per-vertex and per-mesh alpha)
# -------------------------------------------------------------
# Renders three scenes, one PNG each:
#
#   1. transparency_spheres.png          a translucent sphere in front of opaque
#                                        ones, another one behind them, and a
#                                        small opaque sphere *inside* the
#                                        translucent shell
#   2. transparency_alpha_gradient.png   a row of spheres with alpha 1.0 -> 0.0
#   3. transparency_brain_medialwall.png a FreeSurfer hemisphere whose medial
#                                        wall is 50 % transparent, seen through
#                                        an 80 % transparent sphere, with two
#                                        opaque "voxels" inside the brain
#
# Scene 3 is the spatial-reference use case: translucent anatomy with opaque
# data inside it.  Nothing needs to be switched on — the renderer blends any
# mesh whose colors have alpha < 1, sorts the translucent triangles
# back-to-front and depth-tests them against the opaque geometry, so opaque
# objects behind them stay crisp.
#
# Usage:
#   Rscript examples/R/transparency/run.R
#
# Output PNG files are written to the current directory.

library(scimesh)

test_data_dir <- Sys.getenv("SCIMESH_TEST_DATA_DIR",
    unset = file.path(
        # examples/R/transparency/run.R -> <repo root>/test_data
        dirname(sub("^--file=", "", grep("^--file=", commandArgs(FALSE), value = TRUE)[1])),
        "..", "..", "..", "test_data"
    )
)

render_and_save <- function(meshes, direction, path, width = 1000, height = 700) {
    cam <- camera_auto(meshes, direction = direction, up = c(0, 0, 1))
    cat(sprintf("Rendering %s (%dx%d)...\n", path, width, height))
    img <- render_scene(meshes, cam,
        render_options(
            width = width, height = height,
            backface_culling = FALSE,
            background_color = c(1, 1, 1, 1),
            specular_color = c(0.35, 0.35, 0.35, 1),
            shininess = 48
        ))
    write_png(img, path)
    cat(sprintf("  Wrote %s\n", path))
}

# ---------------------------------------------------------------- Scene 1 ---
cat("\n=== Scene 1: translucent spheres ===\n")

# Opaque reference sphere in the middle.
opaque <- generate_sphere(c(0, 0, 0), radius = 1, color = c(0.85, 0.2, 0.2, 1))
# 80 % transparent shell overlapping it from the front, with a small opaque
# sphere inside: the classic "is my data where I think it is?" view.
shell <- generate_sphere(c(-0.6, 0, 1.4), radius = 1, color = c(0.2, 0.4, 0.95, 0.2))
inside <- generate_sphere(c(-0.6, 0, 1.3), radius = 0.28, color = c(1, 0.85, 0.1, 1))
# 60 % transparent sphere overlapping the opaque one from *behind*: it must not
# wash the opaque sphere out.
behind <- generate_sphere(c(0.6, 0, -1.4), radius = 1, color = c(0.2, 0.8, 0.3, 0.4))

render_and_save(list(opaque, shell, inside, behind),
                direction = c(-0.35, 0.6, 0.75),
                path = "transparency_spheres.png")

# ---------------------------------------------------------------- Scene 2 ---
cat("\n=== Scene 2: alpha gradient ===\n")

# Opaque backdrop so the show-through is visible.
backdrop <- generate_cuboid(c(0, 0, -2), half_extents = c(5, 1.6, 0.15),
                            color = c(0.75, 0.75, 0.78, 1))
alphas <- c(1, 0.8, 0.6, 0.4, 0.2, 0.05)
spheres <- lapply(seq_along(alphas), function(i) {
    x <- (i - 1 - (length(alphas) - 1) / 2) * 1.05
    generate_sphere(c(x, 0, 0), radius = 0.62, color = c(0.8, 0.25, 0.6, alphas[i]))
})

render_and_save(c(list(backdrop), spheres),
                direction = c(0, 0.55, 0.85),
                path = "transparency_alpha_gradient.png", width = 1100, height = 500)

# ---------------------------------------------------------------- Scene 3 ---
cat("\n=== Scene 3: brain hemisphere, medial wall 50 % transparent ===\n")

sjd <- file.path(test_data_dir, "freesurfer/subjects_dir")
sj <- "subject1"
surface_file <- file.path(sjd, sj, "surf", "lh.white")
sulc_file    <- file.path(sjd, sj, "surf", "lh.sulc")
label_file   <- file.path(sjd, sj, "label", "lh.cortex.label")

if (!all(file.exists(surface_file, sulc_file, label_file))) {
    cat("  SKIPPED: FreeSurfer test data not found (", sj, ")\n", sep = "")
} else {
    surface <- freesurferformats::read.fs.surface(surface_file)
    sulc <- freesurferformats::read.fs.morph(sulc_file)
    cortex <- freesurferformats::read.fs.label(label_file)

    nv <- nrow(surface$vertices)
    cat(sprintf("  %d vertices, %d faces, %d cortex label entries\n",
                nv, nrow(surface$faces), length(cortex)))

    # Medial wall = everything outside the cortex label.
    medial_wall <- rep(TRUE, nv)
    medial_wall[cortex] <- FALSE
    sulc[medial_wall] <- NA
    cat(sprintf("  %d vertices on the medial wall\n", sum(medial_wall)))

    # Viridis coloring of the cortex, white medial wall.
    colors <- apply_colormap(sulc,
        colormap = viridis_colormap(256L),
        nan_color = c(1, 1, 1, 1),
        winsor_percentiles = c(2, 98))
    if (ncol(colors) == 3) colors <- cbind(colors, 1)
    # The medial wall is drawn half transparent instead of opaque white, so the
    # inside of the brain (and the voxels in it) shows through it.  The renderer
    # picks the translucent pass up from the colors - no flag to set.
    colors[medial_wall, 4] <- 0.5

    brain <- list(vertices = surface$vertices, triangles = surface$faces,
                  colors = colors)

    # Two opaque "activation blobs" inside the hemisphere, plus an 80 %
    # transparent sphere between the camera and the hemisphere.
    bbox <- apply(surface$vertices, 2, range)
    center <- colMeans(bbox)
    extent <- sqrt(sum((bbox[2, ] - bbox[1, ])^2))

    blobs <- list(
        generate_sphere(center + c(0, -0.035, 0.02) * extent,
                        radius = 0.05 * extent, color = c(1, 0.45, 0.05, 1)),
        generate_sphere(center + c(-0.02, 0.045, -0.03) * extent,
                        radius = 0.04 * extent, color = c(0.95, 0.9, 0.1, 1)),
        # Seen through this one:
        generate_sphere(center + c(0.30, 0, 0) * extent,
                        radius = 0.22 * extent, color = c(0.25, 0.5, 0.95, 0.2))
    )

    # Medial view: the medial wall faces the midline (+X for the left
    # hemisphere), so that is the direction the camera goes.
    render_and_save(c(list(brain), blobs),
                    direction = c(1, 0.12, 0.12),
                    path = "transparency_brain_medialwall.png", height = 800)
}

cat("\nDone.\n")
