# Render multiple spheres from point data

Generates a merged sphere mesh from a set of center points, radii, and
colors, then renders it with the given camera and options.

## Usage

``` r
render_spheres(
  centers,
  radii,
  colors,
  camera,
  options = render_options(),
  segments = 16L
)
```

## Arguments

- centers:

  Nx3 numeric matrix of sphere centre coordinates.

- radii:

  Numeric vector of sphere radii (length N, or 1 recycled to N).

- colors:

  Nx4 numeric matrix of RGBA colours (0-1 scale), or a single colour
  recycled to N.

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).

- options:

  Render options from
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

- segments:

  Number of latitude/longitude segments per sphere (default 16).

## Value

An image list with `width`, `height`, `pixels`.

## Examples

``` r
centers <- matrix(c(0, 2, 4, 0, 0, 0, 0, 0, 0), ncol = 3)
img <- render_spheres(centers, radii = 0.5,
                      colors = c(1, 0, 0, 1),
                      camera = camera_auto(centers))
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)
```
