# Generate a cylinder mesh

Creates a cylinder from `start` to `end` with the given `radius`,
subdivided into `segments` around the axis. Both end caps are included.

## Usage

``` r
generate_cylinder(
  start,
  end,
  radius = 0.5,
  segments = 32,
  color = c(1, 1, 1, 1)
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

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_cylinder(c(0, -1, 0), c(0, 1, 0), radius = 0.5)
nrow(mesh$vertices)
#> [1] 130
```
