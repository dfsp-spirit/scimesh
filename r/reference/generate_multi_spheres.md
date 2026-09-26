# Generate multiple spheres as a single mesh

Batched variant of
[`generate_sphere()`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_sphere.md):
all spheres are generated into one mesh with a single vertex/triangle
array, which is much faster than generating and merging them one by one.
This is the function to use for thousands of nodes, e.g. the nodes of a
network graph or a point cloud. The returned mesh can be added to a
scene and rendered with
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)
or
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md).

## Usage

``` r
generate_multi_spheres(
  centers,
  radii = 1,
  colors = c(1, 1, 1, 1),
  segments = 16L
)
```

## Arguments

- centers:

  Nx3 numeric matrix of sphere centres (or a length-3 vector for a
  single sphere).

- radii:

  Numeric vector of radii (length 1, recycled; or one per sphere).

- colors:

  RGBA colour(s): a single vector applied to all spheres, or an Nx4
  numeric matrix (values in `[0, 1]`, alpha optional).

- segments:

  Subdivision count per sphere (default 16).

## Value

A mesh descriptor list with `vertices`, `triangles` and `colors`.

## See also

[`generate_sphere`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_sphere.md),
[`generate_multi_cylinders`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_multi_cylinders.md)

## Examples

``` r
centers <- matrix(c(0, 0, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
mesh <- generate_multi_spheres(centers, radii = c(0.5, 0.3),
                               colors = c(1, 0, 0, 1), segments = 12)
nrow(mesh$vertices) > 0
#> [1] TRUE
```
