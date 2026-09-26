# Information about the font used for text labels

Reports which font file is used (and where it lives), its family name
and its vertical metrics. Useful to check that the bundled font was
found and to see what a custom `.ttf` file contains.

## Usage

``` r
font_info(font_file = NULL, size = 18)
```

## Arguments

- font_file:

  Path to a `.ttf` file, or `NULL` for the bundled font (see
  [`default_font()`](https://dfsp-spirit.github.io/scimesh/r/reference/default_font.md)).

- size:

  Text height in output pixels (default 18).

## Value

A list with components `family`, `path`, `size`, `ascent`, `descent` and
`line_gap`.

## See also

[`default_font`](https://dfsp-spirit.github.io/scimesh/r/reference/default_font.md),
[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)

## Examples

``` r
font_info()
#> $family
#> [1] "Inter"
#> 
#> $path
#> [1] "/home/runner/work/_temp/Library/scimesh/extdata/Inter-Regular.ttf"
#> 
#> $size
#> [1] 18
#> 
#> $ascent
#> [1] 14.41162
#> 
#> $descent
#> [1] 3.588378
#> 
#> $line_gap
#> [1] 0
#> 
```
