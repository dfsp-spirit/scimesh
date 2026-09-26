# Path of the font used for text labels

Text labels are drawn with a TrueType font that ships with the package
(`inst/extdata/Inter-Regular.ttf`, SIL Open Font License 1.1), so labels
look the same everywhere and no system font is required. This function
returns the path of the font that will be used by default.

## Usage

``` r
default_font()
```

## Value

A character scalar: the path to an existing font file.

## Details

It can be overridden without touching any code by setting the
`SCIMESH_FONT` environment variable to the path of another `.ttf` file,
or per call by passing `font_file` to
[`text_layer()`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md),
[`text_extent()`](https://dfsp-spirit.github.io/scimesh/r/reference/text_extent.md)
and friends.

## See also

[`font_info`](https://dfsp-spirit.github.io/scimesh/r/reference/font_info.md),
[`text_layer`](https://dfsp-spirit.github.io/scimesh/r/reference/text_layer.md)

## Examples

``` r
default_font()
#> [1] "/home/runner/work/_temp/Library/scimesh/extdata/Inter-Regular.ttf"
```
