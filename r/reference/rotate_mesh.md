# Rotate a mesh around an axis

Rotate a mesh around an axis

## Usage

``` r
rotate_mesh(mesh, angle_rad, axis = c(0, 0, 1))
```

## Arguments

- mesh:

  A mesh descriptor list.

- angle_rad:

  Rotation angle in radians.

- axis:

  Length-3 numeric vector defining the rotation axis.

## Value

A new mesh descriptor list with rotated vertices.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
rotated <- rotate_mesh(mesh, pi / 4, axis = c(0, 1, 0))
rotated$vertices[1, ]
#> [1]  0.000000 -1.000000  1.414214
```
