# Create render options

Create render options

## Usage

``` r
render_options(
  width = 800L,
  height = 600L,
  shading = c("smooth", "flat"),
  backface_culling = TRUE,
  background_color = c(1, 1, 1, 1),
  default_color = c(0.7, 0.7, 0.7, 1),
  invert_normals = FALSE,
  wireframe = FALSE,
  wireframe_color = c(0, 0, 0, 1),
  projection = c("perspective", "orthographic"),
  specular_color = c(0, 0, 0, 0),
  shininess = 0,
  lights = NULL,
  ambient = 0.3,
  contrast = 1,
  fog_enabled = FALSE,
  fog_start = 0,
  fog_end = 1,
  fog_color = c(0, 0, 0, 0),
  fog_space = c("world", "ndc"),
  threads = 0L,
  clip_planes = NULL,
  ssao_enabled = FALSE,
  ssao_radius = 16,
  ssao_intensity = 0.8,
  aa_samples = NULL,
  near_plane = 0.1,
  far_plane = 10000
)
```

## Arguments

- width:

  Output image width in pixels.

- height:

  Output image height in pixels.

- shading:

  Shading mode: `"smooth"` or `"flat"`.

- backface_culling:

  Whether to cull back-facing triangles.

- background_color:

  Background RGBA color as numeric vector of length 4 (values 0-1).

- default_color:

  Default vertex color when no colors are provided.

- invert_normals:

  Whether to invert surface normals.

- wireframe:

  Whether to render in wireframe mode.

- wireframe_color:

  RGBA color for wireframe edges (0-1 scale). Default `c(0, 0, 0, 1)`
  (black).

- projection:

  Projection type: `"perspective"` (default) or `"orthographic"`.
  Orthographic gives a parallel projection (no perspective
  foreshortening), matching rgl's `view3d(fov=0)` convention.

- specular_color:

  Specular highlight color (0-1 scale). When `shininess > 0`, a
  Blinn-Phong highlight in this colour is added where the surface faces
  the camera. Default `c(0, 0, 0, 0)` (off).

- shininess:

  Specular exponent controlling highlight sharpness. Higher values
  produce a tighter spot. Typical values: 32 (soft plastic), 64 (shiny),
  128 (glass). Default `0` (off).

- lights:

  A list of light descriptors, each a list with `position` (length-3
  direction vector or point position), `color` (length-4 RGBA, 0-1
  scale), `intensity` (numeric, default 1), and `directional` (logical,
  default `TRUE`). When empty or `NULL`, a single headlight at
  `c(0, 0, 1)` is used (the original behaviour).

- ambient:

  Ambient light contribution (0-1). Default 0.3.

- contrast:

  Contrast adjustment applied after shading, before uint8_t conversion.
  Default 1.0 (no change). Values \> 1.0 produce darker darks and
  lighter highlights (S-curve). Formula:
  `(value - 0.5) * contrast + 0.5`, clamped to `[0, 1]`.

- fog_enabled:

  Enable depth cueing (fog). Default `FALSE`.

- fog_start:

  Distance where fog begins, i.e. where objects start fading toward
  `fog_color`. Default 0.

- fog_end:

  Distance where fog is fully opaque. Must be larger than `fog_start`.
  Default 1.

- fog_color:

  RGBA fog colour (0-1 scale). Defaults to `background_color`.

- fog_space:

  Character, either `"world"` (default) or `"ndc"`: the space (and
  therefore the unit) of `fog_start` and `fog_end`.

  `"world"`

  :   Distances in world units from the camera, measured along the
      viewing direction. `fog_start = 20` means "fog starts 20 world
      units in front of the camera". This is independent of
      `near_plane`/`far_plane` and of the projection type.

  `"ndc"`

  :   Normalized device depth, i.e. the raw depth-buffer values in
      `[-1, 1]`, where `-1` is the near plane, `0` the middle of the
      depth range and `+1` the far plane. This is the legacy behaviour;
      it depends on the near/far plane settings and is strongly
      non-linear for perspective cameras.

- threads:

  Number of render threads. 0 = auto-detect (use all cores), 1 =
  single-threaded (deterministic). Default 0. Requires OpenMP at compile
  time.

- clip_planes:

  A list of clip planes (see
  [`clip_plane`](https://dfsp-spirit.github.io/scimesh/r/reference/clip_plane.md)),
  or `NULL` (default) for no clipping. Each plane removes the geometry
  on its negative side, i.e. a point `p` is kept when
  `dot(normal, p) + offset >= 0`; several planes are combined with a
  logical AND. By default `p` is the world-space position, so the cut is
  fixed in the scene and does not move when the camera moves; use
  `clip_plane(..., space = "eye")` for a camera-relative cut. Note that
  clip planes only apply to mesh and triangle rendering;
  [`render_points()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_points.md)
  and
  [`render_spheres()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_spheres.md)
  ignore them.

- ssao_enabled:

  Enable screen-space ambient occlusion. Default `FALSE`.

- ssao_radius:

  Screen-space sample radius in pixels. Default 16.

- ssao_intensity:

  Occlusion strength (0-1). Default 0.8.

- aa_samples:

  Anti-aliasing supersampling factor. Renders internally at
  `width * aa_samples` x `height * aa_samples`, then downsamples to the
  requested size via box averaging. Use `1` (the default) for no AA, `2`
  for 2x2 SSAA, `4` for 4x4. If `NULL`, the global option
  `scimesh.aa_samples` is used (which defaults to `1`). Set that option
  once per session to enable AA for all render calls, e.g.
  `options(scimesh.aa_samples = 2)`. Note that AA increases render time
  and memory roughly with `aa_samples^2`, and that thin lines and points
  get smoother edges from it.

- near_plane:

  Distance of the near clipping plane (default 0.1). Geometry closer to
  the camera is clipped away. Also defines the depth range together with
  `far_plane`, which matters when `fog_space = "ndc"`.

- far_plane:

  Distance of the far clipping plane (default 10000). Must be larger
  than `near_plane`.

## Value

A render options list for use with
[`render_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md)
or
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

## Examples

``` r
# Default options
opts <- render_options()

# High-resolution with anti-aliasing and specular highlights
opts <- render_options(width = 1200, height = 900,
    aa_samples = 2L,
    specular_color = c(0.4, 0.4, 0.4, 1),
    shininess = 64)

# Wireframe with transparent background
opts <- render_options(wireframe = TRUE,
    wireframe_color = c(0, 0, 0, 1),
    background_color = c(0, 0, 0, 0))

# World-space clip plane: keep the half of the scene with x <= 0.
# The cut stays at x = 0, whatever the camera does.
opts <- render_options(clip_planes = list(
    clip_plane(normal = c(-1, 0, 0), offset = 0)))

# Eye-space clip plane: additionally remove everything closer than 2 units
# to the camera (camera-attached cutaway).
opts <- render_options(clip_planes = list(
    clip_plane(normal = c(-1, 0, 0), offset = 0),
    clip_plane(normal = c(0, 0, -1), offset = -2, space = "eye")))

# Fog in world units (default): fade from 20 to 60 units away from the
# camera
opts <- render_options(fog_enabled = TRUE, fog_start = 20, fog_end = 60,
    fog_color = c(0.9, 0.95, 1, 1))

# Legacy normalized-device-depth fog, for backwards compatibility
opts <- render_options(fog_enabled = TRUE, fog_space = "ndc",
    fog_start = 0.5, fog_end = 1)

# Enable 2x2 anti-aliasing for this session: affects all subsequent
# render calls that do not pass \code{aa_samples} explicitly.
old <- options(scimesh.aa_samples = 2L)
opts <- render_options()
opts$aa_samples
#> [1] 2
options(old)
```
