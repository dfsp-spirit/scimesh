# Render a 3D mesh to an image

Renders a single mesh using the scimesh software renderer. The mesh can
be specified either as separate `vertices`/`triangles` matrices, as a
scimesh mesh descriptor list, or as an rgl `tmesh3d`-style list (with
`vb`/`it` components). rgl meshes are transparently converted via
[`mesh_from_rgl()`](https://dfsp-spirit.github.io/scimesh/r/reference/mesh_from_rgl.md).

## Usage

``` r
render_mesh(
  vertices,
  triangles = NULL,
  colors = NULL,
  face_colors = NULL,
  normals = NULL,
  uv = NULL,
  texture = NULL,
  camera = NULL,
  options = render_options()
)
```

## Arguments

- vertices:

  Either an Nx3 numeric matrix of vertex positions, or a scimesh mesh
  descriptor list (with `vertices` and `triangles` components), or an
  rgl-style list (with `vb` and `it` components).

- triangles:

  Mx3 integer matrix of triangle indices (1-based). Ignored when
  `vertices` is a list.

- colors:

  Optional Nx4 numeric matrix of RGBA vertex colors (0-1). Use
  `face_colors` (Mx4) for per-triangle colours instead.

- face_colors:

  Optional Mx4 numeric matrix of per-face RGBA colors, one row per
  triangle. When present, all three vertices of a triangle use the same
  colour. Takes precedence over vertex `colors`.

- normals:

  Optional Nx3 numeric matrix of vertex normals.

- uv:

  Optional Nx2 numeric matrix of texture coordinates (0-1).

- texture:

  Optional texture image as a 3D array (H x W x 3 or 4) with values in
  `[0, 1]`, e.g. from
  [`png::readPNG()`](https://rdrr.io/pkg/png/man/readPNG.html).

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).

- options:

  A render options list from
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

## Value

A list with components `width`, `height`, and `pixels` (raw vector of
RGBA values).

## Examples

``` r
# Render a simple colored triangle
verts <- matrix(c(0, 0, 0,  1, 0, 0,  0.5, 1, 0), ncol = 3, byrow = TRUE)
tris  <- matrix(1L, nrow = 1, ncol = 3)
cols  <- matrix(c(1, 0, 0, 1,  0, 1, 0, 1,  0, 0, 1, 1), ncol = 4, byrow = TRUE)
img <- render_mesh(verts, tris, colors = cols)
tmp_file <- tempfile(fileext = ".png")
write_png(img, tmp_file)

# Render from a mesh descriptor list (scimesh format)
mesh_desc <- list(vertices = verts, triangles = tris, colors = cols)
img <- render_mesh(mesh_desc)
```
