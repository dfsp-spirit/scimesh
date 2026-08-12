# Render screen-space point primitives

Renders points as fixed-size filled circles in screen space with depth
testing. Unlike
[`render_spheres()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_spheres.md),
point size is measured in pixels and does not change with camera
distance.

## Usage

``` r
render_points(
  positions,
  colors,
  radius = 3,
  camera = camera_auto(positions),
  options = render_options()
)
```

## Arguments

- positions:

  Nx3 numeric matrix of point positions.

- colors:

  Nx4 numeric matrix of RGBA colours (0-1 scale).

- radius:

  Point radius in pixels.

- camera:

  A camera list.

- options:

  Render options.

## Value

An image list.

## Examples

``` r
pts <- matrix(c(0, 1, 2, 0, 1, 2, 0, 0, 0),
 ncol = 3)
colors = matrix(c(0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1), ncol = 4)
img <- render_points(pts, colors = colors, radius = 5)
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)
```
