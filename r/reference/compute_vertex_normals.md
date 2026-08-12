# Compute per-vertex normals for a mesh

Computes smooth vertex normals by averaging face normals. Returns the
same mesh with a `normals` component (Nx3 numeric matrix). Useful for
imported meshes that lack pre-computed normals.

## Usage

``` r
compute_vertex_normals(mesh)
```

## Arguments

- mesh:

  A mesh descriptor list with `vertices` and `triangles`.

## Value

The mesh with a `normals` component added.

## Examples

``` r
mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
mesh <- compute_vertex_normals(mesh)
nrow(mesh$normals)
#> [1] 24
```
