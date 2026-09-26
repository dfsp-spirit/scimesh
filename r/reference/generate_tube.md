# Generate a tube (generalized cylinder) along a path

Sweeps a circular cross-section along the points of `path`, which allows
for curved shapes such as arcs, Bezier samples of network edges or
streamlines. A path of exactly two points produces the same mesh as
[`generate_cylinder()`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_cylinder.md).

## Usage

``` r
generate_tube(
  path,
  radius = 0.1,
  segments = 12L,
  color = c(1, 1, 1, 1),
  cap_start = TRUE,
  cap_end = TRUE
)
```

## Arguments

- path:

  Nx3 numeric matrix of path points (or a length-3 vector).

- radius:

  Tube radius (default 0.1).

- segments:

  Subdivision count around the circumference (default 12).

- color:

  Length-4 RGBA colour.

- cap_start:

  Whether to close the beginning of the tube (default TRUE).

- cap_end:

  Whether to close the end of the tube (default TRUE).

## Value

A mesh descriptor list.

## Details

The cross-section frames are computed by parallel transport
(rotation-minimizing frames), so the tube does not twist around its own
axis. Consecutive duplicate points are removed; a path with fewer than
two distinct points yields an empty mesh.

## See also

[`generate_tubes`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tubes.md),
[`generate_cylinder`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_cylinder.md)

## Examples

``` r
path <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
arc <- generate_tube(path, radius = 0.1, segments = 12,
                     cap_start = FALSE, cap_end = FALSE)
nrow(arc$vertices) > 0
#> [1] TRUE
```
