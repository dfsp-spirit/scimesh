# Scale a mesh uniformly or per-axis

Scale a mesh uniformly or per-axis

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
