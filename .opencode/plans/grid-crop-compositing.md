# Plan: Tight grid compositing with per-row/column cropping

## Problem

Currently `vis.subject.morph.native()` calls `.uniform_crop()` which uses a **global**
max height/width across ALL rendered images.  For a t4 layout (2×2) with laterals
(wide, short) and medials (narrow, tall), every tile gets padded to `max(w_lat, w_med) ×
max(h_lat, h_med)` — wasting huge horizontal space on medials and vertical space on
laterals.

## Solution

Replace global-max cropp with **per-row height + per-column width** padding.
After individual trimming each image keeps only the padding it actually needs.

### Approach: True grid (per-row heights, per-column widths)

Rows have independent heights; columns have independent widths.  This gives:
- Rows only as tall as their tallest member (saves vertical space)
- Columns only as wide as their widest member (saves horizontal space)
- Clean column alignment across rows (better than fsbrain's row-major approach)

---

## File changes

### 1. `R/compose.R` — Add `.grid_crop()` and modify `compose_layout()`

#### a) New function `.grid_crop()`

Insert after `.uniform_crop()` (after line 48):

```r
# Internal: crop each image individually and pad to per-row height and
# per-column width.  This gives a tight grid where no cell has more
# padding than necessary, saving white space for publication figures.
#
# Returns padded arrays and the row/column dimension vectors used.
.grid_crop <- function(arrays, nrow, ncol, background = c(0, 0, 0, 0)) {
    n <- length(arrays)
    if (n == 0L) stop("arrays must be non-empty")

    cropped <- lapply(arrays, .crop_content)
    cropped_h <- vapply(cropped, function(x) dim(x$arr)[1L], 0L)
    cropped_w <- vapply(cropped, function(x) dim(x$arr)[2L], 0L)

    row_heights <- integer(nrow)
    for (r in seq_len(nrow)) {
        idx <- ((r - 1L) * ncol + 1L):min(r * ncol, n)
        row_heights[r] <- max(cropped_h[idx])
    }

    col_widths <- integer(ncol)
    for (c in seq_len(ncol)) {
        idx <- seq(c, n, by = ncol)
        col_widths[c] <- max(cropped_w[idx])
    }

    padded <- lapply(seq_len(n), function(i) {
        r <- (i - 1L) %/% ncol + 1L
        c <- (i - 1L) %% ncol + 1L
        .pad_to_size(cropped[[i]]$arr, row_heights[r], col_widths[c],
                     background)
    })

    list(arrays = padded, row_heights = row_heights,
         col_widths = col_widths)
}
```

#### b) Modify `compose_layout()` — add `crop` parameter, unified grid building

**Signature change** (line 125):

Change from:
```r
compose_layout <- function(images, nrow = NULL, ncol = NULL,
                           colorbar = NULL,
                           colorbar_height = 80L,
                           colorbar_width = 80L,
                           background = c(0, 0, 0, 0)) {
```

To:
```r
compose_layout <- function(images, nrow = NULL, ncol = NULL,
                           colorbar = NULL,
                           colorbar_height = 80L,
                           colorbar_width = 80L,
                           background = c(0, 0, 0, 0),
                           crop = FALSE) {
```

**Early return for single image** (lines 136-138):

Change from:
```r
    if (length(arrays) == 1L && is.null(colorbar)) {
        return(images[[1L]])
    }
```

To (keep early return only for non-crop case):
```r
    if (length(arrays) == 1L && is.null(colorbar) && !isTRUE(crop)) {
        return(images[[1L]])
    }
```

**Dimension validation + sizing section** (lines 140-159):

Replace the entire section from the early return through the dimension
check at line 159 with:

```r
    # ---- Determine layout ----
    if (is.null(nrow) && is.null(ncol)) {
        ncol <- ceiling(sqrt(length(arrays)))
        nrow <- ceiling(length(arrays) / ncol)
    } else if (is.null(nrow)) {
        nrow <- ceiling(length(arrays) / ncol)
    } else if (is.null(ncol)) {
        ncol <- ceiling(length(arrays) / nrow)
    }

    # ---- Compute per-row heights and per-column widths ----
    if (isTRUE(crop)) {
        gc <- .grid_crop(arrays, nrow, ncol, background = background)
        arrays <- gc$arrays
        row_heights <- gc$row_heights
        col_widths  <- gc$col_widths
    } else {
        cell_h <- dim(arrays[[1L]])[1L]
        cell_w <- dim(arrays[[1L]])[2L]
        for (i in seq_along(arrays)) {
            if (dim(arrays[[i]])[1L] != cell_h ||
                dim(arrays[[i]])[2L] != cell_w) {
                stop("All images must have the same dimensions for composition. ",
                     "Image ", i, " has dimensions ",
                     paste(dim(arrays[[i]]), collapse = "x"),
                     " but expected ", cell_h, "x", cell_w)
            }
        }
        row_heights <- rep(cell_h, nrow)
        col_widths  <- rep(cell_w, ncol)
    }

    # ---- Single image (cropped), no colorbar: return cropped result ----
    if (length(arrays) == 1L && is.null(colorbar)) {
        arr <- arrays[[1L]]
        out_h <- dim(arr)[1L]
        out_w <- dim(arr)[2L]
        pixels <- as.raw(round(aperm(arr, c(3L, 2L, 1L)) * 255))
        return(list(width = out_w, height = out_h, pixels = pixels))
    }
```

**Grid building section** (lines 161-181):

Replace:
```r
    full_w <- ncol * cell_w
    full_h <- nrow * cell_h

    composite <- array(c(bg_r, bg_g, bg_b, bg_a),
                       dim = c(full_h, full_w, 4L))
    for (r in seq_len(nrow)) {
        for (c_idx in seq_len(ncol)) {
            i <- (r - 1L) * ncol + c_idx
            if (i > length(arrays)) break
            row_start <- (r - 1L) * cell_h + 1L
            row_end <- r * cell_h
            col_start <- (c_idx - 1L) * cell_w + 1L
            col_end <- c_idx * cell_w
            composite[row_start:row_end, col_start:col_end, ] <-
                arrays[[i]]
        }
    }
```

With:
```r
    full_w <- sum(col_widths)
    full_h <- sum(row_heights)

    composite <- array(c(bg_r, bg_g, bg_b, bg_a),
                       dim = c(full_h, full_w, 4L))
    for (r in seq_len(nrow)) {
        row_start <- if (r == 1L) 1L else cumsum(row_heights)[r - 1L] + 1L
        row_end   <- cumsum(row_heights)[r]
        for (c_idx in seq_len(ncol)) {
            i <- (r - 1L) * ncol + c_idx
            if (i > length(arrays)) break
            col_start <- if (c_idx == 1L) 1L
                         else cumsum(col_widths)[c_idx - 1L] + 1L
            col_end   <- cumsum(col_widths)[c_idx]
            composite[row_start:row_end, col_start:col_end, ] <-
                arrays[[i]]
        }
    }
```

**Colorb ar section** (lines 183-231): No changes needed — it already
uses `full_w` and `full_h` which are now computed correctly from variable
cell sizes.

#### c) Modify `stack_vertical()` and `stack_horizontal()` — pass through `crop`

`stack_vertical()` — add `crop` param:
```
stack_vertical <- function(..., colorbar = NULL,
                           colorbar_height = 80L,
                           colorbar_width = 80L,
                           background = c(0, 0, 0, 0),
                           crop = FALSE) {
    ...
    compose_layout(images, ncol = 1L, colorbar = colorbar,
                   colorbar_height = colorbar_height,
                   colorbar_width = colorbar_width,
                   background = background,
                   crop = crop)
}
```

`stack_horizontal()` — add `crop` param:
```
stack_horizontal <- function(..., colorbar = NULL,
                             colorbar_height = 80L,
                             colorbar_width = 80L,
                             background = c(0, 0, 0, 0),
                             crop = FALSE) {
    ...
    compose_layout(images, nrow = 1L, colorbar = colorbar,
                   colorbar_height = colorbar_height,
                   colorbar_width = colorbar_width,
                   background = background,
                   crop = crop)
}
```

---

### 2. `R/vis.R` — Remove `.uniform_crop()`, pass `crop=TRUE`

**Remove lines 409-417** (the `.uniform_crop()` step):
```r
    # ---- Auto-crop: trim transparent borders uniformly ----
    arrays <- lapply(images, image_to_array)
    uc <- .uniform_crop(arrays, background = background)
    # Convert cropped arrays back to image-list format
    images <- lapply(uc$arrays, function(arr) {
        list(width  = dim(arr)[2L],
             height = dim(arr)[1L],
             pixels = as.raw(round(aperm(arr, c(3L, 2L, 1L)) * 255)))
    })
```

**Change `compose_layout()` call** (line 443):
From:
```r
    compose_layout(images, colorbar = cbar,
                   colorbar_height = colorbar_height,
                   background = background)
```
To:
```r
    compose_layout(images, colorbar = cbar,
                   colorbar_height = colorbar_height,
                   background = background,
                   crop = TRUE)
```

---

### 3. `tests/testthat/test-compose.R` — Update tests

#### a) Keep dimension validation test (crop=FALSE, still errors)

The existing test at line 57-60 checks that different-sized images cause an
error with default `crop=FALSE`.  This should still pass.  No change needed.

#### b) Add new tests for crop=TRUE

```r
test_that("compose_layout with crop=TRUE handles different-sized images", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))

    img1 <- render_mesh(verts, tris, camera = cam,
                        options = render_options(width = 20, height = 10))
    img2 <- render_mesh(verts, tris, camera = cam,
                        options = render_options(width = 8, height = 16))

    composed <- compose_layout(list(img1, img2), crop = TRUE)
    expect_type(composed, "list")
    # Images should have been cropped and padded;
    # both should be in the output
    expect_true(composed$width >= 8 && composed$height >= 10)
})

test_that("compose_layout crop=TRUE with 2x2 grid is tighter than crop=FALSE", {
    skip_if_not_installed("png")
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 80, height = 60)

    images <- lapply(1:4, function(i) {
        render_mesh(verts, tris, camera = cam, options = opts)
    })

    no_crop <- compose_layout(images, nrow = 2, ncol = 2, crop = FALSE)
    with_crop <- compose_layout(images, nrow = 2, ncol = 2, crop = TRUE)
    # Cropped version should not be larger (usually smaller)
    expect_true(with_crop$width <= no_crop$width)
    expect_true(with_crop$height <= no_crop$height)
})

test_that("stack_horizontal with crop=TRUE", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 16, height = 16)

    img1 <- render_mesh(verts, tris, camera = cam, options = opts)
    img2 <- render_mesh(verts, tris, camera = cam, options = opts)

    composed <- stack_horizontal(img1, img2, crop = TRUE)
    expect_type(composed, "list")
    expect_equal(length(composed$pixels),
                 composed$width * composed$height * 4L)
})
```

---

### 4. `tests/testthat/test-vis.R` — Update dimension expectations

The t4 and t9 tests check exact output dimensions.  With crop=TRUE these
become variable.  Change to range checks:

#### t4 test (lines 13-14):
```r
    # expect_equal(img$width, 400L)   # 2x2 * 200
    # expect_equal(img$height, 300L)  # 2x2 * 150
    expect_true(img$width >= 100L && img$width <= 400L)
    expect_true(img$height >= 100L && img$height <= 300L)
```

#### Single view + colorbar test (lines 31-33):
```r
    # expect_equal(img$width, 200L)
    # expect_equal(img$height, 230L)  # 150 + 80 (colorbar)
    expect_true(img$width >= 50L && img$width <= 200L)
    expect_true(img$height >= 100L && img$height <= 230L)
```

#### lh-only test (lines 48-49):
```r
    # expect_equal(img$width, 200L)
    # expect_equal(img$height, 150L)
    expect_true(img$width >= 50L && img$width <= 200L)
    expect_true(img$height >= 50L && img$height <= 150L)
```

#### rh-only test (lines 64-65):
```r
    # expect_equal(img$width, 200L)
    # expect_equal(img$height, 150L)
    expect_true(img$width >= 50L && img$width <= 200L)
    expect_true(img$height >= 50L && img$height <= 150L)
```

#### t9 test (lines 134-135):
```r
    # expect_equal(img$width, 300L)
    # expect_equal(img$height, 225L)
    expect_true(img$width >= 100L && img$width <= 300L)
    expect_true(img$height >= 75L && img$height <= 225L)
```

---

### 5. Verify — Run example and tests

```bash
# Run the example
Rscript examples/R/vis_subject_morph_native/run.R

# Run tests
Rscript -e 'testthat::test_dir("tests/testthat/")'

# Check no regressions in C++ build
R CMD INSTALL . && Rscript -e 'devtools::test()'
```
