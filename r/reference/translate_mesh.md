# Translate a mesh

Translate a mesh

## Usage

``` r
translate_mesh(mesh, translation)
```

## Arguments

- mesh:

  A mesh descriptor list.

- translation:

  Length-3 numeric vector (x, y, z).

## Value

A new mesh descriptor list with translated vertices.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
moved <- translate_mesh(mesh, c(5, 0, 0))
colMeans(moved$vertices)
#> [1] 5 0 0
```
