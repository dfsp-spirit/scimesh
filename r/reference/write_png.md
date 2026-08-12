# Write a rendered image to a PNG file

Writes the output of
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)
to a PNG file using the built-in C++ PNG writer (stb_image_write). No
additional R packages are required.

## Usage

``` r
write_png(image, filename)
```

## Arguments

- image:

  An image list returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

- filename:

  Output PNG file path.

## Value

No return value; called for side effects.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(mesh$vertices, mesh$triangles)
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)
```
