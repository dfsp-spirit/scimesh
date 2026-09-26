# Curvature along a path

Estimates the curvature at every point from the neighbouring points,
using the standard formula \\\kappa = \|x' \times x''\| / \|x'\|^3\\,
which does not care how the points are spaced. On a circle of radius
\\r\\ the values come out as \\1/r\\, and on a straight line as 0.

## Usage

``` r
path_curvature(path, closed = FALSE)
```

## Arguments

- path:

  Nx3 numeric matrix of path points (or a length-3 vector).

- closed:

  Whether the path loops back to its first point (default `FALSE`).

## Value

A numeric vector with one curvature value per input point. The endpoints
of an open path report the value of their only neighbour; fewer than 3
points yield a zero-length vector, since curvature is undefined there.

## Details

This is a diagnostic tool rather than a rendering input: sweeping a tube
of radius \\r\\ along a curve whose curvature reaches \\\kappa\\ folds
the tube inside out, so a path can be swept safely up to a radius of \\1
/ \max(\kappa)\\, which is worth checking for tight turns in measured
data (`max(path_curvature(path))`).

## See also

[`spline_path`](https://dfsp-spirit.github.io/scimesh/r/reference/spline_path.md),
[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md)

## Examples

``` r
# Largest tube radius that does not fold this path onto itself:
path <- spline_path(matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0),
                           ncol = 3, byrow = TRUE))
1 / max(path_curvature(path))
#> [1] 0.2171869
```
