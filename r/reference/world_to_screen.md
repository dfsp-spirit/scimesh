# Project world coordinates to image pixels

Runs the same view and projection as the renderer, so the result lands
exactly on the rendered image. Useful to place screen-space annotations
([`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)`(space = "screen")`)
next to a 3D location, or to draw callout lines with
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md)
in image coordinates.

## Usage

``` r
world_to_screen(points, camera, width, height, options = NULL)
```

## Arguments

- points:

  An Nx3 numeric matrix of world coordinates (or a length-3 vector for a
  single point).

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).

- width, height:

  Size of the rendered image in pixels.

- options:

  Render options, used for the projection type and the clipping planes.
  `NULL` uses default options with the given size.

## Value

A data frame with one row per input point and the columns `x`, `y`
(pixels, origin top left), `depth` (NDC depth, smaller is closer) and
`in_front` (whether the point is in front of the camera; for `FALSE` the
pixel coordinates are not meaningful).

## See also

[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)

## Examples

``` r
sph <- generate_sphere(c(0, 0, 0), radius = 1)
cam <- camera_auto(list(sph), direction = c(0, 0, 1))
opts <- render_options(width = 400, height = 300)
world_to_screen(matrix(c(0, 1, 0), ncol = 3), cam, 400, 300, opts)
#>     x        y     depth in_front
#> 1 200 75.42025 0.9588302     TRUE
```
