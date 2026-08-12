# Generate XYZ axis arrows as cylinder meshes

Creates three coloured arrow meshes (red X, green Y, blue Z) from a
centre point.

## Usage

``` r
generate_axes(center = c(0, 0, 0), size = 1, shaft_radius = 0.02)
```

## Arguments

- center:

  Length-3 vector: origin of the axes.

- size:

  Length of each axis.

- shaft_radius:

  Cylinder radius for axis shafts.

## Value

A mesh descriptor list suitable for
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or inclusion in a scene list.

## Examples

``` r
axes_mesh <- generate_axes(size = 2)
nrow(axes_mesh$vertices)
#> [1] 102
```
