# Create a scene descriptor

Bundles a list of meshes together with their placement transforms, a
camera, and render options into a single scene object that can be passed
to
[`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)
or
[`write_gltf()`](https://dfsp-spirit.github.io/scimesh/r/reference/write_gltf.md).

## Usage

``` r
scene(meshes, camera = NULL, options = NULL, transforms = NULL, names = NULL)
```

## Arguments

- meshes:

  A list of mesh descriptors (scimesh or rgl format, see
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)),
  or a list of already-built scene nodes
  (`list(mesh = ..., transform = ..., name = ...)`).

- camera:

  A camera list from
  [`camera()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera.md)
  or
  [`camera_auto()`](https://dfsp-spirit.github.io/scimesh/r/reference/camera_auto.md).
  Optional here; when `NULL` it must be supplied when calling
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md).

- options:

  A render options list from
  [`render_options()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_options.md).
  Optional here; when `NULL`,
  [`render_scene()`](https://dfsp-spirit.github.io/scimesh/r/reference/render_scene.md)
  uses its default options.

- transforms:

  Optional list of 4x4 numeric matrices, one per mesh, overriding any
  embedded `transform` in the nodes. May contain `NULL` entries to mean
  identity.

- names:

  Optional character vector, one per mesh, overriding any embedded
  `name`.

## Value

A scene descriptor list with S3 class `"scimesh_scene"`, with components
`meshes` (list of scene nodes), `camera`, and `options`.

## Details

Each mesh is wrapped into a scene node
`list(mesh = ..., transform = ..., name = ...)`. The optional
`transform` is a 4x4 numeric matrix (column-major, GLM style — the same
convention accepted by
[`transform_mesh()`](https://dfsp-spirit.github.io/scimesh/r/reference/transform_mesh.md))
that places the mesh in world space at render/export time without
modifying the mesh itself. The optional `name` is used by exporters such
as glTF for node names.

## Examples

``` r
cube1 <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
cube2 <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(0, 0, 1, 1))
# place the second cube 2 units along +X
tr <- diag(1, 4); tr[1, 4] <- 2
sc <- scene(list(cube1, list(mesh = cube2, transform = tr, name = "blue")),
            camera = camera_auto(list(cube1, cube2), direction = c(1, 1, 1)))
img <- render_scene(sc)
```
