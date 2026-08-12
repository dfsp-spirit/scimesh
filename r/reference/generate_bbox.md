# Generate a wireframe bounding box mesh

Creates 12 edge segments around an axis-aligned bounding box.

## Usage

``` r
generate_bbox(bbox, color = c(0, 0, 0, 1), radius = 0.01)
```

## Arguments

- bbox:

  A bounding box list from
  [`mesh_bbox()`](https://dfsp-spirit.github.io/scimesh/r/reference/mesh_bbox.md),
  or a mesh descriptor (in which case
  [`mesh_bbox()`](https://dfsp-spirit.github.io/scimesh/r/reference/mesh_bbox.md)
  is called).

- color:

  RGBA colour for the edges (length 4, 0-1 scale).

- radius:

  Cylinder radius for the edges.

## Value

A mesh descriptor list suitable for
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or inclusion in a scene list.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 2, 3))
bbox_mesh <- generate_bbox(mesh)
nrow(bbox_mesh$vertices)
#> [1] 408
```
