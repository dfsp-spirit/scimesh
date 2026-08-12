# Render line segments as thin cylinders

Generates a merged cylinder mesh from start/end point pairs, radii, and
colors, then renders it.

## Usage

``` r
render_lines(
  from,
  to,
  radii = 0.1,
  colors,
  camera,
  options = render_options(),
  segments = 12L
)
```

## Arguments

- from:

  Nx3 numeric matrix of segment start points.

- to:

  Nx3 numeric matrix of segment end points.

- radii:

  Numeric vector of cylinder radii (length N, or 1 recycled to N).

- colors:

  Nx4 numeric matrix of RGBA colours, or a single colour recycled to N.

- camera:

  A camera list.

- options:

  Render options.

- segments:

  Number of sides around the cylinder (default 12).

## Value

An image list.

## Examples

``` r
from <- matrix(c(0, 0, 0, 1, 1, 1), ncol = 3, byrow = TRUE)
to   <- matrix(c(3, 0, 0, 0, 3, 0), ncol = 3, byrow = TRUE)
img <- render_lines(from, to, radii = 0.05,
                    colors = c(0, 0, 1, 1),
                    camera = camera_auto(rbind(from, to)))
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)
```
