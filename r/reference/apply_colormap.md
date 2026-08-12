# Apply a colormap to numerical data

Maps numeric per-vertex (or per-element) data to RGBA colours using a
colormap. Handles multi-dataset data (e.g., two brain hemispheres), NaN
values, and optional outlier clipping via winsorizing.

## Usage

``` r
apply_colormap(
  data,
  colormap = viridis_colormap(256L),
  limits = NULL,
  nan_color = c(0.5, 0.5, 0.5, 1),
  winsor_percentiles = NULL
)
```

## Arguments

- data:

  A numeric vector, or a list of numeric vectors for multi-dataset
  mapping (e.g., list(lh_data, rh_data)).

- colormap:

  A colormap specification: a function f(n) returning n hex colour
  strings (e.g., viridis_colormap), a character vector of hex colours,
  or an Nx3/Nx4 matrix of RGBA values in \[0,1\]. Default is
  viridis_colormap(256L).

- limits:

  How the data value range is determined. NULL (default) auto-detects
  from finite values after winsorizing. c(min, max) sets an explicit
  fixed range. "global" pools all datasets for a shared range. "each"
  uses independent per-dataset ranges.

- nan_color:

  RGBA colour for NaN/NA values as a length-3 (RGB) or length-4 (RGBA)
  numeric vector in \[0,1\]. Default mid-grey.

- winsor_percentiles:

  Optional c(lower, upper) percentiles for outlier clipping, e.g.
  c(0.02, 0.98). NULL disables winsorizing.

## Value

If data is a single vector: an Nx4 numeric matrix of RGBA colours. If
data is a list: a list of Nx4 matrices.

Attributes on the result provide metadata. Single-dataset: data_min,
data_max, raw_min, raw_max, winsor_lo, winsor_hi, nan_count.
Multi-dataset: pooled_data_min, pooled_data_max (use for a colourbar),
data_ranges, winsor_cutoffs, nan_counts.

## Examples

``` r
data <- c(1.2, 3.4, NA, 2.1, 5.0, 2.8)
colors <- apply_colormap(data)

noisy <- c(rnorm(95, mean = 50, sd = 10), 200, -50)
colors <- apply_colormap(noisy, winsor_percentiles = c(0.02, 0.98))

lh <- c(2.3, 2.1, NA, 3.4)
rh <- c(2.5, 2.0, NA, 3.1)

colors <- apply_colormap(list(lh, rh),
    colormap = viridis_colormap(256L),
    limits = "global",
    winsor_percentiles = c(0.02, 0.98),
    nan_color = c(1, 1, 1, 1))
```
