# Render text labels to an image

Convenience wrapper around
[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)
for the case where no meshes are involved: the labels are drawn onto the
background described by the render options. Screen-space labels
(`space = "screen"`) do not use a camera at all, so this is the quick
way to turn a label into an image or to decorate an empty canvas;
world-space labels need a `camera`.

## Usage

``` r
render_text(positions, text, camera = NULL, options = NULL, ...)
```

## Arguments

- positions:

  Anchor positions, see
  [`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md).

- text:

  Character vector of labels, see
  [`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md).

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).
  Ignored for screen-space labels; when `NULL`, a default camera is
  used.

- options:

  Render options from
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

- ...:

  Further arguments passed to
  [`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)
  (for example `size`, `space`, `color`, `halo_color`).

## Value

A list with components `width`, `height`, and `pixels` (raw vector of
RGBA values).

## See also

[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md),
[`render_scene`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)

## Examples

``` r
img <- render_text(c(20, 20), "figure A", space = "screen",
                   adj = c(0, 1), size = 24,
                   options = render_options(width = 300, height = 80))
tmp <- tempfile(fileext = ".png")
write_png(img, tmp)
```
