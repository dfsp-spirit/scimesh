# Generate a torus mesh

Creates a torus (donut shape) centred at `center`, lying in the XZ
plane.

## Usage

``` r
generate_torus(
  center = c(0, 0, 0),
  major_radius = 1,
  minor_radius = 0.3,
  major_segments = 32,
  minor_segments = 16,
  color = c(0.7, 0.7, 0.7, 1)
)
```

## Arguments

- center:

  Length-3 vector: centre of the torus.

- major_radius:

  Radius of the ring (tube path).

- minor_radius:

  Radius of the tube cross-section.

- major_segments:

  Number of segments around the ring.

- minor_segments:

  Number of segments around the tube.

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_torus(major_radius = 2, minor_radius = 0.5)
nrow(mesh$vertices)
#> [1] 512
```
