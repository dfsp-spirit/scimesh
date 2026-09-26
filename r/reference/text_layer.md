# Create a text label layer (2D annotations for a scene)

Bundles strings with anchor positions into a layer that can be added to
a scene (see the `texts` argument of
[`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)),
or drawn directly with
[`render_text`](https://dfsp-spirit.github.io/scimesh/r/reference/render_text.md).

## Usage

``` r
text_layer(
  positions,
  text,
  colors = NULL,
  size = 18,
  font_file = NULL,
  space = c("world", "screen"),
  adj = c(0.5, 0.5),
  offset = c(0, 0),
  line_spacing = 1.2,
  depth_test = TRUE,
  halo_color = NULL,
  halo_width = 1.5,
  rotation = 0
)
```

## Arguments

- positions:

  Anchor positions: an Nx3 numeric matrix for world space, or an Nx2/Nx3
  numeric matrix for `space = "screen"` (third column ignored). A single
  point may be given as a numeric vector of length 2 or 3.

- text:

  Character vector of labels (UTF-8), recycled to `nrow(positions)`. Use
  `"\n"` for line breaks.

- colors:

  RGBA colour(s): a single vector applied to all labels, or an Nx4
  numeric matrix (values in `[0, 1]`, alpha optional). The default
  `NULL` uses the `default_color` of the render options.

- size:

  Text height in output pixels (default 18). Independent of the
  anti-aliasing setting: a label keeps its physical size when
  `aa_samples` is raised.

- font_file:

  Path to a `.ttf` file to use. `NULL` (the default) uses the bundled
  Inter font, see
  [`default_font()`](https://dfsp-spirit.github.io/scimesh/r/reference/default_font.md).

- space:

  `"world"` (default) for positions in the 3D scene, or `"screen"` for
  positions in output pixels.

- adj:

  Numeric vector of length 2 giving where the position sits on the text
  box, in `[0, 1]`: `c(0, 0)` is the bottom left corner, `c(1, 1)` the
  top right one, and the default `c(0.5, 0.5)` centres the text on the
  position.

- offset:

  Numeric vector of length 2: extra offset in output pixels (positive x
  = right, positive y = down), applied after anchoring. Handy to push an
  atom label next to the atom instead of onto it.

- line_spacing:

  Distance between two lines of a multi-line label, as a multiple of the
  font's glyph box height (default 1.2).

- depth_test:

  Whether a label is hidden by geometry in front of its anchor (default
  `TRUE`). Set to `FALSE` to always draw the labels on top of
  everything, which is the right choice for direction annotations
  ("anterior") and for labels on a surface.

- halo_color:

  RGBA colour of the halo (outline) drawn behind the glyphs, which keeps
  labels readable on dark or busy geometry. `NULL` (the default) draws
  no halo.

- halo_width:

  Halo thickness in pixels (default 1.5).

- rotation:

  Rotation of the label in degrees, counter-clockwise, about the anchor
  position (default 0). Use 90 to write along a vertical axis (the usual
  orientation of a y-axis label), 180 for an upside-down label, or any
  other angle to follow an annotation line. The anchor stays fixed while
  the text turns around it.

## Value

A text layer object (a list with class `scimesh_text`) for use in
[`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)
or
[`render_text`](https://dfsp-spirit.github.io/scimesh/r/reference/render_text.md).

## Details

Labels are *billboards*: they always face the camera and keep the size
given in `size` (in output pixels), so they stay readable from any
viewpoint — unlike text that is turned into 3D geometry, which skews as
the camera moves. This is the screen-friendly counterpart of
`rgl::text3d()`, and the intended way to label brain regions, atoms,
panels or figure axes.

Positions are given either in **world space** (the default) or in
**screen space**: with `space = "screen"` the coordinates are pixels of
the output image, measured from the top left corner, which is what you
want for titles, panel tags and captions. World-space labels are
projected with the camera of the scene, so they stick to the annotated
location, and they are hidden by geometry in front of them unless
`depth_test = FALSE`.

## See also

[`render_text`](https://dfsp-spirit.github.io/scimesh/r/reference/render_text.md),
[`text_extent`](https://dfsp-spirit.github.io/scimesh/r/reference/text_extent.md),
[`scene`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md),
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md)

## Examples

``` r
sph <- generate_sphere(c(0, 0, 0), radius = 1)
# world-space label above the sphere
labels <- text_layer(matrix(c(0, 1.4, 0), ncol = 3), "top", size = 20)
labels
#> scimesh text layer with 1 label(s), 20 px, world space
#>   'top'

# screen-space panel tag, positioned by its top left corner
tag <- text_layer(c(10, 12), "A", space = "screen", adj = c(0, 1), size = 28,
                  halo_color = c(1, 1, 1, 0.9))
tag
#> scimesh text layer with 1 label(s), 28 px, screen space, halo
#>   'A'

# multiple labels with per-label colors and positions
multi <- text_layer(matrix(c(-1, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE),
                    c("left", "right"), colors = matrix(c(1, 0, 0, 1,
                                                         0, 0, 1, 1),
                                                        ncol = 4, byrow = TRUE))
multi
#> scimesh text layer with 2 label(s), 18 px, world space
#>   'left', 'right'
```
