# Screen-space line layers (line_layer(), scene(lines = ...), render_segments()).

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

test_that("line_layer creates a line layer descriptor", {
    from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
    to <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)

    layer <- line_layer(from, to, colors = c(1, 0, 0, 1), width = 2,
                        depth_test = FALSE, lit = TRUE)

    expect_s3_class(layer, "scimesh_lines")
    expect_equal(nrow(layer$from), 2)
    expect_equal(nrow(layer$to), 2)
    expect_equal(dim(layer$colors), c(2L, 4L))
    expect_equal(layer$colors[1, ], c(1, 0, 0, 1))
    expect_equal(layer$width, 2)
    expect_false(layer$depth_test)
    expect_true(layer$lit)

    # Defaults: width 1, depth test on, flat (unlit) lines.
    plain <- line_layer(from, to)
    expect_equal(plain$width, 1)
    expect_true(plain$depth_test)
    expect_false(plain$lit)
    # No colors given: the renderer falls back to the default color.
    expect_equal(nrow(plain$colors), 0)

    # A single length-3 vector is accepted as one segment.
    single <- line_layer(c(0, 0, 0), c(1, 1, 1))
    expect_equal(nrow(single$from), 1)

    expect_output(print(layer), "2 segment")
})

test_that("line_layer validates its input", {
    from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
    to <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)

    expect_error(line_layer(from, to[1, , drop = FALSE]), "same number of rows")
    expect_error(line_layer(from, "nope"), "Nx3 numeric matrix")
    expect_error(line_layer(from, to, colors = c(1, 2)), "RGB or RGBA")
    expect_error(line_layer(from, to, width = -1), "positive")
    expect_error(line_layer(from, to, width = c(1, 2)), "positive")
})

test_that("scene() accepts line layers", {
    cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.5, 1))
    layer <- line_layer(matrix(c(-1, 0, 2), ncol = 3),
                        matrix(c(1, 0, 2), ncol = 3), width = 2)

    sc <- scene(list(cube), lines = layer)
    expect_s3_class(sc, "scimesh_scene")
    expect_length(sc$lines, 1)
    expect_output(print(sc), "1 line layer")

    # A single layer, a list of layers, and NULL all work.
    expect_length(scene(list(cube), lines = list(layer))$lines, 1)
    expect_length(scene(list(cube))$lines, 0)

    # A layer wrapped into a node with a transform is kept as a node.
    tr <- diag(1, 4)
    tr[1, 4] <- 5
    sc2 <- scene(list(cube), lines = list(list(lines = layer, transform = tr)))
    expect_length(sc2$lines, 1)

    expect_error(scene(list(cube), lines = list("nope")), "not a line layer")
    expect_error(scene(list(cube), lines = "nope"), "line layer")
})

test_that("render_scene draws line layers", {
    layer <- line_layer(matrix(c(-1, 0, 0), ncol = 3),
                        matrix(c(1, 0, 0), ncol = 3),
                        colors = c(1, 0, 0, 1), width = 2)
    sc <- scene(list(), lines = layer)
    img <- render_scene(sc, camera(c(0, 0, 5), c(0, 0, 0)),
                        render_options(width = 64, height = 64))

    expect_equal(img$width, 64)
    expect_gt(count_color(img, c(255, 0, 0)), 30)
})

test_that("line layers respect the depth buffer of the scene", {
    cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.5, 1))
    opts <- render_options(width = 64, height = 64)
    cam <- camera(c(0, 0, 5), c(0, 0, 0))

    # A line in front of the cube (front face at z = 1) is visible.
    front <- line_layer(matrix(c(-0.8, 0, 1.5), ncol = 3),
                        matrix(c(0.8, 0, 1.5), ncol = 3),
                        colors = c(1, 0, 0, 1), width = 2)
    img_front <- render_scene(scene(list(cube), lines = front), cam, opts)
    expect_gt(count_color(img_front, c(255, 0, 0)), 0)

    # A line inside/behind the cube is hidden by it.
    behind <- line_layer(matrix(c(-0.8, 0.5, 0), ncol = 3),
                         matrix(c(0.8, 0.5, 0), ncol = 3),
                         colors = c(0, 255, 0), width = 2)
    img_behind <- render_scene(scene(list(cube), lines = behind), cam, opts)
    expect_equal(count_color(img_behind, c(0, 255, 0)), 0)
})

test_that("line layers do not change the camera framing", {
    cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.5, 1))
    cam <- camera(c(0, 0, 5), c(0, 0, 0))
    opts <- render_options(width = 32, height = 32)

    img_mesh <- render_scene(scene(list(cube)), cam, opts)
    far_layer <- line_layer(matrix(c(-100, -100, -100), ncol = 3),
                            matrix(c(100, 100, 100), ncol = 3),
                            colors = c(1, 0, 0, 1), width = 1)
    img_lines <- render_scene(scene(list(cube), lines = far_layer), cam, opts)

    # The mesh covers the same pixels, i.e. the camera did not move.
    px_mesh <- image_pixels(img_mesh)
    px_lines <- image_pixels(img_lines)
    mesh_only <- px_mesh[, 1] < 200
    expect_gt(sum(mesh_only & (px_lines[, 1] < 200)), 0.8 * sum(mesh_only))
})

test_that("render_segments renders free-standing lines", {
    from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
    to <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)

    img <- render_segments(from, to, colors = c(1, 0, 0, 1), width = 3,
                           options = render_options(width = 64, height = 64))
    expect_equal(img$width, 64)
    expect_gt(count_color(img, c(255, 0, 0)), 50)

    # Translucent lines blend with the background instead of replacing it:
    # fewer fully saturated pixels, but more mixed ones than the opaque case.
    translucent <- render_segments(from, to, colors = c(1, 0, 0, 0.4),
                                   width = 3,
                                   options = render_options(width = 64, height = 64))
    px_opaque <- image_pixels(img)
    px_trans <- image_pixels(translucent)
    pure_red <- function(px) sum(px[, 1] > 240 & px[, 2] < 20 & px[, 3] < 20)

    expect_lt(pure_red(px_trans), pure_red(px_opaque))
    expect_gt(sum(px_trans[, 1] > 200 & px_trans[, 2] >= 20), 50)
})

test_that("empty line layers are allowed", {
    empty <- line_layer(matrix(numeric(0), ncol = 3), matrix(numeric(0), ncol = 3))
    expect_equal(nrow(empty$from), 0)

    sc <- scene(list(), lines = empty)
    img <- render_scene(sc, camera(c(0, 0, 5), c(0, 0, 0)),
                        render_options(width = 16, height = 16))
    expect_equal(count_color(img, c(255, 0, 0)), 0)

    img2 <- render_segments(matrix(numeric(0), ncol = 3),
                            matrix(numeric(0), ncol = 3),
                            options = render_options(width = 16, height = 16))
    expect_equal(img2$width, 16)
})

test_that("glTF export skips line layers with a warning", {
    tmp <- tempfile(fileext = ".gltf")
    layer <- line_layer(matrix(c(0, 0, 0), ncol = 3), matrix(c(1, 1, 1), ncol = 3))
    sc <- scene(list(generate_cuboid(c(0, 0, 0), c(1, 1, 1))), lines = layer)
    expect_warning(write_gltf(sc, tmp), "line layer")
})
