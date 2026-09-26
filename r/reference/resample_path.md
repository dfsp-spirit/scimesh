# Resample a path at a fixed arc length step

Walks along a path and emits a point every `step` units of arc length.
The result has (nearly) uniform point spacing regardless of how the
input was parameterized, which is what makes a swept tube look even:
[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md)
places exactly one cross-section per path point, so uneven spacing means
a tube that is finely subdivided in one place and faceted in another.
Typical use is to even out the output of
[`spline_path`](https://dfsp-spirit.github.io/scimesh/r/reference/spline_path.md),
whose samples are evenly spaced in the curve parameter rather than along
the curve.

## Usage

``` r
resample_path(path, step, closed = FALSE)
```

## Arguments

- path:

  Nx3 numeric matrix of path points (or a length-3 vector).

- step:

  Arc length between samples. Must be a single positive number; other
  values return the path unchanged.

- closed:

  Whether the path loops back to its first point (default `FALSE`).

## Value

An Nx3 numeric matrix of resampled path points. A path without length
(or with fewer than two points) is returned as it is.

## Details

The spacing that is uniform is the \*arc length\* along the path; the
straight-line distances between consecutive samples are slightly shorter
wherever the path curves. An open path always keeps its final point
(with a possibly shorter last step); a closed path ends on a copy of its
first point and is covered in a whole number of steps, so its effective
step can differ from the requested one by a fraction of a percent.

## See also

[`path_length`](https://dfsp-spirit.github.io/scimesh/r/reference/path_length.md),
[`spline_path`](https://dfsp-spirit.github.io/scimesh/r/reference/spline_path.md)

## Examples

``` r
waypoints <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
smooth <- spline_path(waypoints, samples_per_segment = 32)
even <- resample_path(smooth, step = 0.1)
nrow(even)
#> [1] 30
```
