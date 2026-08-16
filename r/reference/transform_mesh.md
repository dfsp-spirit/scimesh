# Apply a 4x4 transformation matrix to a mesh

Transforms all vertex positions in a mesh by a 4x4 homogeneous matrix
(applied as `M * (x, y, z, 1)^T`). Vertex colors and normals are
untouched.

## Usage

``` r
transform_mesh(mesh, matrix)
```

## Arguments

- mesh:

  A mesh descriptor list with `vertices` and `triangles`, as returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or built by `scimesh_generate_multi_spheres()` etc.

- matrix:

  A 4x4 numeric matrix.

## Value

A new mesh descriptor list with transformed vertices.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
mat <- diag(4)
mat[1:3, 4] <- c(2, 3, 4)
translated <- transform_mesh(mesh, mat)
translated$vertices[1, ]
#> [1] 1 2 5
```
