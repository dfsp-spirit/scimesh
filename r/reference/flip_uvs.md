# Flip the texture coordinates of a mesh vertically

scimesh stores texture coordinates in **image space**, with `v = 0` at
the *top* edge of the texture image — the same rule as every other
coordinate in scimesh (`c(0, 0)` addresses the top-left pixel of the
texture image, `c(1, 1)` the bottom-right one). OBJ and PLY files,
OpenGL, rgl and tools like Blender and MeshLab use the opposite
convention (`v = 0` at the bottom), so UVs taken from those sources have
to be converted once; this function does that, instead of you having to
rewrite the second column by hand.

## Usage

``` r
flip_uvs(mesh)
```

## Arguments

- mesh:

  A mesh descriptor (scimesh or rgl format, see
  [`as_scimesh_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/as_scimesh_mesh.md)).

## Value

The mesh with flipped UVs.

## Details

Geometry, colors and normals are untouched. A mesh without texture
coordinates is returned unchanged, so calling this is safe either way.

## See also

[`render_mesh`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
(the `uv` and `texture` arguments)

## Examples

``` r
quad <- list(vertices = matrix(c(-1, -1, 0, 1, -1, 0, 1, 1, 0,
                                 -1, -1, 0, 1, 1, 0, -1, 1, 0),
                               ncol = 3, byrow = TRUE),
             triangles = matrix(c(1, 2, 3, 1, 3, 4), ncol = 3, byrow = TRUE),
             # UVs with v = 0 at the bottom (OBJ/OpenGL convention)
             uv = matrix(c(0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0),
                         ncol = 2, byrow = TRUE))
flipped <- flip_uvs(quad)
flipped$uv[, 2]  # v is now measured from the top of the texture
#> [1] 0 0 1 0 1 1
```
