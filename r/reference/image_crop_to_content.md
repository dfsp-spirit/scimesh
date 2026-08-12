# Crop an image to its content bounding box

Removes background-coloured margin from the specified edges of the
image. The first non-background pixel found on each edge defines the
crop boundary.

## Usage

``` r
image_crop_to_content(image, direction, background)
```

## Arguments

- image:

  An image list.

- direction:

  One of `"left"`, `"right"`, `"horizontal"` (both left and right),
  `"top"`, `"bottom"`, `"vertical"` (both top and bottom), or `"all"`
  (all four sides).

- background:

  Numeric vector of length 4 with RGBA values in `[0, 1]` defining the
  background colour to crop away.

## Value

A new image list with cropped dimensions.

## Examples

``` r
cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(cube$vertices, cube$triangles,
                   options = render_options(background_color = c(0, 0, 0, 0)))
img <- image_crop_to_content(img, "all", c(0, 0, 0, 0))

```
