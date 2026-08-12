# Generate an arrow mesh

Creates a 3D arrow from `from` to `to`, with a cylindrical shaft and a
conical head.

## Usage

``` r
generate_arrow(
  from,
  to,
  shaft_radius = 0.1,
  head_radius = 0.3,
  head_length = 0.6,
  segments = 32,
  color = c(1, 1, 1, 1)
)
```

## Arguments

- from:

  Length-3 start point.

- to:

  Length-3 end point (tip of the arrowhead).

- shaft_radius:

  Radius of the shaft cylinder.

- head_radius:

  Radius at the base of the conical head.

- head_length:

  Length of the arrowhead.

- segments:

  Subdivision count (default 32).

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_arrow(c(0, 0, 0), c(0, 2, 0))
nrow(mesh$vertices)
#> [1] 227
```
