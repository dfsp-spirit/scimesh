# Fit a camera to a whole scene

Computes a camera that frames the contents of a scene (see
[`scene`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)),
i.e. its meshes together with the line layers that contribute to the
scene bounds (see
[`line_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/line_layer.md),
parameter `affects_bounds`). Use this instead of
[`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md)
when the camera has to consider something else than a mesh: a scene that
contains only line layers (e.g. a tractogram or a connectome without a
brain surface) is framed by those lines, and decorational layers
(`affects_bounds = FALSE`) are ignored.

## Usage

``` r
camera_fit_scene(
  scene,
  direction = c(0, 0, -1),
  up = c(0, 1, 0),
  fov = 45,
  margin = 1.1,
  projection = c("perspective", "orthographic")
)
```

## Arguments

- scene:

  A scene descriptor list, see
  [`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md).

- direction:

  Length-3 view direction, from the camera towards the scene (default
  `c(0, 0, -1)`, i.e. looking along -Z).

- up:

  Length-3 up vector (default `c(0, 1, 0)`).

- fov:

  Vertical field of view in degrees (default 45).

- margin:

  Scale factor applied to the fitted distance; values above 1 leave a
  margin around the content (default 1.1).

- projection:

  Projection type, `"perspective"` (default) or `"orthographic"`.

## Value

A camera list (see
[`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md))
with class `"scimesh_camera"`.

## Details

The camera is placed on the line from the center of the bounding box
along `direction`, at a distance that makes the content fit into the
field of view, exactly like
[`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md)
does it for a mesh.

## See also

[`camera_auto`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md)
for meshes,
[`scene`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md),
[`scene_set_line_affects_bounds`](https://dfsp-spirit.github.io/scimesh/r/reference/scene_set_line_affects_bounds.md)

## Examples

``` r
# A scene without any mesh is framed by its lines.
line <- line_layer(matrix(c(0, 0, 0), ncol = 3), matrix(c(2, 0, 0), ncol = 3))
sc <- scene(list(), lines = line)
cam <- camera_fit_scene(sc, direction = c(0, 0, -1))
```
