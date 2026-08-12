# Generate a cone mesh

Creates a cone from `base` to `tip` with the given base `radius`,
subdivided into `segments` around the axis. The base cap is included.

## Usage

``` r
generate_cone(base, tip, radius = 0.5, segments = 32, color = c(1, 1, 1, 1))
```

## Arguments

- base:

  Length-3 vector: centre of the circular base.

- tip:

  Length-3 vector: tip of the cone.

- radius:

  Base radius.

- segments:

  Subdivision count (default 32).

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_cone(c(0, -1, 0), c(0, 1, 0), radius = 0.8)
nrow(mesh$vertices)
#> [1] 97
```
