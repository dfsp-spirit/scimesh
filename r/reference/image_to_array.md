# Convert a rendered image to an RGBA array

Converts the output of
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)
into a 3-dimensional R array of dimensions (height x width x 4) with
RGBA channels.

## Usage

``` r
image_to_array(image)
```

## Arguments

- image:

  An image list returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

## Value

A 3D array of dimensions (height, width, 4) with values in `[0, 1]`.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(mesh$vertices, mesh$triangles)
arr <- image_to_array(img)
dim(arr)  # height x width x 4
#> [1] 600 800   4
```
