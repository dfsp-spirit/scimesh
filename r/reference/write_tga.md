# Write a rendered image to a TGA file

Writes the output of
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)
to a TGA file using scimesh's own C++ TGA writer (no external
dependencies). TGA output is uncompressed true-color.

## Usage

``` r
write_tga(image, filename, use24bit = FALSE)
```

## Arguments

- image:

  An image list returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

- filename:

  Output TGA file path.

- use24bit:

  If `TRUE`, write 24-bit RGB (no alpha channel). The default `FALSE`
  writes 32-bit RGBA.

## Value

No return value; called for side effects.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(mesh$vertices, mesh$triangles)
tmp_file <- tempfile(fileext = ".tga")
write_tga(img, tmp_file)
```
