# Auto-frame a camera to fit a mesh or vertex set

Computes a camera position that frames the entire mesh in view. The
camera looks along the given direction, positioned at a distance that
ensures the mesh fits within the field of view.

## Usage

``` r
camera_auto(
  mesh,
  direction = c(0, 0, -1),
  up = c(0, 1, 0),
  fov = 45,
  margin = 1.1,
  rgl_compat = FALSE,
  projection = c("perspective", "orthographic")
)
```

## Arguments

- mesh:

  Either an Nx3 numeric matrix of vertex positions, or a mesh descriptor
  list with a `vertices` component.

- direction:

  The view direction as a length-3 vector. For example, `c(0, 0, -1)`
  looks along the negative Z axis. Ignored when `rgl_compat = TRUE`.

- up:

  The up vector as a length-3 vector. Default `c(0, 1, 0)`. Ignored when
  `rgl_compat = TRUE`.

- fov:

  Field of view in degrees. Default 45° (30° when `rgl_compat = TRUE`).

- margin:

  Extra margin factor (1.0 = tight fit, 1.1 = 10% margin).

- rgl_compat:

  Logical. If `TRUE`, use rgl's camera defaults and bounding-sphere
  distance formula. Default `FALSE`.

- projection:

  Projection type: `"perspective"` (default) or `"orthographic"`. When
  orthographic, the camera distance is computed to tightly frame the
  mesh regardless of FOV.

## Value

A camera list, with S3 class `"scimesh_camera"`.

## Details

When `rgl_compat = TRUE`, the camera mimics rgl's default auto-framing
behaviour: a 30° FOV, 15° elevation, and the distance is computed from
the *bounding sphere* of the mesh (the half-diagonal of the axis-aligned
bounding box), reproducing the formula
`distance = sphere_radius / sin(FOV/2)` used by rgl.

## Examples

``` r
verts <- matrix(c(-1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
                   -1,-1, 1, 1,-1, 1, 1,1, 1, -1,1, 1), ncol = 3, byrow = TRUE)
tris <- matrix(c(0L,3L,2L, 0L,2L,1L, 4L,5L,6L, 4L,6L,7L,
                  0L,1L,5L, 0L,5L,4L, 2L,3L,7L, 2L,7L,6L,
                  0L,4L,7L, 0L,7L,3L, 1L,2L,6L, 1L,6L,5L), ncol = 3, byrow = TRUE)
mesh <- list(vertices = verts, triangles = tris)
cam <- camera_auto(mesh, direction = c(1, 1, 1))
cam_rgl <- camera_auto(mesh, rgl_compat = TRUE)
```
