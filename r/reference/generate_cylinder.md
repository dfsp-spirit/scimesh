# Generate a cylinder mesh

Creates a cylinder from `start` to `end` with the given `radius`,
subdivided into `segments` around the axis. Both end caps are included
unless `caps = FALSE` is passed.

## Usage

``` r
generate_cylinder(
  start,
  end,
  radius = 0.5,
  segments = 32,
  color = c(1, 1, 1, 1),
  caps = TRUE
)
```

## Arguments

- start:

  Length-3 vector: cylinder start point.

- end:

  Length-3 vector: cylinder end point.

- radius:

  Cylinder radius.

- segments:

  Subdivision count (default 32).

- color:

  Length-4 RGBA colour.

- caps:

  Whether to close both ends with caps (default TRUE). Pass FALSE for an
  open tube, which roughly halves the number of vertices and triangles.
  Useful for edges whose ends are hidden by other geometry.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_cylinder(c(0, -1, 0), c(0, 1, 0), radius = 0.5)
nrow(mesh$vertices)
#> [1] 130
open <- generate_cylinder(c(0, -1, 0), c(0, 1, 0), radius = 0.5, caps = FALSE)
nrow(open$vertices)
#> [1] 64
```
