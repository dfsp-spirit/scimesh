# Generate a vertical colorbar image

Creates a vertical colorbar as a 4-channel RGBA array.

## Usage

``` r
colorbar_vertical(
  colormap,
  n_colors = 256L,
  width = 80L,
  height = 600L,
  ticks = NULL,
  tick_labels = NULL,
  data_range = c(0, 1),
  label_cex = 1,
  title = NULL,
  background = c(1, 1, 1, 1)
)
```

## Arguments

- colormap:

  A vector of colors or a function returning colors.

- n_colors:

  Number of discrete color segments in the gradient.

- width:

  Output width in pixels.

- height:

  Output height in pixels.

- ticks:

  Numeric vector of tick positions in data units. If `NULL`, ticks are
  computed automatically via
  [`pretty()`](https://rdrr.io/r/base/pretty.html) restricted to the
  data range.

- tick_labels:

  Character vector of tick labels.

- data_range:

  The data range that `ticks` are specified in.

- label_cex:

  Label size multiplier.

- title:

  Optional title string drawn to the right of the color strip.

- background:

  Background RGBA color (0-1 scale).

## Value

A 3D array of dimensions (height, width, 4) with values in `[0, 1]`.

## Examples

``` r
cbar <- colorbar_vertical(viridis_colormap, data_range = c(0, 100),
    title = "Count")
dim(cbar)
#> [1] 600  80   4
```
