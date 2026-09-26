# Sample a Bezier curve from a control polygon

Evaluates a Bezier curve of any degree with de Casteljau's algorithm.
Unlike
[`spline_path`](https://dfsp-spirit.github.io/scimesh/r/reference/spline_path.md),
the control points are \*\*not\*\* points on the curve: only the first
and the last one are, and the rest pull the curve like a magnet. This is
the function to use when the input really is a control polygon (a font
outline, a vector graphics path, a designed shape); use
[`spline_path`](https://dfsp-spirit.github.io/scimesh/r/reference/spline_path.md)
when your points are positions the curve should pass through.

## Usage

``` r
bezier_path(control_points, samples = 64L)
```

## Arguments

- control_points:

  Nx3 numeric matrix of control points (at least 2).

- samples:

  Number of points to emit along the curve (default 64, at least 2).
  Both endpoints are included in the result.

## Value

An Nx3 numeric matrix of path points, or an empty (0-row) matrix if
there are fewer than two control points.

## See also

[`spline_path`](https://dfsp-spirit.github.io/scimesh/r/reference/spline_path.md),
[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md)

## Examples

``` r
# A quadratic Bezier arc, drawn as a tube:
arc <- bezier_path(matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE))
nrow(arc)
#> [1] 64
```
