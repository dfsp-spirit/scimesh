# Recycle per-primitive colors to an Nx4 matrix

Accepts a single RGB/RGBA vector (applied to all primitives), a
single-row matrix (recycled), or an Nx4 (or Nx3, alpha is set to 1)
matrix. An empty input means "no colors given", which the C++ generators
interpret as white.

## Usage

``` r
recycle_colors(colors, n, arg_name = "colors")
```

## Arguments

- colors:

  Numeric vector or matrix of RGB/RGBA colors, or NULL.

- n:

  Number of primitives.

- arg_name:

  Name of the argument, used in error messages.

## Value

An Nx4 numeric matrix, or a 0x4 matrix.
