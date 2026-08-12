# Render raw triangles without index buffer

Renders triangle geometry where positions and colours are given as flat
arrays with 3 vertices per triangle (no index buffer). Useful for voxel
renderings, misc3d isosurfaces, and other dynamically generated
geometry.

## Usage

``` r
render_triangles(positions, colors, camera, options = render_options())
```

## Arguments

- positions:

  Nx3 numeric matrix of vertex positions, where N is a multiple of 3 (3
  per triangle).

- colors:

  Nx4 numeric matrix of RGBA colours (0-1 scale).

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).

- options:

  Render options from
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

## Value

An image list with `width`, `height`, `pixels`.

## Examples

``` r
# Render a single red triangle from raw vertices
positions <- matrix(c(0, 0, 0, 1, 0, 0, 0.5, 1, 0), ncol = 3, byrow = TRUE)
colors <- matrix(c(1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1), ncol = 4, byrow = TRUE)
cam <- camera_auto(positions)
img <- render_triangles(positions, colors, cam)
```
