# --- Category 1: render individual tiles, check pixel counts ----------------

# Helper: create a simple 2-triangle quad covering [-1,1]²
.helper_quad <- function(rgba = c(1, 0, 0, 1)) {
    verts <- cbind(c(-1, 1, -1, 1), c(-1, -1, 1, 1), c(0, 0, 0, 0))
    tris <- rbind(c(1L, 2L, 4L), c(1L, 4L, 3L))
    cols <- matrix(rgba, nrow = 4L, ncol = 4L, byrow = TRUE)
    list(vertices = verts, triangles = tris, colors = cols)
}

test_that("single quad renders non-empty pixels with correct color", {
    quad <- .helper_quad(c(1, 0, 0, 1))  # red
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 64, height = 64,
                           background_color = c(0, 0, 0, 0))

    img <- render_mesh(quad$vertices, quad$triangles,
                       colors = quad$colors, camera = cam, options = opts)
    arr <- image_to_array(img)

    n_colored <- sum(arr[, , 1L] > 0.5)
    n_opaque  <- sum(arr[, , 4L] > 0.5)
    expect_true(n_colored > 200L)   # quad fills ~half of 64×64
    expect_true(n_opaque > 200L)

    # Background corners should be transparent
    expect_equal(arr[1L, 1L, 4L], 0)
    expect_equal(arr[64L, 64L, 4L], 0)

    # Center should be red
    expect_gt(arr[32L, 32L, 1L], 0.5)  # red channel high
    expect_lt(arr[32L, 32L, 2L], 0.1)  # green channel low
})

test_that("four tiles of different colors save to separate PNGs", {
    skip_if_not_installed("png")

    quads <- list(
        red   = .helper_quad(c(1, 0, 0, 1)),
        green = .helper_quad(c(0, 1, 0, 1)),
        blue  = .helper_quad(c(0, 0, 1, 1)),
        white = .helper_quad(c(1, 1, 1, 1))
    )

    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 32, height = 32)

    outfiles <- c()
    for (nm in names(quads)) {
        q <- quads[[nm]]
        img <- render_mesh(q$vertices, q$triangles,
                           colors = q$colors, camera = cam, options = opts)
        fname <- file.path(tempdir(), paste0("scimesh_test_quad_", nm, ".png"))
        write_png(img, fname)
        expect_true(file.exists(fname))
        outfiles <- c(outfiles, fname)

        # Read back and verify
        arr <- png::readPNG(fname)
        center <- round(dim(arr)[1:2] / 2)
        cat(sprintf("  %s: center pixel RGB=%.2f %.2f %.2f\n",
                    nm, arr[center[1], center[2], 1],
                    arr[center[1], center[2], 2],
                    arr[center[1], center[2], 3]))
    }

    unlink(outfiles)
    succeed()
})

test_that("colorbar saves as separate PNG with ticks", {
    skip_if_not_installed("png")

    cbar <- colorbar_horizontal(viridis_colormap, n_colors = 64,
                                 width = 400, height = 60,
                                 ticks = c(0, 0.5, 1),
                                 tick_labels = c("min", "mid", "max"))

    fname <- file.path(tempdir(), "scimesh_test_colorbar.png")
    png::writePNG(cbar, fname)
    expect_true(file.exists(fname))

    arr <- png::readPNG(fname)
    expect_equal(dim(arr)[3L], 4L)

    # Colorbar should be mostly non-transparent (the color strip itself)
    opaque <- sum(arr[, , 4L] > 0.9)
    total <- dim(arr)[1L] * dim(arr)[2L]
    expect_true(opaque > total * 0.3)  # at least 30% opaque

    unlink(fname)
})

test_that("rendered image pixel count is consistent with simple geometry", {
    # A single triangle that nearly fills the viewport
    verts <- cbind(c(-1.5, 1.5, 0), c(-1.5, -1.5, 1.5), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cols <- cbind(c(0, 0, 0), c(1, 1, 1), c(0, 0, 0), c(1, 1, 1))

    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    for (res in c(32L, 64L, 128L)) {
        opts <- render_options(width = res, height = res)
        img <- render_mesh(verts, tris, colors = cols, camera = cam,
                           options = opts)
        arr <- image_to_array(img)
        n_col <- sum(arr[, , 1L] > 0.01 | arr[, , 2L] > 0.01 |
                     arr[, , 3L] > 0.01)
        # Most of the image should be colored
        expect_true(n_col > res * res * 0.15)
    }
})

test_that("FS mesh individual views have sufficient content", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    library(freesurferformats)
    surf <- read.fs.surface(file.path(sjd, sj, "surf", "lh.white"))
    morph <- read.fs.morph(file.path(sjd, sj, "surf", "lh.sulc"))

    label_file <- file.path(sjd, sj, "label", "lh.cortex.label")
    cort <- read.fs.label(label_file)
    mask <- rep(FALSE, nrow(surf$vertices))
    mask[cort] <- TRUE
    morph[!mask] <- NA_real_

    cols <- scimesh:::.apply_colormap(morph, list(colFn = viridis_colormap, n = 64))
    rgba <- scimesh:::.hex_to_rgba_matrix(cols)

    mesh <- list(vertices = surf$vertices, triangles = surf$faces,
                 colors = rgba)

    views_list <- list(
        lateral_lh = view_lateral_left(mesh),
        medial_lh  = view_medial_left(mesh),
        dorsal     = view_dorsal(mesh),
        ventral    = view_ventral(mesh)
    )

    outfiles <- c()
    for (vn in names(views_list)) {
        cam <- views_list[[vn]]
        opts <- render_options(width = 200, height = 150,
                               background_color = c(0, 0, 0, 0))
        img <- render_mesh(mesh$vertices, mesh$triangles,
                           colors = mesh$colors, camera = cam, options = opts)
        arr <- image_to_array(img)
        n_col <- sum(arr[, , 1L] > 0.01 | arr[, , 2L] > 0.01 |
                     arr[, , 3L] > 0.01)
        cat(sprintf("  %s: colored=%d (%.1f%%)\n", vn, n_col,
                    100 * n_col / length(arr[, , 1L])))
        expect_true(n_col > 200L)

        if (requireNamespace("png", quietly = TRUE)) {
            fname <- file.path(tempdir(),
                               paste0("scimesh_test_lh_", vn, ".png"))
            write_png(img, fname)
            expect_true(file.exists(fname))
            outfiles <- c(outfiles, fname)
        }
    }

    unlink(outfiles)
    succeed()
})


# --- Category 2: image merging with known color patterns ---------------------

.make_solid_image <- function(width, height, rgba) {
    # Produce C++-compatible row-major RGBA interleaved raw vector.
    # The layout expected by image_to_array is: pixel(y,x) = bytes at
    # (y*W+x)*4:(y*W+x)*4+3 for R,G,B,A.
    npix <- width * height
    pixels <- raw(npix * 4L)
    for (i in seq_len(npix)) {
        off <- (i - 1L) * 4L
        pixels[off + 1L] <- as.raw(round(rgba[1L] * 255))
        pixels[off + 2L] <- as.raw(round(rgba[2L] * 255))
        pixels[off + 3L] <- as.raw(round(rgba[3L] * 255))
        pixels[off + 4L] <- as.raw(round(rgba[4L] * 255))
    }
    list(width = width, height = height, pixels = pixels)
}

test_that("compose 2x2 grid of solid colors matches expectations", {
    # Create 4 images: red, green, blue, transparent
    red   <- .make_solid_image(10, 10, c(1, 0, 0, 1))
    green <- .make_solid_image(10, 10, c(0, 1, 0, 1))
    blue  <- .make_solid_image(10, 10, c(0, 0, 1, 1))
    trans <- .make_solid_image(10, 10, c(0, 0, 0, 0))

    composed <- compose_layout(list(red, green, blue, trans), nrow = 2, ncol = 2)
    arr <- image_to_array(composed)

    # Result should be 20x20
    expect_equal(dim(arr)[1L], 20L)
    expect_equal(dim(arr)[2L], 20L)

    # Top-left (1:10, 1:10) = red
    expect_equal(arr[5, 5, 1L], 1.0, tolerance = 0.01)
    expect_equal(arr[5, 5, 2L], 0.0, tolerance = 0.01)
    expect_equal(arr[5, 5, 3L], 0.0, tolerance = 0.01)
    expect_equal(arr[5, 5, 4L], 1.0, tolerance = 0.01)

    # Top-right (1:10, 11:20) = green
    expect_equal(arr[5, 15, 1L], 0.0, tolerance = 0.01)
    expect_equal(arr[5, 15, 2L], 1.0, tolerance = 0.01)
    expect_equal(arr[5, 15, 3L], 0.0, tolerance = 0.01)
    expect_equal(arr[5, 15, 4L], 1.0, tolerance = 0.01)

    # Bottom-left (11:20, 1:10) = blue
    expect_equal(arr[15, 5, 1L], 0.0, tolerance = 0.01)
    expect_equal(arr[15, 5, 2L], 0.0, tolerance = 0.01)
    expect_equal(arr[15, 5, 3L], 1.0, tolerance = 0.01)
    expect_equal(arr[15, 5, 4L], 1.0, tolerance = 0.01)

    # Bottom-right (11:20, 11:20) = transparent
    expect_equal(arr[15, 15, 4L], 0.0, tolerance = 0.01)
})

test_that("compose 1x4 horizontal strip matches expectations", {
    r <- .make_solid_image(5, 5, c(1, 0, 0, 1))
    g <- .make_solid_image(5, 5, c(0, 1, 0, 1))
    b <- .make_solid_image(5, 5, c(0, 0, 1, 1))
    a <- .make_solid_image(5, 5, c(0, 0, 0, 0.5))

    composed <- stack_horizontal(r, g, b, a)
    arr <- image_to_array(composed)

    expect_equal(dim(arr)[2L], 20L)
    expect_equal(dim(arr)[1L], 5L)

    # Column 3 (red part) = red
    expect_gt(arr[3, 3, 1L], 0.9)
    # Column 8 (green part) = green
    expect_gt(arr[3, 8, 2L], 0.9)
    # Column 13 (blue part) = blue
    expect_gt(arr[3, 13, 3L], 0.9)
    # Column 18 (alpha part) = half alpha
    expect_equal(arr[3, 18, 4L], 0.5, tolerance = 0.01)
})

test_that("compose 4x1 vertical strip matches expectations", {
    r <- .make_solid_image(5, 5, c(1, 0, 0, 1))
    g <- .make_solid_image(5, 5, c(0, 1, 0, 1))
    b <- .make_solid_image(5, 5, c(0, 0, 1, 1))
    t <- .make_solid_image(5, 5, c(0, 0, 0, 0))

    composed <- stack_vertical(r, g, b, t)
    arr <- image_to_array(composed)

    expect_equal(dim(arr)[1L], 20L)
    expect_equal(dim(arr)[2L], 5L)

    # Row 3 (red) = red
    expect_gt(arr[3, 3, 1L], 0.9)
    # Row 8 (green) = green
    expect_gt(arr[8, 3, 2L], 0.9)
    # Row 13 (blue) = blue
    expect_gt(arr[13, 3, 3L], 0.9)
    # Row 18 (transparent) = clear
    expect_equal(arr[18, 3, 4L], 0.0, tolerance = 0.01)
})


# --- Category 3: PNG alpha export round-trip --------------------------------

test_that("half-blue half-transparent image round-trips through PNG", {
    skip_if_not_installed("png")

    w <- 16L; h <- 8L

    # Build (H,W,4) array: left half blue opaque, right half transparent
    arr <- array(0, dim = c(h, w, 4L))
    arr[, 1L:(w / 2L), 1L] <- 0.0   # R
    arr[, 1L:(w / 2L), 2L] <- 0.0   # G
    arr[, 1L:(w / 2L), 3L] <- 1.0   # B
    arr[, 1L:(w / 2L), 4L] <- 1.0   # A (opaque blue left)
    arr[, (w / 2L + 1L):w, 4L] <- 0.0  # A transparent right, RGB don''t matter

    # Convert to C++-compatible raw pixels
    pixels_cpp <- as.raw(round(aperm(arr, c(3L, 2L, 1L)) * 255))

    # Write via write_png (which calls image_to_array internally)
    img <- list(width = w, height = h, pixels = pixels_cpp)
    fname <- file.path(tempdir(), "scimesh_test_alpha_roundtrip.png")
    write_png(img, fname)
    expect_true(file.exists(fname))

    # Read back
    arr2 <- png::readPNG(fname)
    expect_equal(dim(arr2), c(h, w, 4L))

    # Left half: blue opaque
    expect_equal(arr2[4L, 2L, 3L], 1.0, tolerance = 0.01)  # blue channel
    expect_equal(arr2[4L, 2L, 4L], 1.0, tolerance = 0.01)  # alpha channel

    # Right half: transparent
    expect_equal(arr2[4L, 14L, 4L], 0.0, tolerance = 0.01)  # alpha channel
    expect_equal(arr2[1L, w, 4L], 0.0, tolerance = 0.01)

    unlink(fname)
})

test_that("round-trip through compose_layout preserves pixel data", {
    # Create a 4x4 red image using C++-compatible raw pixel format
    w <- 4L; h <- 4L
    img_in <- .make_solid_image(w, h, c(1, 0, 0, 1))

    # Convert to array
    arr_in <- image_to_array(img_in)
    expect_equal(dim(arr_in), c(h, w, 4L))
    expect_true(all(arr_in[, , 1L] == 1.0))
    expect_true(all(arr_in[, , 4L] == 1.0))

    # Compose with itself (1 image, no colorbar)
    composed <- compose_layout(list(img_in), nrow = 1, ncol = 1)
    arr_out <- image_to_array(composed)

    expect_equal(dim(arr_out), c(h, w, 4L))
    expect_equal(arr_out[, , 1L], arr_in[, , 1L], tolerance = 0.01)
    expect_equal(arr_out[, , 4L], arr_in[, , 4L], tolerance = 0.01)
})

test_that("ensure_rgba_array pads 3-channel to 4-channel correctly", {
    arr3 <- array(0.5, dim = c(8, 8, 3))
    arr4 <- ensure_rgba_array(arr3)
    expect_equal(dim(arr4), c(8L, 8L, 4L))
    expect_equal(arr4[, , 1L], arr3[, , 1L])
    expect_equal(arr4[, , 2L], arr3[, , 2L])
    expect_equal(arr4[, , 3L], arr3[, , 3L])
    expect_true(all(arr4[, , 4L] == 1.0))
})

test_that("image_to_array produces (H,W,4) from render output", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 16, height = 12,
                           background_color = c(0, 0, 0, 0))

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    arr <- image_to_array(img)

    expect_equal(dim(arr)[1L], 12L)
    expect_equal(dim(arr)[2L], 16L)
    expect_equal(dim(arr)[3L], 4L)
    expect_true(all(arr >= 0 & arr <= 1))
})

test_that("image_to_array correctly separates channels from raw bytes", {
    # Build a known raw pixel buffer: 2x2 image where every pixel has a
    # unique RGBA to catch de-interleaving errors.
    w <- 2L; h <- 2L
    # Pixel(0,0)=(10,20,30,40), (0,1)=(50,60,70,80),
    # Pixel(1,0)=(90,100,110,120), (1,1)=(130,140,150,160)
    raw_vals <- c(10,20,30,40,  50,60,70,80,
                  90,100,110,120, 130,140,150,160)
    img <- list(width = w, height = h, pixels = as.raw(raw_vals))

    arr <- image_to_array(img)
    expect_equal(dim(arr), c(2L, 2L, 4L))

    # Check pixel by pixel
    expect_equal(arr[1,1,], c(10,20,30,40)/255)
    expect_equal(arr[1,2,], c(50,60,70,80)/255)
    expect_equal(arr[2,1,], c(90,100,110,120)/255)
    expect_equal(arr[2,2,], c(130,140,150,160)/255)
})
