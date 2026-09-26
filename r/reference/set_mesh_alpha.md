# Set the transparency of a whole mesh

Returns a copy of the mesh in which every vertex (and every face, if
per-face colors are used) has the given alpha value. The renderer blends
meshes whose colors are not fully opaque automatically, so this is all
that is needed to draw a mesh translucently - for example a brain
surface at 10 percent opacity for spatial reference.

## Usage

``` r
set_mesh_alpha(mesh, alpha)
```

## Arguments

- mesh:

  A mesh descriptor list (see `as_scimesh_mesh`), e.g. as returned by
  [`generate_sphere()`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_sphere.md)
  or
  [`read_ply()`](https://dfsp-spirit.github.io/scimesh/r/reference/read_ply.md).

- alpha:

  Alpha value in `[0, 1]`: 0 = fully transparent, 1 = fully opaque.

## Value

A mesh descriptor list with the alpha applied.

## Details

A mesh without colors gets uniform colors first (its `default_color` if
it has one, light gray otherwise), so the mesh keeps its appearance and
only becomes see-through. Use `alpha = 0` for completely invisible
geometry and `alpha = 1` to make a mesh opaque again.

## See also

[`render_mesh`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md),
[`scene`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)

## Examples

``` r
sphere <- generate_sphere(c(0, 0, 0), 1)
ghost  <- set_mesh_alpha(sphere, 0.2)

# Per-vertex alpha (here: every other vertex transparent) can be set
# directly on the color matrix:
cols <- sphere$colors
cols[, 4] <- rep(c(0, 1), length.out = nrow(cols))
sphere$colors <- cols
```
