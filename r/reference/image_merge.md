# Merge two images side by side or stacked

Merges another image into this one at the specified edge. For left/right
merging, the heights must match. For top/bottom, the widths must match.

## Usage

``` r
image_merge(image, other, direction)
```

## Arguments

- image:

  An image list.

- other:

  Another image list.

- direction:

  One of `"left"`, `"right"`, `"top"`, `"bottom"`.

## Value

A new image list with the merged dimensions.

## Examples

``` r
cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
sphere <- generate_sphere(c(0, 0, 0), radius = 0.5)
left  <- render_mesh(sphere$vertices, sphere$triangles)
right <- render_mesh(cube$vertices, cube$triangles)
merged <- image_merge(left, right, "right")
```
