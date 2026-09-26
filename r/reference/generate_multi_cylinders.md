# Generate multiple cylinders as a single mesh

Batched variant of
[`generate_cylinder()`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_cylinder.md):
all cylinders are generated into one mesh. This is the function to use
for thousands of straight edges, e.g. the edges of a network graph or a
connectome. Pass `caps = FALSE` to leave the ends open, which is usually
what you want when the ends are hidden inside spherical nodes (and
roughly halves the geometry).

## Usage

``` r
generate_multi_cylinders(
  from,
  to,
  radii = 0.1,
  colors = c(1, 1, 1, 1),
  segments = 12L,
  caps = TRUE
)
```

## Arguments

- from:

  Nx3 numeric matrix of start points (or a length-3 vector).

- to:

  Nx3 numeric matrix of end points (same number of rows as `from`).

- radii:

  Numeric vector of radii (length 1, recycled; or one per cylinder).

- colors:

  RGBA colour(s): a single vector applied to all cylinders, or an Nx4
  numeric matrix (values in `[0, 1]`, alpha optional).

- segments:

  Subdivision count around the circumference (default 12).

- caps:

  Whether to close both ends of every cylinder (default TRUE).

## Value

A mesh descriptor list with `vertices`, `triangles` and `colors`.

## See also

[`generate_cylinder`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_cylinder.md),
[`generate_tubes`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tubes.md)

## Examples

``` r
from <- matrix(c(0, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE)
to   <- matrix(c(0, 3, 0, 1, 3, 0), ncol = 3, byrow = TRUE)
mesh <- generate_multi_cylinders(from, to, radii = 0.1,
                                 colors = c(0.7, 0.7, 0.7, 1), caps = FALSE)
nrow(mesh$vertices) > 0
#> [1] TRUE
```
