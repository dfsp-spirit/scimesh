# Plan: 2×2 Supersampling Anti-Aliasing (SSAA) in C++

## Approach

Ordered-grid supersampling: render internally at
`(width * aa_samples) × (height * aa_samples)`, then downsample by
averaging each `aa_samples × aa_samples` block into one output pixel.

For `aa_samples = 2`: renders at 4× the pixel count (2W × 2H), averages 2×2
blocks → 1 output pixel.  No sub-pixel jitter needed — higher resolution alone
provides the anti-aliasing.

This is the simplest and most portable SSAA method.  It works across all C++
bindings (R, future Python, etc.) and requires no changes to the rasterizer's
sampling logic.

## Files to modify

### 1. `src/core/render_options.h` — Add `aa_samples` field

```cpp
struct RenderOptions {
    // ... existing fields ...
    int aa_samples = 1;   // 1 = no AA, 2 = 2×2 SSAA, 4 = 4×4 SSAA
};
```

### 2. `src/core/image.h` + `image.cpp` — Add `downsample_box()` method

```cpp
// image.h — new method
Image downsample_box(int factor) const;
```

```cpp
// image.cpp — implementation
Image Image::downsample_box(int factor) const {
    if (factor <= 1) return *this;
    int out_w = width / factor;
    int out_h = height / factor;
    Image result(out_w, out_h);

    int n = factor * factor;  // samples per output pixel
    for (int y = 0; y < out_h; y++) {
        for (int x = 0; x < out_w; x++) {
            int r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;
            for (int dy = 0; dy < factor; dy++) {
                for (int dx = 0; dx < factor; dx++) {
                    int sx = x * factor + dx;
                    int sy = y * factor + dy;
                    int idx = (sy * width + sx) * 4;
                    r_sum += pixels[idx];
                    g_sum += pixels[idx + 1];
                    b_sum += pixels[idx + 2];
                    a_sum += pixels[idx + 3];
                }
            }
            result.set_pixel(x, y,
                static_cast<uint8_t>(r_sum / n),
                static_cast<uint8_t>(g_sum / n),
                static_cast<uint8_t>(b_sum / n),
                static_cast<uint8_t>(a_sum / n));
        }
    }
    return result;
}
```

### 3. `src/core/renderer.cpp` — Modify `render_pipeline()` signature and `render_mesh()`/`render_scene()`

**`render_pipeline()` signature**: accept `RenderOptions` directly so it can
read `aa_samples`.

**`render_mesh()` and `render_scene()`**: compute internal resolution,
render, then downsample.

```cpp
Image Renderer::render_mesh(const Mesh &mesh, const Camera &camera,
                             const RenderOptions &options) {
    int int_w = options.width * options.aa_samples;
    int int_h = options.height * options.aa_samples;
    Image internal(int_w, int_h);
    render_pipeline({&mesh}, camera, options, internal);
    return internal.downsample_box(options.aa_samples);
}
```

Same pattern for `render_scene()`.

**`render_pipeline()`**: the rasterizer and image are already created
externally (in `render_mesh`/`render_scene`) — the internal image size
accounts for supersampling.  The rasterizer's `ndc_to_screen()` already
maps NDC [-1,1] to screen [0,width-1] × [0,height-1], so with a 2×
wider/taller image, the triangle edges are sampled at higher resolution
automatically.

```cpp
void Renderer::render_pipeline(
    const std::vector<const Mesh *> &meshes,
    const Camera &camera,
    const RenderOptions &options,
    Image &output)  // already sized for ssaa
{
    // Pixel sample positions: for ssaa, use (x + 0.5, y + 0.5) as always.
    // The rasterizer works at the internal (higher) resolution
    // since output.width/height are already upscaled.
    // ... rest of pipeline unchanged ...
}
```

Actually, wait — `render_pipeline()` currently creates the `Rasterizer` with
the output image dimensions internally:

```cpp
Rasterizer rasterizer(output.width, output.height);
```

And `ndc_to_screen()` uses `output.width` and `output.height`.  If the
output image passed in is already at the upscaled resolution, the rasterizer
automatically renders at that resolution.  **No rasterizer changes needed.**

### 4. `src/core/renderer.h` — Update declarations

```cpp
// Change render_pipeline signature to accept RenderOptions
void render_pipeline(
    const std::vector<const Mesh *> &meshes,
    const Camera &camera,
    const RenderOptions &options,
    Image &output);
```

### 5. `src/rcpp_bindings.cpp` — Parse `aa_samples` from R

In `build_options_from_r()`, after the wireframe line:

```cpp
if (opt_desc.containsElementNamed("aa_samples") && !Rf_isNull(opt_desc["aa_samples"]))
    opts.aa_samples = as<int>(opt_desc["aa_samples"]);
```

### 6. `R/render.R` — Add `aa_samples` to `render_options()`

```r
render_options <- function(width = 800L, height = 600L,
                           shading = c("smooth", "flat"),
                           backface_culling = TRUE,
                           background_color = c(0, 0, 0, 0),
                           default_color = c(0.7, 0.7, 0.7, 1),
                           invert_normals = FALSE,
                           wireframe = FALSE,
                           aa_samples = 1L) {
    ...
    list(
        ...
        wireframe = isTRUE(wireframe),
        aa_samples = as.integer(aa_samples)
    )
}
```

### 7. `R/RcppExports.R` — Regenerate (via `Rcpp::compileAttributes()`)

## Usage from R

```r
# Default: no AA (aa_samples = 1)
img <- vis.subject.morph.native(..., width = 800, height = 600)

# 2×2 SSAA
img <- vis.subject.morph.native(..., width = 800, height = 600,
    aa_samples = 2L)
```

Default stays `1` for speed; users opt into AA when exporting for publication.

## Performance

| aa_samples | Internal render | Memory |
|-----------|----------------|--------|
| 1 (off)   | W × H          | W·H·4 bytes |
| 2         | 2W × 2H        | 4× memory |
| 4         | 4W × 4H        | 16× memory |

For an 800×600 image with 2× SSAA: internal 1600×1200 (~7.7 MB), output 800×600.

## Files requiring recompilation

All `.cpp` files that include `render_options.h`, `image.h`, `renderer.h`,
`rcpp_bindings.cpp`.

## Test plan

Add a test that renders a diagonal edge with and without AA, verifies that
AA output contains intermediate (blended) pixel values along edges:
```r
test_that("ssaa produces blended edge pixels", {
    # Render a sharp triangle edge
    img_noaa <- render_mesh(..., options = render_options(aa_samples = 1L))
    img_aa   <- render_mesh(..., options = render_options(aa_samples = 2L))
    # AA image should have intermediate alpha values along edges
    arr_noaa <- image_to_array(img_noaa)
    arr_aa   <- image_to_array(img_aa)
    n_gray <- sum(arr_aa[, , 4] > 0.01 & arr_aa[, , 4] < 0.99)
    n_binary <- sum(arr_noaa[, , 4] > 0.01 & arr_noaa[, , 4] < 0.99)
    expect_true(n_gray > n_binary)
})
```
