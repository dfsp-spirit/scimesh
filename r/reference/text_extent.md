# Measure text

Computes the size of the text box that
[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)
uses, which is needed to place labels relative to each other (for
example to right-align a caption, or to keep two labels from
overlapping). Multi-line labels are measured as a whole, using the same
line spacing as the renderer.

## Usage

``` r
text_extent(text, size = 18, font_file = NULL, line_spacing = 1.2)
```

## Arguments

- text:

  Character vector of labels (UTF-8; `"\n"` for line breaks).

- size:

  Text height in output pixels (default 18).

- font_file:

  Path to a `.ttf` file, or `NULL` for the bundled font (see
  [`default_font()`](https://dfsp-spirit.github.io/scimesh/r/reference/default_font.md)).

- line_spacing:

  Distance between lines, as a multiple of the glyph box height (default
  1.2).

## Value

A data frame with one row per input string and the columns `text`,
`width` (widest line), `height` (whole block), `ascent`, `descent` and
`lines`.

## See also

[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)

## Examples

``` r
text_extent("anterior", size = 20)
#>           text    width height   ascent  descent lines
#> width anterior 60.45198     20 16.01291 3.987087     1
text_extent(c("left hemisphere", "right hemisphere"), size = 16)
#>               text     width height   ascent  descent lines
#> 1  left hemisphere  97.45924     16 12.81033 3.189669     1
#> 2 right hemisphere 105.76271     16 12.81033 3.189669     1
```
