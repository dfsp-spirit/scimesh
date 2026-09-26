# Validate and normalize a set of 3D points

Internal helper shared by the generator and render functions. Accepts an
Nx3 numeric matrix or a single length-3 numeric vector (which is treated
as a single point) and returns an Nx3 numeric matrix of storage mode
double, as expected by the C++ layer. An empty (0-row) matrix is allowed
and means "no points"; the generators then return an empty mesh.

## Usage

``` r
check_points_matrix(x, arg_name = "x")
```

## Arguments

- x:

  A numeric matrix with 3 columns, or a length-3 numeric vector.

- arg_name:

  Name of the argument, used in error messages.

## Value

An Nx3 numeric matrix.
