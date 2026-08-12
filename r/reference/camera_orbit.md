# Orbit a camera around an axis

Rotates a camera's eye position and up vector around its center by a
given angle about a rotation axis. Useful for generating turntable-style
frame sequences.

## Usage

``` r
camera_orbit(camera, axis = c(0, 0, 1), angle_degrees)
```

## Arguments

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).

- axis:

  Rotation axis as a length-3 vector. Default `c(0, 0, 1)` (Z axis).

- angle_degrees:

  Rotation angle in degrees.

## Value

A camera list with S3 class `"scimesh_camera"`.

## Examples

``` r
mesh <- generate_torus(c(0, 0, 0))
cam <- camera_auto(mesh, direction = c(1, 1, 1))
cam2 <- camera_orbit(cam, axis = c(0, 0, 1), angle_degrees = 90)
```
