# Read a Wavefront OBJ file

Reads the geometry (vertices and triangles) of a Wavefront OBJ file and
returns a scimesh mesh descriptor list with `vertices` and `triangles`.
Normals and texture coordinates in the file are ignored: call
[`compute_vertex_normals()`](https://dfsp-spirit.github.io/scimesh/r/reference/compute_vertex_normals.md)
if you need normals, and assign `uv` yourself (see
[`render_mesh`](https://dfsp-spirit.github.io/scimesh/r/reference/render_mesh.md))
if you want to render the mesh with a texture.

## Usage

``` r
read_obj(path)
```

## Arguments

- path:

  Path to the OBJ file.

## Value

A mesh descriptor list with `vertices` and `triangles`.

## Examples

``` r
if (FALSE) { # \dontrun{
mesh <- read_obj("model.obj")
nrow(mesh$vertices)
} # }
```
