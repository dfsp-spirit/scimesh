# Convert rgl or scimesh mesh to canonical scimesh format

Internal helper that transparently accepts either an rgl-style mesh
(list with `vb`/`it`) or a scimesh mesh descriptor (list with
`vertices`/`triangles`) and returns the canonical scimesh format.

## Usage

``` r
as_scimesh_mesh(x)
```

## Arguments

- x:

  A mesh-like object (rgl tmesh3d or scimesh mesh descriptor).

## Value

A scimesh mesh descriptor list with `vertices` and `triangles`.
