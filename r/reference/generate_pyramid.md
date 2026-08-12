# Generate a square pyramid mesh

Creates a pyramid with a square base centred at `base_center` in the XZ
plane, with the apex above it along Y.

## Usage

``` r
generate_pyramid(
  base_center,
  apex,
  half_width = 1,
  color = c(0.7, 0.7, 0.7, 1)
)
```

## Arguments

- base_center:

  Length-3 vector: centre of the square base.

- apex:

  Length-3 vector: position of the tip.

- half_width:

  Half-width of the square base.

- color:

  Length-4 RGBA colour.

## Value

A mesh descriptor list.

## Examples

``` r
mesh <- generate_pyramid(c(0, 0, 0), c(0, 2, 0), half_width = 1)
mesh$vertices
#>       [,1] [,2] [,3]
#>  [1,]    1    0   -1
#>  [2,]   -1    0   -1
#>  [3,]    0    2    0
#>  [4,]    1    0    1
#>  [5,]    1    0   -1
#>  [6,]    0    2    0
#>  [7,]   -1    0    1
#>  [8,]    1    0    1
#>  [9,]    0    2    0
#> [10,]   -1    0   -1
#> [11,]   -1    0    1
#> [12,]    0    2    0
#> [13,]   -1    0   -1
#> [14,]    1    0   -1
#> [15,]    1    0    1
#> [16,]   -1    0    1
```
