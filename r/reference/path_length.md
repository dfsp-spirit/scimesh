# Length of a path

Sums the distances between consecutive points, plus the closing distance
from the last point back to the first for a closed path. Useful to pick
a sensible `step` for
[`resample_path`](https://dfsp-spirit.github.io/scimesh/r/reference/resample_path.md),
or to derive a sampling density from the size of the data.

## Usage

``` r
path_length(path, closed = FALSE)
```

## Arguments

- path:

  Nx3 numeric matrix of path points (or a length-3 vector).

- closed:

  Whether the path loops back to its first point (default `FALSE`).

## Value

A single number: the arc length of the path (0 for fewer than two
points).

## See also

[`resample_path`](https://dfsp-spirit.github.io/scimesh/r/reference/resample_path.md)

## Examples

``` r
path_length(matrix(c(0, 0, 0, 1, 0, 0, 1, 1, 0), ncol = 3, byrow = TRUE))
#> [1] 2
```
