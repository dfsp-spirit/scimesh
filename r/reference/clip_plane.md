# Create a clip plane specification

Defines a clipping plane for
[`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).
Geometry on the negative side of the plane is removed, i.e. a point `p`
is kept when `dot(normal, p) + offset >= 0`.

## Usage

``` r
clip_plane(normal, offset = 0, space = c("world", "eye"))
```

## Arguments

- normal:

  Numeric vector of length 3: the plane normal. It points toward the
  side of the plane that is *kept*. It does not have to be unit-length;
  it is normalized internally and `offset` is always interpreted as a
  distance in world units.

- offset:

  Numeric scalar: the signed distance of the plane from the origin along
  `normal`, in world units. For example `offset = -d` places the plane
  at distance `d` from the origin (in the direction of `normal`).

- space:

  Character, either `"world"` (default) or `"eye"`:

  `"world"`

  :   The plane is fixed in world coordinates and does not move when the
      camera moves. This is the convention used by rgl's
      `clipplanes3d()`, VTK/PyVista, ParaView and three.js.

  `"eye"`

  :   The plane is defined relative to the camera, i.e. `offset` is a
      distance from the camera, and the plane moves and rotates with it.

## Value

A list with components `normal`, `offset` and `space`, suitable for the
`clip_planes` argument of
[`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).

## Details

By default the plane is defined in **world space**, so the cut is a
fixed feature of the scene: it does not move when the camera is moved
around, which is what you want for cross-sections and multi-view
figures. Set `space = "eye"` for a camera-relative plane that travels
with the camera (the classic OpenGL `glClipPlane` behaviour), e.g. for
cutaway views.

## See also

[`render_options`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md)

## Examples

``` r
# World space (default): keep the half of the scene with x <= 0.
# The cut stays at x = 0, no matter where the camera is placed.
clip_plane(normal = c(-1, 0, 0), offset = 0)
#> $normal
#> [1] -1  0  0
#> 
#> $offset
#> [1] 0
#> 
#> $space
#> [1] "world"
#> 

# Keep only the part with z >= -0.5 (remove everything below z = -0.5):
clip_plane(normal = c(0, 0, 1), offset = 0.5)
#> $normal
#> [1] 0 0 1
#> 
#> $offset
#> [1] 0.5
#> 
#> $space
#> [1] "world"
#> 

# Eye space: remove everything closer than 2 units to the camera
# (a camera-attached cutaway).
clip_plane(normal = c(0, 0, -1), offset = -2, space = "eye")
#> $normal
#> [1]  0  0 -1
#> 
#> $offset
#> [1] -2
#> 
#> $space
#> [1] "eye"
#> 

# A world-space cut through a cuboid; the cut stays at x = 0 for any camera
cuboid <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
cut_opts <- render_options(clip_planes = list(
    clip_plane(normal = c(-1, 0, 0), offset = 0)))

mesh_pixels <- function(img) sum(image_to_array(img)[, , 1] < 1)
cam <- camera(eye = c(3, 3, 3), center = c(0, 0, 0))

full <- render_mesh(cuboid$vertices, cuboid$triangles, camera = cam)
cut <- render_mesh(cuboid$vertices, cuboid$triangles, camera = cam,
    options = cut_opts)

mesh_pixels(cut) < mesh_pixels(full)   # TRUE: half of the cuboid is gone
#> [1] TRUE
```
