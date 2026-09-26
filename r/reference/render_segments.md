# Render line segments directly to an image

Convenience wrapper around
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md)
for the case where no meshes are involved: the segments are drawn (with
a screen-space width) into an image and nothing else. To combine lines
with meshes, add the layer to a scene instead and render that scene.

## Usage

``` r
render_segments(
  from,
  to,
  colors = NULL,
  width = 1,
  camera = NULL,
  options = render_options(),
  lit = FALSE
)
```

## Arguments

- from:

  Nx3 numeric matrix of segment start points (or a length-3 vector).

- to:

  Nx3 numeric matrix of segment end points (same number of rows as
  `from`).

- colors:

  RGBA colour(s): a single vector applied to all segments, or an Nx4
  numeric matrix. `NULL` (the default) uses the `default_color` of the
  render options.

- width:

  Line width in pixels (default 1).

- camera:

  A camera list, e.g. from
  [`camera`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).
  Defaults to a camera framing the segments.

- options:

  Render options, see
  [`render_options`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

- lit:

  Whether to apply lighting (default `FALSE`, flat colour).

## Value

An image list, see
[`render_scene`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

## See also

[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md),
[`render_points`](https://dfsp-spirit.github.io/scimesh/r/reference/render_points.md)

## Examples

``` r
from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
to   <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)
img <- render_segments(from, to, colors = c(1, 0, 0, 1), width = 3)
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)
```
