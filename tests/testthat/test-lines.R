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

test_that("line_layer stores whether the layer affects the scene bounds", {
    from <- matrix(c(0, 0, 0), ncol = 3)
    to <- matrix(c(1, 0, 0), ncol = 3)

    expect_true(line_layer(from, to)$affects_bounds)                 # the default
    expect_false(line_layer(from, to, affects_bounds = FALSE)$affects_bounds)

    # The print method marks decorational layers.
    decor_text <- paste(capture.output(print(line_layer(from, to, affects_bounds = FALSE))),
                        collapse = " ")
    content_text <- paste(capture.output(print(line_layer(from, to))), collapse = " ")
    expect_match(decor_text, "decoration")
    expect_false(grepl("decoration", content_text))
})

test_that("camera_fit_scene frames line layers and respects the flag", {
    cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.5, 1))
    mesh_only <- camera_fit_scene(scene(list(cube)))

    # A scene without meshes is framed by its line layers.
    line <- line_layer(matrix(c(-1, 0, 0), ncol = 3), matrix(c(1, 0, 0), ncol = 3))
    cam_lines <- camera_fit_scene(scene(list(), lines = line))
    expect_equal(cam_lines$center, c(0, 0, 0), tolerance = 1e-6)
    expect_gt(abs(cam_lines$eye[3]), 0)

    # Larger lines give a larger camera distance.
    big_line <- line_layer(matrix(c(-10, 0, 0), ncol = 3), matrix(c(10, 0, 0), ncol = 3))
    cam_big <- camera_fit_scene(scene(list(), lines = big_line))
    expect_gt(abs(cam_big$eye[3]), abs(cam_lines$eye[3]))

    # Content lines count, even next to a mesh: the camera is pulled towards them.
    content <- line_layer(matrix(c(100, 0, 0), ncol = 3), matrix(c(101, 0, 0), ncol = 3))
    with_content <- camera_fit_scene(scene(list(cube), lines = content))
    expect_gt(with_content$center[1], 40)

    # Decorational lines are ignored ...
    leader <- line_layer(matrix(c(100, 0, 0), ncol = 3), matrix(c(101, 0, 0), ncol = 3),
                         affects_bounds = FALSE)
    with_leader <- camera_fit_scene(scene(list(cube), lines = leader))
    expect_equal(with_leader$eye, mesh_only$eye, tolerance = 1e-6)
    expect_equal(with_leader$center, mesh_only$center, tolerance = 1e-6)

    # ... unless they are the only content of the scene, which is framed by them
    # anyway (there is nothing else to fit).
    leader_only <- camera_fit_scene(scene(list(), lines = leader))
    expect_gt(leader_only$center[1], 90)

    expect_error(camera_fit_scene("nope"), "scene must be a scene descriptor")
})

test_that("scene_set_line_affects_bounds updates layers by index and by name", {
    cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.5, 1))
    far <- line_layer(matrix(c(100, 0, 0), ncol = 3), matrix(c(101, 0, 0), ncol = 3))
    sc <- scene(list(cube), lines = list(list(lines = far, name = "leader")))
    expect_true(sc$lines[[1]]$lines$affects_bounds)

    # By name: the returned scene is updated, the original one is not.
    sc_named <- scene_set_line_affects_bounds(sc, name = "leader", affects_bounds = FALSE)
    expect_false(sc_named$lines[[1]]$lines$affects_bounds)
    expect_true(sc$lines[[1]]$lines$affects_bounds)

    # The flag changes the framing of the scene.
    expect_gt(camera_fit_scene(sc)$center[1], camera_fit_scene(sc_named)$center[1])

    # By index, on a bare (not node-wrapped) layer.
    sc_bare <- scene(list(cube), lines = far)
    sc_index <- scene_set_line_affects_bounds(sc_bare, index = 1, affects_bounds = FALSE)
    expect_false(sc_index$lines[[1]]$affects_bounds)
    sc_back <- scene_set_line_affects_bounds(sc_index, index = 1, affects_bounds = TRUE)
    expect_true(sc_back$lines[[1]]$affects_bounds)

    expect_error(scene_set_line_affects_bounds(sc), "exactly one of index and name")
    expect_error(scene_set_line_affects_bounds(sc, index = 1, name = "leader"),
                 "exactly one of index and name")
    expect_error(scene_set_line_affects_bounds(sc, index = 3), "index must be in 1")
    expect_error(scene_set_line_affects_bounds(sc, name = "nope"), "no line layer with name")
    expect_error(scene_set_line_affects_bounds(sc, index = 1, affects_bounds = NA),
                 "affects_bounds must be")
    expect_error(scene_set_line_affects_bounds(scene(list(cube)), index = 1),
                 "contains no line layer")
    expect_error(scene_set_line_affects_bounds("nope", index = 1),
                 "scene must be a scene descriptor")
})

test_that("decorational lines do not push the camera away from the data", {
    near <- line_layer(matrix(c(-0.5, 0, 0), ncol = 3), matrix(c(0.5, 0, 0), ncol = 3),
                       colors = c(1, 0, 0, 1), width = 3)
    far <- line_layer(matrix(c(1000, 0, 0), ncol = 3), matrix(c(1001, 0, 0), ncol = 3),
                      colors = c(0, 1, 0, 1), width = 3, affects_bounds = FALSE)
    opts <- render_options(width = 64, height = 64, background_color = c(1, 1, 1, 1))

    sc <- scene(list(), lines = list(near, far))
    img <- render_scene(sc, camera_fit_scene(sc), opts)
    visible_decoration <- count_color(img, c(255, 0, 0))
    expect_gt(visible_decoration, 20)                  # the data is visible
    expect_equal(count_color(img, c(0, 255, 0)), 0)    # the decoration is way outside the frame

    # The same scene with the far line as content zooms out until the near line
    # is too small to be visible: this is what the flag protects against.
    sc_content <- scene_set_line_affects_bounds(sc, index = 2, affects_bounds = TRUE)
    img_content <- render_scene(sc_content, camera_fit_scene(sc_content), opts)
    expect_lt(count_color(img_content, c(255, 0, 0)), visible_decoration / 3)
})
