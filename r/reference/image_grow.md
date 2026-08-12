# Grow an image by adding padding

Expands the canvas by adding pixel rows/columns filled with a background
colour.

## Usage

``` r
image_grow(image, top, bottom, left, right, background)
```

## Arguments

- image:

  An image list.

- top:

  Number of pixel rows to add above.

- bottom:

  Number of pixel rows to add below.

- left:

  Number of pixel columns to add to the left.

- right:

  Number of pixel columns to add to the right.

- background:

  Numeric vector of length 4 with RGBA values in `[0, 1]`.

## Value

A new image list with the expanded dimensions.

## Examples

``` r
cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(cube$vertices, cube$triangles)
img <- image_grow(img, 10, 10, 20, 20, c(1, 1, 1, 1))
```
