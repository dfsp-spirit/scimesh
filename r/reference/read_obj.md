# Read a Wavefront OBJ file

Reads a Wavefront OBJ file (with optional UV coordinates and normals)
and returns a scimesh mesh descriptor list with `vertices`, `triangles`,
and optionally `uv` and `normals`.

## Usage

``` r
read_obj(path)
```

## Arguments

- path:

  Path to the OBJ file.

## Value

A mesh descriptor list.

## Examples

``` r
if (FALSE) { # \dontrun{
mesh <- read_obj("model.obj")
nrow(mesh$vertices)
} # }
```
