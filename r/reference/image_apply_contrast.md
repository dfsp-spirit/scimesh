# Apply contrast adjustment to an image

Applies a contrast stretch (S-curve) to the RGB channels of a rendered
image. Formula: `(value - 0.5) * contrast + 0.5`, clamped to `[0, 1]`.
The default 1.0 means no change. Values \> 1.0 produce darker darks and
lighter highlights.

## Usage

``` r
image_apply_contrast(image, contrast = 1)
```

## Arguments

- image:

  An image list returned by
  [`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
  or
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

- contrast:

  Contrast multiplier. Default 1.0 (no change). Typical values: 1.1–1.2
  for subtle S-curve, 1.5 for strong.

## Value

A new image list with contrast-adjusted pixel data.

## Examples

``` r
cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
img <- render_mesh(cube$vertices, cube$triangles)
img <- image_apply_contrast(img, contrast = 1.1)

```
