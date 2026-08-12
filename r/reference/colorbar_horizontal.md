# Generate a horizontal colorbar image

Creates a horizontal colorbar as a 4-channel RGBA array. The color strip
and optional tick labels are rendered to PNG using base R graphics
(headless-safe). Returns a 3D array suitable for
[`image_to_array()`](https://dfsp-spirit.github.io/scimesh/r/reference/image_to_array.md)
or direct composition.

## Usage

``` r
colorbar_horizontal(
  colormap,
  n_colors = 256L,
  width = 600L,
  height = 80L,
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

  A vector of colors or a function returning colors (e.g.
  [`grDevices::hcl.colors`](https://rdrr.io/r/grDevices/palettes.html)).

- n_colors:

  Number of discrete color segments in the gradient.

- width:

  Output width in pixels.

- height:

  Output height in pixels.

- ticks:

  Numeric vector of tick positions in data units (matching
  `data_range`). If `NULL`, ticks are computed automatically via
  [`pretty()`](https://rdrr.io/r/base/pretty.html) restricted to the
  data range.

- tick_labels:

  Character vector of tick labels. If `NULL`, defaults to formatted tick
  values.

- data_range:

  The data range that `ticks` are specified in. Defaults to `c(0, 1)`.

- label_cex:

  Label size multiplier.

- title:

  Optional title string drawn above the color strip (horizontal) or to
  the right (vertical).

- background:

  Background RGBA color (0-1 scale).

## Value

A 3D array of dimensions (height, width, 4) with values in `[0, 1]`.

## Examples

``` r
cbar <- colorbar_horizontal(viridis_colormap, data_range = c(-2, 3),
    title = "Value")
dim(cbar)  # height x width x 4
#> [1]  80 600   4
```
