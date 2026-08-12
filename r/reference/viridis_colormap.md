# Viridis colormap

Returns the viridis color palette. A convenience wrapper around
[`grDevices::hcl.colors`](https://rdrr.io/r/grDevices/palettes.html)
that mimics the viridis color scheme without requiring extra packages.

## Usage

``` r
viridis_colormap(n, alpha = 1, direction = 1)
```

## Arguments

- n:

  Number of colors.

- alpha:

  Alpha channel value (0-1).

- direction:

  Forward (1) or reversed (-1) direction.

## Value

A character vector of hex color strings.

## Examples

``` r
cols <- viridis_colormap(10)
plot(1:10, pch = 19, col = cols, cex = 3)

```
