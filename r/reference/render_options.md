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
  threads = 0L,
  clip_planes = NULL,
  ssao_enabled = FALSE,
  ssao_radius = 16,
  ssao_intensity = 0.8,
  aa_samples = 1L
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

  Z-depth where fog begins (0 = near plane, 1 = far plane). Default 0.

- fog_end:

  Z-depth where fog is fully opaque. Default 1.

- fog_color:

  RGBA fog colour (0-1 scale). Defaults to `background_color`.

- threads:

  Number of render threads. 0 = auto-detect (use all cores), 1 =
  single-threaded (deterministic). Default 0. Requires OpenMP at compile
  time.

- clip_planes:

  A list of clip plane descriptors, each a list with `normal` (length-3
  vector) and `offset` (numeric). Points satisfying
  `dot(normal, position) + offset >= 0` are kept. Default `NULL` (no
  clipping).

- ssao_enabled:

  Enable screen-space ambient occlusion. Default `FALSE`.

- ssao_radius:

  Screen-space sample radius in pixels. Default 16.

- ssao_intensity:

  Occlusion strength (0-1). Default 0.8.

- aa_samples:

  Anti-aliasing supersampling factor. Renders internally at
  `width * aa_samples` x `height * aa_samples`, then downsamples to the
  requested size via box averaging. Default `1` (no AA), `2` for 2x2
  SSAA, `4` for 4x4.

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
```
