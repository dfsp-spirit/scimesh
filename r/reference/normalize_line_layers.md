# Normalize the `lines` argument of scene()

Accepts `NULL`, a single line layer, or a list of line layers and
returns a (possibly empty) list of line layers. Layers may also be
wrapped into a scene node (`list(lines = <layer>, transform = ...)`).

## Usage

``` r
normalize_line_layers(lines)
```

## Arguments

- lines:

  `NULL`, a line layer, or a list of line layers / nodes.

## Value

A list of line layers (or line layer nodes).
