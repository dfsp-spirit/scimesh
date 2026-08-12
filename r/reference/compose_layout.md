# Compose multiple images into a single figure

Arranges rendered images (from
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md))
into a grid layout and optionally appends a colorbar. All composition is
done with pure R array operations.

## Usage

``` r
compose_layout(
  images,
  nrow = NULL,
  ncol = NULL,
  colorbar = NULL,
  colorbar_height = 80L,
  colorbar_width = 80L,
  background = c(0, 0, 0, 0),
  colorbar_side = c("right", "left"),
  crop = FALSE
)
```

## Arguments

- images:

  A list of images, each a list with `width`, `height`, `pixels` as
  returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md).

- nrow:

  Number of rows in the grid layout.

- ncol:

  Number of columns in the grid layout. If both `nrow` and `ncol` are
  `NULL`, a square-ish layout is chosen automatically.

- colorbar:

  Optional colorbar array (from
  [`colorbar_horizontal()`](https://dfsp-spirit.github.io/scimesh/r/reference/colorbar_horizontal.md)
  or
  [`colorbar_vertical()`](https://dfsp-spirit.github.io/scimesh/r/reference/colorbar_vertical.md)).
  Placed below if horizontal, to the right if vertical.

- colorbar_height:

  Height of the colorbar row in pixels. Only used when appending a
  horizontal colorbar.

- colorbar_width:

  Width of the colorbar column in pixels. Only used when appending a
  vertical colorbar.

- background:

  Background RGBA color for padding (0-1 scale).

- colorbar_side:

  For vertical colorbars, whether to place the bar on the `"right"`
  (default) or `"left"` of the brain images. Ignored for horizontal
  colorbars.

- crop:

  Logical. If `TRUE`, transparent borders are cropped individually and
  images are padded to per-row height and per-column width for a tight
  layout with minimal white space. Default is `FALSE` (images must be
  same size).

## Value

A list with `width`, `height`, `pixels` suitable for
[`write_png()`](https://dfsp-spirit.github.io/scimesh/r/reference/write_png.md)
or
[`image_to_array()`](https://dfsp-spirit.github.io/scimesh/r/reference/image_to_array.md).

## Examples

``` r
mesh1 <- generate_cuboid(c(-1.5, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
mesh2 <- generate_cuboid(c( 1.5, 0, 0), c(0.5, 0.5, 0.5), c(0, 0, 1, 1))
img1 <- render_mesh(mesh1$vertices, mesh1$triangles)
img2 <- render_mesh(mesh2$vertices, mesh2$triangles)
result <- compose_layout(list(img1, img2), nrow = 1L)
tmp_file <- tempfile(fileext = ".png")
write_png(result, tmp_file)
```
