# Stack images vertically

Stacks a list of rendered images vertically (one below another). A
convenience wrapper around
[`compose_layout()`](https://dfsp-spirit.github.io/scimesh/r/reference/compose_layout.md).

## Usage

``` r
stack_vertical(
  ...,
  colorbar = NULL,
  colorbar_height = 80L,
  colorbar_width = 80L,
  background = c(0, 0, 0, 0),
  colorbar_side = c("right", "left"),
  crop = FALSE
)
```

## Arguments

- ...:

  Images from
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md),
  or a list of images.

- colorbar:

  Optional colorbar from
  [`colorbar_horizontal()`](https://dfsp-spirit.github.io/scimesh/r/reference/colorbar_horizontal.md)
  or
  [`colorbar_vertical()`](https://dfsp-spirit.github.io/scimesh/r/reference/colorbar_vertical.md).

- colorbar_height:

  Height of the colorbar in pixels.

- colorbar_width:

  Width of the colorbar in pixels.

- background:

  Background RGBA color for padding.

- colorbar_side:

  Side for the colorbar: `"right"` (default) or `"left"`.

- crop:

  If `TRUE`, crop whitespace from the output.

## Value

A composed image list.

## Examples

``` r
mesh1 <- generate_cuboid(c(0, 2, 0), c(1, 0.5, 1), c(1, 0, 0, 1))
mesh2 <- generate_cuboid(c(0, -2, 0), c(1, 0.5, 1), c(0, 1, 0, 1))
img1 <- render_mesh(mesh1$vertices, mesh1$triangles)
img2 <- render_mesh(mesh2$vertices, mesh2$triangles)
result <- stack_vertical(img1, img2)
tmp_file <- tempfile(fileext = ".png")
write_png(result, tmp_file)
```
