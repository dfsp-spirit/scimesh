# Change whether line layers of a scene contribute to its bounds

Sets the `affects_bounds` flag of one or more line layers of a scene,
see
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md).
A layer with the flag set contributes to the bounding box of the scene,
and thus to the extent that a camera fitted to the scene
([`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md))
has to cover; a layer without it is ignored when the bounds are
computed, which is what you want for decorational lines.

## Usage

``` r
scene_set_line_affects_bounds(
  scene,
  index = NULL,
  name = NULL,
  affects_bounds = TRUE
)
```

## Arguments

- scene:

  A scene descriptor list, see
  [`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md).

- index:

  Integer vector, the positions (1-based) of the layers to update, or
  `NULL` to select them by `name`.

- name:

  Character vector, the names of the layers to update, or `NULL` to
  select them by `index`. Bare line layers (which are not wrapped into a
  scene node) have no name.

- affects_bounds:

  Whether the selected layers contribute to the scene bounds (default
  `TRUE`), see
  [`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md).

## Value

The scene with the updated layers, invisibly. Since a scene is a plain
list, the update has to be assigned to take effect, e.g.
`sc <- scene_set_line_affects_bounds(sc, name = "leader", affects_bounds = FALSE)`.

## Details

The layers can be selected by position (`index`) or by name (`name`, for
layers that were added as scene nodes with a `name`). Exactly one of the
two has to be given. A scene that contains no mesh at all is framed by
its line layers even when they all opted out, since there would
otherwise be no geometry to derive a camera from.

## See also

[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md),
[`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md)

## Examples

``` r
from <- matrix(c(-1, 0, 0, 5, 0, 0), ncol = 3, byrow = TRUE)
to   <- matrix(c(1, 0, 0, 6, 0, 0), ncol = 3, byrow = TRUE)
# The second segment is a decorational leader line pointing away from the data.
sc <- scene(list(generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5))),
            lines = list(list(lines = line_layer(from, to), name = "edges")))
sc <- scene_set_line_affects_bounds(sc, index = 1, affects_bounds = TRUE)
```
