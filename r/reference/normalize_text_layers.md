# Normalize the `texts` argument of scene()

Accepts `NULL`, a single text layer, or a list of text layers and
returns a (possibly empty) list of text layers. Layers may also be
wrapped into a scene node (`list(text = <layer>, transform = ...)`),
which allows moving a group of world-space labels with one transform.

## Usage

``` r
normalize_text_layers(texts)
```

## Arguments

- texts:

  `NULL`, a text layer, or a list of text layers / nodes.

## Value

A list of text layers (or text layer nodes).
