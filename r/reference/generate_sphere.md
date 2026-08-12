# Generate a sphere mesh

Creates a UV sphere centred at `center` with the given `radius`. The
sphere is subdivided into `segments` rings and segments per ring.

## Usage

``` r
generate_sphere(center, radius = 1, segments = 32, color = c(1, 1, 1, 1))
```

## Arguments

- center:

  Length-3 vector: sphere centre.

- radius:

  Sphere radius.

- segments:

  Subdivision count (default 32).

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_sphere(c(0, 0, 0), radius = 1.5, segments = 32)
nrow(mesh$vertices)
#> [1] 994
```
