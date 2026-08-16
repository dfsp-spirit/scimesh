# Render multiple meshes to an image

Renders a list of meshes as a single scene using the scimesh software
renderer. Each element can be a scimesh mesh descriptor or an rgl-style
mesh (with `vb`/`it`); rgl meshes are transparently converted.

## Usage

``` r
render_scene(meshes, camera = NULL, options = NULL)
```

## Arguments

- meshes:

  Either a `scimesh_scene` object (see
  [`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)),
  a list of mesh descriptors, or a list of scene nodes. Each mesh
  descriptor is a list with components `vertices` (Nx3 matrix),
  `triangles` (Mx3 integer matrix), and optionally `colors`,
  `face_colors`, `normals`, and `default_color`. Elements may also be
  rgl-style lists (with `vb` and `it`), which are converted
  automatically.

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).
  Ignored (falls back to the scene's camera) when `meshes` is a
  `scimesh_scene` and `camera` is `NULL`.

- options:

  A render options list from
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).
  Defaults to the scene's options (if any) or
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

## Value

A list with components `width`, `height`, and `pixels` (raw vector of
RGBA values).

## Examples

``` r
# Render two cubes side by side
cube1 <- generate_cuboid(c(-1.5, 0, 0), c(0.8, 0.8, 0.8), c(1, 0, 0, 1))
cube2 <- generate_cuboid(c( 1.5, 0, 0), c(0.8, 0.8, 0.8), c(0, 0, 1, 1))
cam <- camera_auto(list(cube1, cube2), direction = c(1, 1, 1))
img <- render_scene(list(cube1, cube2), cam,
    render_options(width = 800, height = 600, background_color = c(1, 1, 1, 1)))
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)

# Render multiple meshes together
scimesh_cube <- generate_cuboid(c(-1, 0, 0), c(0.5, 0.5, 0.5))
sphere <- generate_sphere(c(1, 0, 0), radius = 0.5, color = c(0, 1, 0, 1))
cam <- camera_auto(list(scimesh_cube, sphere), direction = c(1, 1, 1))
img <- render_scene(list(scimesh_cube, sphere), cam,
    render_options(width = 400, height = 300, background_color = c(1, 1, 1, 1)))
```
