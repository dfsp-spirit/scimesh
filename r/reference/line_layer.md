# Create a line layer (screen-space lines, no geometry)

Bundles a set of independent line segments with a width measured in
pixels into a layer that can be added to a scene (see the `lines`
argument of
[`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)),
or drawn directly with
[`render_segments`](https://dfsp-spirit.github.io/scimesh/r/reference/render_segments.md).
In contrast to tube meshes
([`generate_tubes`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tubes.md)),
no geometry is created: the renderer draws the segments itself, so
thousands of lines cost almost nothing, and a width of 1 stays 1 pixel
wide no matter how far away the geometry is. This is what hardware line
rendering (and `rgl::segments3d()`) does.

## Usage

``` r
line_layer(
  from,
  to,
  colors = NULL,
  width = 1,
  depth_test = TRUE,
  lit = FALSE,
  affects_bounds = TRUE
)
```

## Arguments

- from:

  Nx3 numeric matrix of segment start points (or a length-3 vector for a
  single segment).

- to:

  Nx3 numeric matrix of segment end points (same number of rows as
  `from`).

- colors:

  RGBA colour(s): a single vector applied to all segments, or an Nx4
  numeric matrix (values in `[0, 1]`, alpha optional). The default
  `NULL` uses the `default_color` of the render options.

- width:

  Line width in pixels (default 1).

- depth_test:

  Whether to test the lines against the depth buffer (default `TRUE`).
  Set to `FALSE` to draw them on top of everything, which is only useful
  for opaque lines.

- lit:

  Whether to apply lighting to the lines (default `FALSE`, i.e. a flat
  colour, like hardware-rendered lines).

- affects_bounds:

  Whether this layer contributes to the bounding box of the scene, and
  thus to the camera fitted to it (default `TRUE`, see the description).
  Set to `FALSE` for decorational lines. The flag of a layer that is
  already part of a scene can be changed with
  [`scene_set_line_affects_bounds`](https://dfsp-spirit.github.io/scimesh/r/reference/scene_set_line_affects_bounds.md).

## Value

A line layer object (a list with class `scimesh_lines`) for use in
[`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)
or
[`render_segments`](https://dfsp-spirit.github.io/scimesh/r/reference/render_segments.md).

## Details

Line layers are drawn together with the meshes of the scene, after them
and against the same depth buffer, so opaque meshes can hide lines and
opaque lines can hide meshes. Segments whose colors have an alpha value
below 1 are drawn in the blended pass, back to front, exactly like
translucent triangles.

Lines usually \*are\* the content of a figure (graph or connectome
edges, streamlines, trajectories), so by default a layer contributes to
the bounding box of its scene, exactly like a mesh does: it defines the
extent that the camera has to cover. Set `affects_bounds = FALSE` for a
layer that is decoration rather than content (a leader line to a label,
an axis cross, a scale bar drawn as segments), so that it can never push
the camera away from the data. A scene that contains no mesh at all is
framed by its line layers even when they all opted out, since there
would otherwise be no geometry to derive a camera from.

## See also

[`render_segments`](https://dfsp-spirit.github.io/scimesh/r/reference/render_segments.md),
[`generate_tubes`](https://dfsp-spirit.github.io/scimesh/r/reference/generate_tubes.md)

## Examples

``` r
from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
to   <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)
layer <- line_layer(from, to, colors = c(0.2, 0.2, 0.2, 0.8), width = 2)
sc <- scene(list(generate_sphere(c(0, 0, 0), 0.5)), lines = layer)
```
