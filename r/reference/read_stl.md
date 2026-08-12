# Read an STL file

Reads an ASCII or binary STL file and returns a scimesh mesh descriptor
list with `vertices`, `triangles`, and `normals`.

## Usage

``` r
read_stl(path)
```

## Arguments

- path:

  Path to the STL file.

## Value

A mesh descriptor list.

## Examples

``` r
if (FALSE) { # \dontrun{
mesh <- read_stl("model.stl")
nrow(mesh$vertices)
} # }
```
