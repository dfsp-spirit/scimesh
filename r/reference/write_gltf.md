# Write a scene or mesh list to a glTF file

Exports a scene (or a list of meshes) to the glTF 2.0 format, either as
a JSON document with an external binary buffer (`.gltf` + `.bin`) or as
a single self-contained binary file (`.glb`). The resulting file can be
viewed in any glTF-capable viewer (e.g. a browser using three.js) and
includes per-mesh placement transforms, vertex colors, and optionally a
camera.

## Usage

``` r
write_gltf(meshes, path, camera = NULL, format = c("gltf", "glb"))
```

## Arguments

- meshes:

  Either a `scimesh_scene` object (see
  [`scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/scene.md)),
  a list of mesh descriptors, or a list of scene nodes — the same inputs
  accepted by
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

- path:

  Output file path. For `format = "gltf"` the binary buffer is written
  next to it as `<stem>.bin`.

- camera:

  Optional camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).
  When provided, a glTF perspective camera node is included.

- format:

  Output format: `"gltf"` (default, JSON + `.bin`) or `"glb"` (single
  binary file).

## Value

Invisibly `NULL`.

## Details

Renderer-specific settings (shading mode, fog, SSAO, ...) are not part
of the glTF standard and are not exported. Per-face colors are exported
by splitting vertices (each triangle gets its own vertices), which
increases geometry roughly 3x.

## Examples

``` r
cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
tr <- diag(1, 4); tr[1, 4] <- 2
sc <- scene(list(cube, list(mesh = cube, transform = tr, name = "second")))
out <- tempfile(fileext = ".glb")
write_gltf(sc, out, format = "glb")
```
