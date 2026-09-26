# Scale a mesh uniformly or per-axis

Per-vertex normals (if the mesh has any) are scaled as well, using the
inverse transpose of the scaling matrix: a non-uniform scale would
otherwise leave normals pointing in a direction that no longer matches
the surface, which shows up as wrong shading.

## Usage

``` r
scale_mesh(mesh, scale)
```

## Arguments

- mesh:

  A mesh descriptor list.

- scale:

  A single numeric scale factor (uniform) or a length-3 numeric vector
  for per-axis scaling (x, y, z).

## Value

A new mesh descriptor list with scaled vertices.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
big <- scale_mesh(mesh, 3)
flat <- scale_mesh(mesh, c(2, 0.5, 1))
```
