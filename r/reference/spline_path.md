# Smooth curve through ordered 3D points

Turns a coarse list of waypoints into a dense, smooth path, which is
what the path-taking geometry functions expect:
[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md)
sweeps a cross-section along a path, and
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md)
draws one straight segment per consecutive pair of points. Handing a
handful of control points to those gives a visibly faceted tube and a
polygonal line; this is how you get the smooth version.

## Usage

``` r
spline_path(
  points,
  method = c("catmull-rom", "bspline"),
  samples_per_segment = 8L,
  closed = FALSE,
  alpha = 0.5
)
```

## Arguments

- points:

  Nx3 numeric matrix of points the curve has to pass through (or a
  length-3 vector for a single point). An open curve needs at least 2
  points, a closed one at least 3 (`"bspline"`: at least 4).

- method:

  Either `"catmull-rom"` (interpolating, default) or `"bspline"`
  (approximating, smoother; see the description).

- samples_per_segment:

  Number of points to generate per segment between two input points
  (default 8, clamped to at least 1). This is the knob that controls how
  smooth the result looks when rendered.

- closed:

  Whether the curve loops back to its first point (default `FALSE`). A
  closed path ends on a copy of its first point.

- alpha:

  Parameterization exponent of the Catmull-Rom curve (default `0.5`, see
  the description). Ignored for `"bspline"`.

## Value

An Nx3 numeric matrix of path points, ready for
[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md),
[`generate_tubes`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tubes.md),
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md)
or
[`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).
An empty (0-row) matrix if there are not enough distinct points for the
chosen curve.

## Details

Two curves are available, and they differ in a way that matters:

- `"catmull-rom"` (the default) \*\*interpolates\*\*: the curve passes
  through every input point, and each point's tangent is derived from
  its two neighbours. This is what you want when the input points are
  positions that the curve has to hit (measured or digitized data).

- `"bspline"` \*\*approximates\*\*: the points act as a control cage
  that the curve stays inside, which irons out noise instead of
  reproducing it, and the curve is \\C^2\\ continuous everywhere. It
  generally does not pass through the input points.

For the Catmull-Rom curve, `alpha` selects the parameterization and is
the knob that matters most on real data:

- `0.5` (default) — \*centripetal\*. Knot intervals grow with the square
  root of the chord length, which prevents the cusps, loops and
  self-intersections that the uniform curve produces when the spacing of
  the input points is uneven (long segment followed by a short one, the
  norm for measured data).

- `1` — chord length: still free of cusps, but can overshoot more.

- `0` — uniform: the textbook curve, exact on evenly spaced points and
  misbehaving on everything else.

## See also

[`bezier_path`](https://dfsp-spirit.github.io/scimesh/r/reference/bezier_path.md),
[`resample_path`](https://dfsp-spirit.github.io/scimesh/r/reference/resample_path.md),
[`path_curvature`](https://dfsp-spirit.github.io/scimesh/r/reference/path_curvature.md),
[`generate_tube`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tube.md)

## Examples

``` r
waypoints <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0),
                    ncol = 3, byrow = TRUE)
path <- spline_path(waypoints, samples_per_segment = 8)
nrow(path)  # 3 segments * 8 samples + the final point
#> [1] 25

# A smooth tube through the waypoints, instead of a faceted one:
mesh <- generate_tube(waypoints, radius = 0.1)
smooth <- generate_tube(path, radius = 0.1)

# An approximating (noise-reducing) closed loop:
loop <- spline_path(waypoints, method = "bspline", closed = TRUE)
```
