# Generate a planar quad mesh

Creates a flat rectangular plane centred at `center` and oriented
perpendicular to `normal`.

## Usage

``` r
generate_plane(
  center = c(0, 0, 0),
  normal = c(0, 1, 0),
  half_size_x = 1,
  half_size_y = 1,
  color = c(0.7, 0.7, 0.7, 1)
)
```

## Arguments

- center:

  Length-3 vector: centre of the plane.

- normal:

  Length-3 vector: surface normal.

- half_size_x:

  Half-extent along the first tangent axis.

- half_size_y:

  Half-extent along the second tangent axis.

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_plane(c(0, 0, 0), normal = c(0, 1, 0),
                       half_size_x = 2, half_size_y = 1)
nrow(mesh$vertices)
#> [1] 8
```
