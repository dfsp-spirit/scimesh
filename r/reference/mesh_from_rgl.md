# Convert an rgl tmesh3d to scimesh mesh format

Extracts vertices and triangle indices from an rgl `tmesh3d` object into
the format expected by
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md).
Does not require the `rgl` package – any list with components `vb` (4xN
homogeneous coordinates) and `it` (3xM index matrix) works.

## Usage

``` r
mesh_from_rgl(tmesh)
```

## Arguments

- tmesh:

  A list with components `vb` and `it`, as produced by `rgl::tmesh3d()`.

## Value

A mesh descriptor list with `vertices` (Nx3) and `triangles` (Mx3,
1-based indices).

## Examples

``` r
fake <- list(vb = rbind(0:3, 0:3, 0:3, rep(1, 4)),
             it = matrix(1:6, nrow = 3))
m <- mesh_from_rgl(fake)
m$vertices
#>      [,1] [,2] [,3]
#> [1,]    0    0    0
#> [2,]    1    1    1
#> [3,]    2    2    2
#> [4,]    3    3    3
m$triangles
#>      [,1] [,2] [,3]
#> [1,]    1    2    3
#> [2,]    4    5    6
```
