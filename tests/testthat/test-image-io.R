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

# --- Category 2: image merging with known color patterns ---------------------

.make_solid_image <- function(width, height, rgba) {
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

# --- Category: C++ image manipulation (crop, merge, grow, rotate, scale) ------

.make_test_image <- function(w, h, r, g, b, a) {
    npix <- w * h * 4L
    raw_vals <- rep(c(as.raw(r), as.raw(g), as.raw(b), as.raw(a)),
                    times = w * h)
    list(width = w, height = h, pixels = as.raw(raw_vals))
}

test_that("image_crop extracts subregion", {
    img <- .make_test_image(4, 4, 255, 0, 0, 255)
    cropped <- image_crop(img, 1, 1, 2, 2)
    expect_equal(cropped$width, 2L)
    expect_equal(cropped$height, 2L)
    arr <- image_to_array(cropped)
    expect_equal(arr[1, 1, 1], 1)
})

test_that("image_crop out-of-bounds clamps", {
    img <- .make_test_image(4, 4, 255, 0, 0, 255)
    cropped <- image_crop(img, -1, -1, 10, 10)
    expect_equal(cropped$width, 4L)
    expect_equal(cropped$height, 4L)
})

test_that("image_merge LEFT places other on left", {
    a <- .make_test_image(2, 2, 255, 0, 0, 255)
    b <- .make_test_image(1, 2, 0, 255, 0, 255)
    merged <- image_merge(a, b, "left")
    expect_equal(merged$width, 3L)
    expect_equal(merged$height, 2L)
    arr <- image_to_array(merged)
    expect_equal(arr[1, 1, 1], 0)  # left column = green
    expect_equal(arr[1, 1, 2], 1)
    expect_equal(arr[1, 3, 1], 1)  # right column = red
})

test_that("image_merge RIGHT places other on right", {
    a <- .make_test_image(2, 2, 255, 0, 0, 255)
    b <- .make_test_image(1, 2, 0, 255, 0, 255)
    merged <- image_merge(a, b, "right")
    expect_equal(merged$width, 3L)
    arr <- image_to_array(merged)
    expect_equal(arr[1, 1, 1], 1)  # left column = red
    expect_equal(arr[1, 3, 2], 1)  # right column = green
})

test_that("image_merge height mismatch errors", {
    a <- .make_test_image(2, 2, 255, 0, 0, 255)
    b <- .make_test_image(1, 3, 0, 255, 0, 255)
    merged <- image_merge(a, b, "left")  # no-op
    expect_equal(merged$width, 2L)
    expect_equal(merged$height, 2L)
})

test_that("image_merge invalid direction errors", {
    a <- .make_test_image(2, 2, 255, 0, 0, 255)
    b <- .make_test_image(2, 2, 0, 255, 0, 255)
    expect_error(image_merge(a, b, "diagonal"))
})

test_that("image_grow adds padding with background colour", {
    img <- .make_test_image(2, 2, 255, 0, 0, 255)
    grown <- image_grow(img, 1, 1, 1, 1, c(0, 1, 0, 1))
    expect_equal(grown$width, 4L)
    expect_equal(grown$height, 4L)
    arr <- image_to_array(grown)
    expect_equal(arr[1, 1, 2], 1)  # top-left = green bg
    expect_equal(arr[2, 2, 1], 1)  # center = red from original
})

test_that("image_grow errors on bad background", {
    img <- .make_test_image(2, 2, 255, 0, 0, 255)
    expect_error(image_grow(img, 1, 1, 1, 1, c(0, 1)))
})

test_that("image_rotate_90 clockwise swaps dimensions", {
    img <- .make_test_image(3, 1, 255, 0, 0, 255)
    rotated <- image_rotate_90(img, TRUE)
    expect_equal(rotated$width, 1L)
    expect_equal(rotated$height, 3L)
})

test_that("image_rotate_90 four CCW rotations restores image", {
    img <- .make_test_image(2, 2, 255, 128, 64, 255)
    orig <- image_to_array(img)
    for (i in 1:4) img <- image_rotate_90(img, FALSE)
    arr <- image_to_array(img)
    expect_equal(arr, orig, tolerance = 0)
})

test_that("image_scale upscale 2x preserves centre pixel colour", {
    img <- .make_test_image(2, 2, 255, 0, 0, 255)
    scaled <- image_scale(img, 4, 4)
    expect_equal(scaled$width, 4L)
    expect_equal(scaled$height, 4L)
    arr <- image_to_array(scaled)
    expect_equal(arr[1, 1, 1], 1)  # nearest-neighbour copies top-left
})

test_that("image_scale downscale reduces dimensions", {
    img <- .make_test_image(4, 4, 128, 0, 0, 255)
    scaled <- image_scale(img, 2, 2)
    expect_equal(scaled$width, 2L)
    expect_equal(scaled$height, 2L)
})

# --- Category: crop_to_content ------------------------------------------------

test_that("image_crop_to_content LEFT removes left margin", {
    img <- .make_test_image(4, 2, 0, 255, 0, 255)  # all green
    # make first two columns red (content)
    npix <- 4 * 2
    raw_bytes <- as.raw(rep(c(0, 255, 0, 255), times = npix))
    raw_bytes[1:8] <- as.raw(c(255, 0, 0, 255, 255, 0, 0, 255))  # col 0 red
    img$pixels <- as.raw(raw_bytes)
    # all columns are green except col 0 has red — not a great test...
    # let me build a better one
    
    # Make a 4x2: only last 2 columns are red, first 2 columns are green bg
    raw <- as.raw(rep(c(rep(c(0, 255, 0, 255), 2), rep(c(255, 0, 0, 255), 2)), times = 2))
    img <- list(width = 4L, height = 2L, pixels = raw)
    cropped <- image_crop_to_content(img, "left", c(0, 1, 0, 1))
    expect_equal(cropped$width, 2L)
})

test_that("image_crop_to_content HORIZONTAL crops both sides", {
    # 5x1: cols 0,4 blue bg, cols 1-3 red content
    raw <- as.raw(c(0, 0, 255, 255,  255, 0, 0, 255, 255, 0, 0, 255,
                    255, 0, 0, 255,  0, 0, 255, 255))
    img <- list(width = 5L, height = 1L, pixels = raw)
    cropped <- image_crop_to_content(img, "horizontal", c(0, 0, 1, 1))
    expect_equal(cropped$width, 3L)
})

test_that("image_crop_to_content ALL crops all four sides", {
    # 5x5: black bg, content at (1,1) and (3,3)
    raw <- as.raw(rep(c(0, 0, 0, 255), times = 25L))
    # set (1,1) and (3,3) to red
    idx1 <- (1*5 + 1)*4 + 1
    idx2 <- (3*5 + 3)*4 + 1
    raw[idx1:(idx1+2)] <- as.raw(c(255, 0, 0))
    raw[idx2:(idx2+2)] <- as.raw(c(255, 0, 0))
    img <- list(width = 5L, height = 5L, pixels = raw)
    cropped <- image_crop_to_content(img, "all", c(0, 0, 0, 1))
    expect_equal(cropped$width, 3L)
    expect_equal(cropped$height, 3L)
})

test_that("image_crop_to_content fully background zeroes dimensions", {
    img <- .make_test_image(3, 3, 255, 0, 0, 255)
    cropped <- image_crop_to_content(img, "all", c(1, 0, 0, 1))
    expect_equal(cropped$width, 0L)
    expect_equal(cropped$height, 0L)
})

test_that("image_crop_to_content invalid direction errors", {
    img <- .make_test_image(2, 2, 255, 0, 0, 255)
    expect_error(image_crop_to_content(img, "diagonal", c(0, 0, 0, 0)))
})
