#!/usr/bin/env Rscript
#
# scimesh Example — Colormaps
# ---------------------------
# Demonstrates mapping data onto a 3D mesh using different colormaps
# with accompanying colorbars. Compatible with viridisLite but works
# without it (using base R built-in colormaps).
#
# Usage:
#   Rscript examples/R/colormaps/run.R
#
# Output PNG files are written to the current directory:
#   colormaps_viridis.png
#   colormaps_diverging.png
#   colormaps_custom.png

library(scimesh)

output_file_viridis    <- "colormaps_viridis.png"
output_file_diverging  <- "colormaps_diverging.png"
output_file_custom     <- "colormaps_custom.png"

cat("Rendering torus mesh with different colormaps...\n")

n_faces   <- 16 * 32 * 2  # torus: 2 triangles per quad face
set.seed(42L)
torus_data <- runif(n_faces, -3, 3)

torus <- generate_torus(c(0, 0, 0), 1.5, 0.4, 32, 16)

map_data_to_colors <- function(data, cmap_func, n = 256L) {
    palette <- cmap_func(n)
    norm_data <- (data - min(data)) / (max(data) - min(data) + 1e-12)
    idx <- as.integer(norm_data * (n - 1L)) + 1L
    hex_cols <- palette[idx]
    rgba <- t(grDevices::col2rgb(hex_cols, alpha = FALSE)) / 255
    cbind(rgba, 1.0)
}

cam <- camera_auto(torus, direction = c(1.2, 0.8, 1), fov = 35)
opts <- render_options(
    width = 800L, height = 600L, shading = "smooth",
    background_color = c(0.92, 0.93, 0.95, 1),
    ambient = 0.15,
    specular_color = c(0.3, 0.3, 0.3, 1),
    shininess = 32,
    lights = list(
        list(position = c(0.5, 1.0, 0.8),  color = c(1.0, 0.95, 0.9, 1),
             intensity = 1.5),
        list(position = c(-0.5, 0.2, 0.6), color = c(0.4, 0.5, 0.8, 1),
             intensity = 0.5)
    )
)

# --- 1: Built-in viridis colormap ---
cat("  viridis...\n")
viridis_cols <- map_data_to_colors(torus_data, viridis_colormap)
torus$face_colors <- viridis_cols
img_viridis <- render_mesh(torus$vertices, torus$triangles,
    face_colors = torus$face_colors, camera = cam, options = opts)
cbar_viridis <- colorbar_vertical(viridis_colormap,
    width = 60, height = opts$height, data_range = range(torus_data),
    ticks = pretty(range(torus_data), n = 5), title = "Viridis")
out_viridis <- stack_horizontal(img_viridis, cbar_viridis)
write_png(out_viridis, output_file_viridis)
cat(sprintf("  -> %s (%dx%d)\n", output_file_viridis,
    out_viridis$width, out_viridis$height))

# --- 2: Built-in diverging colormap ---
cat("  diverging...\n")
diverging_cols <- map_data_to_colors(torus_data, diverging_colormap)
torus$face_colors <- diverging_cols
img_diverging <- render_mesh(torus$vertices, torus$triangles,
    face_colors = torus$face_colors, camera = cam, options = opts)
cbar_diverging <- colorbar_vertical(diverging_colormap,
    width = 60, height = opts$height, data_range = range(torus_data),
    ticks = pretty(range(torus_data), n = 5), title = "Diverging")
out_diverging <- stack_horizontal(img_diverging, cbar_diverging)
write_png(out_diverging, output_file_diverging)
cat(sprintf("  -> %s (%dx%d)\n", output_file_diverging,
    out_diverging$width, out_diverging$height))

# --- 3: Custom base R colormap (inferno via hcl.colors) ---
cat("  custom (inferno via base R)...\n")
my_inferno <- function(n) grDevices::hcl.colors(n, palette = "inferno")
custom_cols <- map_data_to_colors(torus_data, my_inferno)
torus$face_colors <- custom_cols
img_custom <- render_mesh(torus$vertices, torus$triangles,
    face_colors = torus$face_colors, camera = cam, options = opts)
cbar_custom <- colorbar_vertical(my_inferno,
    width = 60, height = opts$height, data_range = range(torus_data),
    ticks = pretty(range(torus_data), n = 5),
    title = expression(Inferno ~ "(base R)"))
out_custom <- stack_horizontal(img_custom, cbar_custom)
write_png(out_custom, output_file_custom)
cat(sprintf("  -> %s (%dx%d)\n", output_file_custom,
    out_custom$width, out_custom$height))

# --- 4: viridisLite (if available, overwrites the viridis output) ---
if (requireNamespace("viridisLite", quietly = TRUE)) {
    cat("  viridisLite detected — trying full family...\n")

    viridis_viridis <- function(n) viridisLite::viridis(n, option = "D")
    vr_cols <- map_data_to_colors(torus_data, viridis_viridis)
    torus$face_colors <- vr_cols
    img_vr <- render_mesh(torus$vertices, torus$triangles,
        face_colors = torus$face_colors, camera = cam, options = opts)
    cbar_vr <- colorbar_vertical(viridis_viridis,
        width = 60, height = opts$height, data_range = range(torus_data),
        ticks = pretty(range(torus_data), n = 5),
        title = "viridis\n(viridisLite)")
    out_vr <- stack_horizontal(img_vr, cbar_vr)
    write_png(out_vr, output_file_viridis)
    cat(sprintf("  -> %s (%dx%d) [viridisLite version]\n",
        output_file_viridis, out_vr$width, out_vr$height))

    magma_cmap <- function(n) viridisLite::viridis(n, option = "A")
    magma_cols <- map_data_to_colors(torus_data, magma_cmap)
    torus$face_colors <- magma_cols
    img_magma <- render_mesh(torus$vertices, torus$triangles,
        face_colors = torus$face_colors, camera = cam, options = opts)
    cbar_magma <- colorbar_vertical(magma_cmap,
        width = 60, height = opts$height, data_range = range(torus_data),
        ticks = pretty(range(torus_data), n = 5), title = "Magma")
    out_magma <- stack_horizontal(img_magma, cbar_magma)
    write_png(out_magma, "colormaps_magma.png")
    cat(sprintf("  -> colormaps_magma.png (%dx%d)\n",
        out_magma$width, out_magma$height))

    inferno_cmap <- function(n) viridisLite::viridis(n, option = "B")
    inferno_cols <- map_data_to_colors(torus_data, inferno_cmap)
    torus$face_colors <- inferno_cols
    img_inferno <- render_mesh(torus$vertices, torus$triangles,
        face_colors = torus$face_colors, camera = cam, options = opts)
    cbar_inferno <- colorbar_vertical(inferno_cmap,
        width = 60, height = opts$height, data_range = range(torus_data),
        ticks = pretty(range(torus_data), n = 5), title = "Inferno")
    out_inferno <- stack_horizontal(img_inferno, cbar_inferno)
    write_png(out_inferno, "colormaps_inferno.png")
    cat("  -> colormaps_inferno.png\n")
}

cat("Done.\n")
