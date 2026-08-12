# Convert a scimesh mesh to rgl tmesh3d format

Builds an rgl-compatible triangular mesh from a scimesh mesh descriptor
so that the result can be used with `rgl::shade3d()` or other rgl
functions.

## Usage

``` r
mesh_to_rgl(mesh, color = NULL, face_color = NULL)
```

## Arguments

- mesh:

  A scimesh mesh descriptor list with `vertices` (Nx3 matrix) and
  `triangles` (Mx3 integer matrix, 1-based).

- color:

  Optional per-vertex colour, either a single length-4 RGBA vector
  (applied to all vertices) or an Nx4 matrix.

- face_color:

  Optional per-face colour (Mx4 matrix).

## Value

A list with components `vb` (4xN homogeneous coordinates), `it` (3xM
1-based index matrix), and optionally `normals` and `mat` (material),
suitable for use with rgl's `tmesh3d()` and `shade3d()`.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
rgl_mesh <- mesh_to_rgl(mesh)
str(rgl_mesh)
#> List of 4
#>  $ vb     : num [1:4, 1:24] -1 -1 1 1 1 -1 1 1 1 1 ...
#>  $ it     : int [1:3, 1:12] 1 2 3 1 3 4 5 6 7 5 ...
#>  $ normals: num [1:3, 1:24] 0 0 1 0 0 1 0 0 1 0 ...
#>  $ mat    :List of 1
#>   ..$ color: num [1:4, 1:24] 0.7 0.7 0.7 1 0.7 ...
```
