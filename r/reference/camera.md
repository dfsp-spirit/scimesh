# Create a camera specification

Defines a camera for rendering by specifying the eye position, look-at
center, up vector, projection type, and field of view.

## Usage

``` r
camera(
  eye,
  center,
  up = c(0, 1, 0),
  projection = c("perspective", "orthographic"),
  fov = 45
)
```

## Arguments

- eye:

  Numeric vector of length 3: camera position.

- center:

  Numeric vector of length 3: point the camera looks at.

- up:

  Numeric vector of length 3: camera up direction.

- projection:

  Projection type: `"perspective"` (default) or `"orthographic"`.

- fov:

  Field of view in degrees (perspective only).

## Value

A camera list suitable for
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

## Examples

``` r
cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
cam$eye
#> [1] 0 0 5
```
