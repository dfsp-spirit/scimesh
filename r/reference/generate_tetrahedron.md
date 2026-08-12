# Generate a tetrahedron mesh

Creates a tetrahedron (triangular pyramid) from four arbitrary 3D
points.

## Usage

``` r
generate_tetrahedron(p0, p1, p2, p3, color = c(0.7, 0.7, 0.7, 1))
```

## Arguments

- p0:

  Length-3 vector: first vertex.

- p1:

  Length-3 vector: second vertex.

- p2:

  Length-3 vector: third vertex.

- p3:

  Length-3 vector: fourth vertex.

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_tetrahedron(
  c(0, 0, 0), c(1, 0, 0),
  c(0.5, 1, 0), c(0.5, 0.5, 1))
nrow(mesh$vertices)
#> [1] 12
```
