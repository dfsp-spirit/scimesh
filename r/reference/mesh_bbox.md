# Compute the axis-aligned bounding box of a mesh

Compute the axis-aligned bounding box of a mesh

## Usage

``` r
mesh_bbox(mesh)
```

## Arguments

- mesh:

  A mesh descriptor list with `vertices`.

## Value

A list with `min` and `max` (each length-3 numeric).

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 2, 3))
bb <- mesh_bbox(mesh)
bb$min
#> [1] -1 -2 -3
bb$max
#> [1] 1 2 3
```
