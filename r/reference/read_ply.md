# Read a Stanford PLY file

Reads a PLY file (ASCII or binary) with optional per-vertex colors and
returns a scimesh mesh descriptor list with `vertices`, `triangles`, and
optionally `colors`.

## Usage

``` r
read_ply(path)
```

## Arguments

- path:

  Path to the PLY file.

## Value

A mesh descriptor list.

## Examples

``` r
if (FALSE) { # \dontrun{
mesh <- read_ply("model.ply")
nrow(mesh$vertices)
} # }
```
