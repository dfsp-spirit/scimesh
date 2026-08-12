# Custom diverging colormap for neuroimaging

Returns a blue-white-red diverging colormap suitable for displaying
signed morphometry data (e.g. cortical thickness Z-scores).

## Usage

``` r
diverging_colormap(n)
```

## Arguments

- n:

  Number of colors.

## Value

A character vector of hex color strings.

## Examples

``` r
cols <- diverging_colormap(256)
plot(1:256, pch = 15, col = cols, cex = 2, axes = FALSE, xlab = "", ylab = "")

```
