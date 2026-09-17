# Text labels (text_layer(), scene(texts = ...), render_text(), Font helpers).

# Helper: decode the raw RGBA pixel vector of a scimesh image into an Nx4
# integer matrix (rows: pixels, columns: R, G, B, A).
image_pixels <- function(img) {
    matrix(as.integer(img$pixels), ncol = 4L, byrow = TRUE)
}

# Helper: number of pixels close to the given RGB color.
count_color <- function(img, rgb, tolerance = 20) {
    px <- image_pixels(img)
    sum(abs(px[, 1] - rgb[1]) <= tolerance &
        abs(px[, 2] - rgb[2]) <= tolerance &
        abs(px[, 3] - rgb[3]) <= tolerance)
}

# Helper: number of pixels that differ between two images of equal size.
count_differences <- function(a, b, tolerance = 8) {
    pa <- image_pixels(a)
    pb <- image_pixels(b)
    su <- abs(pa - pb) > tolerance
    sum(apply(su, 1L, any))
}

# Helper: small render options with a white background.
small_options <- function(width = 120, height = 120) {
    render_options(width = width, height = height,
                   background_color = c(1, 1, 1, 1))
}

test_that("default_font() returns the bundled, usable font", {
    font <- default_font()
    expect_type(font, "character")
    expect_length(font, 1L)
    expect_true(file.exists(font))
    expect_match(font, "\\.ttf$")
    # The bundled font ships with the package.
    expect_match(font, "Inter-Regular\\.ttf$")

    info <- font_info()
    expect_equal(info$family, "Inter")
    expect_equal(info$size, 18)
    expect_true(info$ascent > 0)
    expect_true(info$descent >= 0)
    expect_equal(info$path, font)

    # A missing font file is reported instead of being ignored.
    expect_error(font_info("does_not_exist.ttf"), "not found")
    expect_error(font_info(NULL, size = -1), "positive")
})

test_that("text_extent() measures labels like the renderer does", {
    one <- text_extent("anterior", size = 20)
    expect_s3_class(one, "data.frame")
    expect_equal(names(one),
                 c("text", "width", "height", "ascent", "descent", "lines"))
    expect_equal(nrow(one), 1L)
    expect_equal(one$lines, 1)
    expect_true(one$width > 0)
    expect_true(one$height > 0)
    expect_equal(one$ascent + one$descent, one$height)

    # Width grows with the size, and a longer string is wider.
    expect_true(text_extent("anterior", size = 40)$width >
                text_extent("anterior", size = 20)$width)
    expect_true(text_extent("anterior")$width > text_extent("ant")$width)

    # Multi-line labels are taller but no wider than their widest line.
    short <- text_extent("ab\ncd")
    expect_equal(short$lines, 2)
    expect_true(short$height > text_extent("ab")$height)
    expect_equal(short$width,
                 max(text_extent("ab")$width, text_extent("cd")$width))

    # A vector of strings is measured row by row.
    many <- text_extent(c("a", "bb", "ccc"))
    expect_equal(nrow(many), 3L)
    expect_true(all(diff(many$width) > 0))

    expect_error(text_extent("x", size = 0), "positive")
    expect_error(text_extent("x", line_spacing = -1), "positive")
    expect_error(text_extent(42), "character")
})

test_that("text_layer() creates a text layer descriptor", {
    layer <- text_layer(matrix(c(0, 1, 0), ncol = 3), "anterior")

    expect_s3_class(layer, "scimesh_text")
    expect_equal(layer$strings, "anterior")
    expect_equal(dim(layer$positions), c(1L, 3L))
    expect_equal(layer$size, 18)
    expect_equal(layer$space, "world")
    expect_equal(layer$adj, c(0.5, 0.5))
    expect_equal(layer$offset, c(0, 0))
    expect_true(layer$depth_test)
    expect_null(layer$halo_color)
    expect_true(file.exists(layer$font_file))
    # No colors given: the renderer falls back to the default color.
    expect_equal(nrow(layer$colors), 0L)

    # Screen space accepts 2-column positions and a single point as a vector.
    screen <- text_layer(c(10, 20), "A", space = "screen", size = 30,
                         halo_color = c(1, 1, 1), colors = c(0, 0, 0, 1))
    expect_equal(dim(screen$positions), c(1L, 2L))
    expect_equal(screen$adj, c(0.5, 0.5))
    expect_equal(screen$size, 30)
    expect_equal(screen$halo_color, c(1, 1, 1, 1)) # alpha defaults to 1
    expect_equal(dim(screen$colors), c(1L, 4L))

    # One string with several positions is recycled.
    many <- text_layer(matrix(c(0, 0, 0, 1, 1, 1), ncol = 3, byrow = TRUE), "x")
    expect_equal(many$strings, c("x", "x"))

    # A colour vector is recycled per label, and per-label colours work.
    per_label <- text_layer(matrix(c(0, 0, 0, 1, 1, 1), ncol = 3, byrow = TRUE),
                            c("a", "b"),
                            colors = matrix(c(1, 0, 0, 1, 0, 0, 1, 1),
                                            ncol = 4, byrow = TRUE))
    expect_equal(unname(per_label$colors[1, ]), c(1, 0, 0, 1))
    expect_equal(unname(per_label$colors[2, ]), c(0, 0, 1, 1))

    expect_output(print(layer), "1 label")
    expect_output(print(screen), "screen space")
})

test_that("text_layer() validates its input", {
    pts <- matrix(c(0, 1, 0), ncol = 3)

    expect_error(text_layer(NULL, "a"), "must not be NULL")
    expect_error(text_layer(matrix(c(0, 1), ncol = 1), "a"), "Nx2 .*Nx3")
    expect_error(text_layer(matrix(numeric(0), ncol = 3), "a"), "at least one")
    expect_error(text_layer(pts, character(0)), "character vector")
    expect_error(text_layer(pts, NA_character_), "NA")
    # World-space labels need three coordinates.
    expect_error(text_layer(c(10, 20), "a", space = "world"), "need Nx3")
    # Three strings for two positions do not divide evenly.
    expect_error(text_layer(matrix(c(0, 0, 0, 1, 1, 1), ncol = 3, byrow = TRUE),
                            c("a", "b", "c"), space = "screen"),
                 "entry/entries")
    expect_error(text_layer(pts, "a", size = 0), "positive")
    expect_error(text_layer(pts, "a", adj = c(0.5, 0.5, 0.5)), "length 2")
    expect_error(text_layer(pts, "a", adj = c(-1, 0.5)), "\\[0, 1\\]")
    expect_error(text_layer(pts, "a", offset = 1), "length 2")
    expect_error(text_layer(pts, "a", halo_color = c(1, 1)), "RGB or RGBA")
    expect_error(text_layer(pts, "a", halo_width = -1), "non-negative")
    expect_error(text_layer(pts, "a", line_spacing = 0), "positive")
    expect_error(text_layer(pts, "a", space = "nope"), "should be one of")
    expect_error(text_layer(pts, "a", font_file = "missing.ttf"), "not found")
})

test_that("scene() accepts text layers", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(0.5, 0.5, 0.5, 1))
    layer <- text_layer(matrix(c(0, 2, 0), ncol = 3), "top")

    sc <- scene(list(cube), texts = layer)
    expect_s3_class(sc, "scimesh_scene")
    expect_length(sc$texts, 1)
    expect_output(print(sc), "1 text layer")

    # A single layer, a list of layers, and NULL all work.
    expect_length(scene(list(cube), texts = list(layer))$texts, 1)
    expect_length(scene(list(cube))$texts, 0)
    expect_length(scene(list(cube), texts = NULL)$texts, 0)

    # Text layers can be wrapped into a scene node with a transform.
    tr <- diag(1, 4)
    tr[1, 4] <- 5
    wrapped <- scene(list(cube), texts = list(list(text = layer, transform = tr)))
    expect_length(wrapped$texts, 1)

    # Line and text layers can be combined.
    lines <- line_layer(matrix(c(-1, 0, 2), ncol = 3),
                        matrix(c(1, 0, 2), ncol = 3), width = 2)
    both <- scene(list(cube), lines = lines, texts = layer)
    expect_length(both$lines, 1)
    expect_length(both$texts, 1)

    expect_error(scene(list(cube), texts = "nope"), "text layer")
    expect_error(scene(list(cube), texts = list(layer, "nope")), "texts\\[\\[2\\]\\]")
})

test_that("render_text() draws screen-space labels without meshes", {
    img <- render_text(c(10, 10), "A", space = "screen", adj = c(0, 1),
                       size = 30, colors = c(0, 0, 0, 1),
                       options = small_options(80, 60))

    expect_equal(img$width, 80)
    expect_equal(img$height, 60)
    expect_type(img$pixels, "raw")
    expect_true(count_color(img, c(0, 0, 0)) > 5)

    # Nothing was drawn far away from the label.
    px <- image_pixels(img)
    far <- px[1, ]
    expect_equal(as.integer(far[1:3]), c(255L, 255L, 255L))

    # A halo adds pixels around the glyph.
    plain <- render_text(c(10, 10), "A", space = "screen", adj = c(0, 1),
                         size = 30, colors = c(0, 0, 0, 1),
                         options = small_options(80, 60))
    haloed <- render_text(c(10, 10), "A", space = "screen", adj = c(0, 1),
                          size = 30, colors = c(0, 0, 0, 1),
                          halo_color = c(1, 0, 0, 1), halo_width = 2,
                          options = small_options(80, 60))
    expect_true(count_color(haloed, c(255, 0, 0), 60) > 0)
    expect_true(count_differences(plain, haloed) > 5)

    # A multi-line label is taller than a single-line one.
    one <- render_text(c(5, 5), "H", space = "screen", adj = c(0, 1), size = 24,
                       colors = c(0, 0, 0, 1), options = small_options(120, 120))
    two <- render_text(c(5, 5), "H\nH", space = "screen", adj = c(0, 1), size = 24,
                       colors = c(0, 0, 0, 1), options = small_options(120, 120))
    expect_true(count_differences(one, two) > 5)
    expect_true(count_color(two, c(0, 0, 0)) > 1.5 * count_color(one, c(0, 0, 0)))
})

test_that("render_scene() draws world-space labels at the projected position", {
    sph <- generate_sphere(c(0, 0, 0), radius = 1, segments = 24)
    cam <- camera(eye = c(0, 0, 6), center = c(0, 0, 0))
    opts <- small_options(200, 200)

    # The label sits above the sphere, so it is not occluded by it.
    layer <- text_layer(matrix(c(0, 1.5, 0), ncol = 3), "top", size = 24,
                        colors = c(0, 0, 0, 1), adj = c(0.5, 0.5))
    with_label <- render_scene(scene(list(sph), camera = cam, options = opts,
                                     texts = layer))
    without <- render_scene(scene(list(sph), camera = cam, options = opts))

    expect_true(count_differences(with_label, without) > 5)

    # The label's pixels are exactly the pixels that differ from the render
    # without labels; their centre must be at the projected anchor position.
    projected <- world_to_screen(matrix(c(0, 1.5, 0), ncol = 3), cam, 200, 200,
                                 opts)
    expect_true(projected$in_front)
    anchor_x <- round(projected$x)
    anchor_y <- round(projected$y)

    changed <- which(apply(abs(image_pixels(with_label) - image_pixels(without)) >
                               8L, 1L, any))
    expect_true(length(changed) > 5)
    changed_x <- (changed - 1L) %% 200L
    changed_y <- (changed - 1L) %/% 200L
    expect_true(abs(mean(changed_x) - anchor_x) <= 12)
    expect_true(abs(mean(changed_y) - anchor_y) <= 12)
})

test_that("depth_test hides labels behind the geometry", {
    cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.7, 1))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- small_options(120, 120)

    plain <- render_scene(scene(list(cube), camera = cam, options = opts))
    hidden <- render_scene(scene(list(cube), camera = cam, options = opts,
                                 texts = text_layer(matrix(c(0, 0, 0), ncol = 3),
                                                    "X", size = 24,
                                                    colors = c(0, 0, 0, 1))))
    shown <- render_scene(scene(list(cube), camera = cam, options = opts,
                                texts = text_layer(matrix(c(0, 0, 0), ncol = 3),
                                                   "X", size = 24,
                                                   colors = c(0, 0, 0, 1),
                                                   depth_test = FALSE)))

    # Inside the cube: hidden by default, drawn when depth_test is off.
    expect_equal(count_differences(plain, hidden), 0)
    expect_true(count_differences(plain, shown) > 5)

    # In front of the cube the label is visible either way.
    in_front <- render_scene(scene(list(cube), camera = cam, options = opts,
                                   texts = text_layer(matrix(c(0, 0, 2.5), ncol = 3),
                                                      "X", size = 24,
                                                      colors = c(0, 0, 0, 1))))
    expect_true(count_differences(plain, in_front) > 5)
})

test_that("world_to_screen() matches the rendered image", {
    cam <- camera(eye = c(0, 0, 4), center = c(0, 0, 0))

    centre <- world_to_screen(matrix(c(0, 0, 0), ncol = 3), cam, 200, 100)
    expect_equal(nrow(centre), 1L)
    expect_equal(names(centre), c("x", "y", "depth", "in_front"))
    expect_equal(centre$x, 100, tolerance = 0.5)
    expect_equal(centre$y, 50, tolerance = 0.5)
    expect_true(centre$in_front)

    # +Y is up, so it maps to a smaller y (the origin is the top left corner).
    above <- world_to_screen(matrix(c(0, 0.5, 0), ncol = 3), cam, 200, 100)
    expect_true(above$y < centre$y)
    # +X is to the right.
    right <- world_to_screen(matrix(c(0.5, 0, 0), ncol = 3), cam, 200, 100)
    expect_true(right$x > centre$x)
    # Closer points have a smaller depth.
    near <- world_to_screen(matrix(c(0, 0, 1), ncol = 3), cam, 200, 100)
    expect_true(near$depth < centre$depth)

    # A point behind the camera is flagged, not silently projected.
    behind <- world_to_screen(matrix(c(0, 0, 8), ncol = 3), cam, 200, 100)
    expect_false(behind$in_front)

    # Several points at once, and the orthographic projection.
    many <- world_to_screen(matrix(c(0, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE),
                            cam, 200, 100)
    expect_equal(nrow(many), 2L)
    ortho_cam <- camera(eye = c(0, 0, 4), center = c(0, 0, 0),
                        projection = "orthographic")
    ortho <- world_to_screen(matrix(c(0, 0, 0), ncol = 3), ortho_cam, 200, 100)
    expect_equal(ortho$x, 100, tolerance = 0.5)

    expect_error(world_to_screen(matrix(c(0, 0, 0), ncol = 3), cam, 0, 100),
                 "positive")
    expect_error(world_to_screen(NULL, cam, 100, 100), "must not be NULL")
})
