# Generate multiple tubes as a single mesh

Batched variant of
[`generate_tube()`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md):
all tubes are generated into one mesh. Paths may differ in length. This
is the function to use for curved edges, e.g. connectome edges drawn as
arcs.

## Usage

``` r
generate_tubes(
  paths,
  radii = 0.1,
  colors = c(1, 1, 1, 1),
  segments = 12L,
  caps = FALSE
)
```

## Arguments

- paths:

  List of Nx3 numeric matrices (one per tube). Each path needs at least
  two distinct points to produce geometry.

- radii:

  Numeric vector of radii (length 1, recycled; or one per tube).

- colors:

  RGBA colour(s): a single vector applied to all tubes, or an Nx4
  numeric matrix (values in `[0, 1]`, alpha optional).

- segments:

  Subdivision count around the circumference (default 12).

- caps:

  Whether to close both ends of every tube (default FALSE, since batched
  tubes are typically connected at their ends).

## Value

A mesh descriptor list.

## See also

[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md),
[`generate_multi_cylinders`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_multi_cylinders.md)

## Examples

``` r
paths <- list(matrix(c(0, 0, 0, 1, 1, 0), ncol = 3, byrow = TRUE),
              matrix(c(0, 0, 2, 1, 1, 2, 2, 0, 2), ncol = 3, byrow = TRUE))
mesh <- generate_tubes(paths, radii = 0.05, segments = 8)
nrow(mesh$vertices) > 0
#> [1] TRUE
```
