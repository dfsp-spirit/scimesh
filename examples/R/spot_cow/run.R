#!/usr/bin/env Rscript
#
# scimesh Example — Textured Spot Cow (OBJ)
# ------------------------------------------
# Renders the Keenan Crane "Spot" cow mesh with its texture, using
# read_obj() to load the triangulated OBJ and the png package to
# load the texture image.
#
# Usage:
#   Rscript examples/R/spot_cow/run.R
#
# Output PNG file is written to the current directory:
#   scimesh_spot_cow.png
#
# Dependencies: scimesh, png

library(scimesh)

obj_path <- "test_data/keenan_crane/spot/spot_triangulated.obj"
tex_path <- "test_data/keenan_crane/spot/spot_texture.png"

cat("Loading mesh...\n")
mesh <- read_obj(obj_path)
cat(sprintf("  Vertices: %d, Triangles: %d, UVs: %d\n",
    nrow(mesh$vertices), nrow(mesh$triangles),
    if (!is.null(mesh$uv)) nrow(mesh$uv) else 0L))

cat("Loading texture...\n")
if (!requireNamespace("png", quietly = TRUE)) {
    stop("Package 'png' is required to read texture images.")
}
tex_arr <- png::readPNG(tex_path)  # HxWxC, values 0-1
tw <- ncol(tex_arr)
th <- nrow(tex_arr)
tc <- dim(tex_arr)[3L]
cat(sprintf("  Texture: %dx%d, %d channels\n", tw, th, tc))

# Ensure 4-channel RGBA
if (tc == 3L) {
    rgba <- array(1.0, dim = c(th, tw, 4L))
    rgba[, , 1:3] <- tex_arr
    tex_arr <- rgba
} else if (tc == 1L) {
    rgba <- array(1.0, dim = c(th, tw, 4L))
    rgba[, , 1L] <- tex_arr
    rgba[, , 2L] <- tex_arr
    rgba[, , 3L] <- tex_arr
    tex_arr <- rgba
}

# Convert to scimesh raw pixel format (RGBA interleaved, row-major)
tex_arr <- round(tex_arr * 255)
tex_arr <- aperm(tex_arr, c(3L, 2L, 1L))  # 4xWxH
texture <- list(width = tw, height = th, pixels = as.raw(tex_arr))

cat("Setting up camera and lights...\n")
cam <- camera_auto(mesh,
    direction = c(1.0, 0.4, -1.2),
    up = c(0.0, 1.0, 0.0),
    fov = 40, margin = 1.05)

opts <- render_options(
    width = 1200L, height = 900L,
    shading = "smooth",
    backface_culling = TRUE,
    background_color = c(0.10, 0.10, 0.14, 1),
    ambient = 0.3,
    specular_color = c(0.2, 0.2, 0.2, 1),
    shininess = 16,
    aa_samples = 2L,
    ssao_enabled = TRUE,
    ssao_radius = 12,
    ssao_intensity = 0.5,
    lights = list(
        list(position = c(0.5, 1.0, 1.0),
             color = c(1.00, 0.97, 0.90, 1),
             intensity = 1.5),
        list(position = c(-1.0, 0.2, 0.5),
             color = c(0.4, 0.5, 0.8, 1),
             intensity = 0.5),
        list(position = c(0.0, -0.3, -1.0),
             color = c(0.5, 0.5, 0.5, 1),
             intensity = 0.4)
    )
)

cat("Rendering...\n")
img <- render_mesh(
    vertices = mesh$vertices,
    triangles = mesh$triangles,
    uv = mesh$uv,
    texture = texture,
    camera = cam,
    options = opts
)

out_file <- "scimesh_spot_cow.png"
write_png(img, out_file)
cat(sprintf("  -> %s (%dx%d)\n", out_file, img$width, img$height))
cat("Done.\n")
