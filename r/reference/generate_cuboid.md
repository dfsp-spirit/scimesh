# Generate a cuboid mesh

Creates an axis-aligned cuboid (box) centred at `center` with the given
half-extents along each axis.

## Usage

``` r
generate_cuboid(center, half_extents, color = c(0.7, 0.7, 0.7, 1))
```

## Arguments

- center:

  Length-3 vector: centre of the cuboid.

- half_extents:

  Length-3 vector: half-width, half-height, half-depth.

- color:

  Length-4 RGBA colour (0-1 scale).

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 2, 0.5))
nrow(mesh$vertices)
#> [1] 24
nrow(mesh$triangles)
#> [1] 12
```
