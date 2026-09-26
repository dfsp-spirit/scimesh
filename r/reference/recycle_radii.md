# Recycle a per-primitive radius vector to the requested length

A single value is applied to all primitives, a vector of the exact
length is used as-is, and an empty (or NULL) input means "no radii
given", which the C++ generators interpret as radius 1.0.

## Usage

``` r
recycle_radii(radii, n, arg_name = "radii")
```

## Arguments

- radii:

  Numeric vector of radii, or NULL.

- n:

  Number of primitives.

- arg_name:

  Name of the argument, used in error messages.

## Value

Numeric vector of length \`n\`, or a zero-length vector.
