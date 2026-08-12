# Write a mesh to an STL file

Writes a scimesh mesh descriptor to an ASCII or binary STL file.

## Usage

``` r
write_stl(mesh, path, format = c("binary", "ascii"))
```

## Arguments

- mesh:

  A mesh descriptor list.

- path:

  Path to the output STL file.

- format:

  `"binary"` (default) or `"ascii"`.

## Value

invisible NULL, called for side effects of writing the file.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
tmp_file <- tempfile(fileext = ".stl")
write_stl(mesh, tmp_file, format = "binary")
```
