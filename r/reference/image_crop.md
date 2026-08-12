# Crop an image to a rectangular region

Crop an image to a rectangular region

## Usage

``` r
image_crop(image, x, y, w, h)
```

## Arguments

- image:

  An image list returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or similar.

- x:

  Left edge of the crop region (0-based pixel coordinate).

- y:

  Top edge of the crop region (0-based pixel coordinate).

- w:

  Crop width in pixels.

- h:

  Crop height in pixels.

## Value

A new image list with the cropped dimensions.

## Examples

``` r
cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(cube$vertices, cube$triangles)
img <- image_crop(img, 100, 50, 400, 300)
```
