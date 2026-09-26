# Tests for user clip planes: clip_plane(), input validation, and the
# world-space (default) vs eye-space (explicit) semantics.

# Camera looking straight down -Y with an orthographic projection, so that
# image rows map linearly onto world z (half-height == camera distance == 10).
# This makes the clip plane's cut position measurable in world units.
ortho_cam <- function(eye_z) {
    camera(eye = c(0, 10, eye_z), center = c(0, 0, eye_z), up = c(0, 0, -1),
           projection = "orthographic")
}

# World-space z range covered by the rendered geometry, or NULL if nothing
# is drawn.  `arr` rows map to z via ndc_y = 1 - 2 * (r - 0.5) / h.
visible_world_z <- function(img, eye_z) {
    arr <- image_to_array(img)
    h <- dim(arr)[1]
    fg <- apply(arr[, , 1:3, drop = FALSE], 1, function(row) any(row < 1))
    rows <- which(fg)
    if (length(rows) == 0L) {
        return(NULL)
    }
    ndc_y <- 1 - 2 * (rows - 0.5) / h
    z <- eye_z - ndc_y * 10
    c(min = min(z), max = max(z))
}

fg_count <- function(img) {
    arr <- image_to_array(img)
    sum(apply(arr[, , 1:3, drop = FALSE], 1, function(row) any(row < 1)))
}

cube_mesh <- function() {
    generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.5, 0.5, 0.5, 1))
}

clip_options <- function(planes) {
    render_options(width = 100, height = 100, projection = "orthographic",
                   backface_culling = FALSE, threads = 1L,
                   clip_planes = planes)
}

test_that("clip_plane() builds clip plane descriptors", {
    p <- clip_plane(normal = c(-1, 0, 0), offset = 0)
    expect_equal(p$normal, c(-1, 0, 0))
    expect_equal(p$offset, 0)
    expect_equal(p$space, "world")   # world space is the default

    expect_equal(clip_plane(c(0, 0, 1), 0.5)$offset, 0.5)
    expect_equal(clip_plane(c(0, 0, -1), -2, space = "eye")$space, "eye")
    expect_equal(clip_plane(c(0, 0, 1))$offset, 0)  # offset defaults to 0

    # The normal is normalized internally, so scaling it changes nothing.
    p_scaled <- clip_plane(c(0, 0, 7), 2)
    expect_equal(p_scaled$normal, c(0, 0, 7))
    expect_equal(p_scaled$offset, 2)

    expect_error(clip_plane(c(1, 0, 0), 0, space = "camera"), "arg")
    expect_error(clip_plane(c(1, 0), 0), "length 3")
    expect_error(clip_plane(c(0, 0, 0), 0), "zero")
})

test_that("render_options validates clip planes", {
    expect_null(render_options()$clip_planes)
    expect_null(render_options(clip_planes = list())$clip_planes)

    # Plain descriptors (without space) are accepted and default to world.
    opts <- render_options(clip_planes = list(
        list(normal = c(-1, 0, 0), offset = 0)))
    expect_equal(opts$clip_planes[[1]]$space, "world")

    expect_error(render_options(clip_planes = "not a list"), "must be a list")
    expect_error(render_options(clip_planes = list(1, 2)), "must be a list")
    expect_error(render_options(clip_planes = list(list(offset = 1))), "normal")
    expect_error(render_options(clip_planes = list(
        list(normal = c(1, 0), offset = 0))), "length 3")
    expect_error(render_options(clip_planes = list(
        list(normal = c(0, 0, 0), offset = 0))), "zero")
    expect_error(render_options(clip_planes = list(
        list(normal = c(1, 0, 0), offset = c(1, 2)))), "offset")
    expect_error(render_options(clip_planes = list(
        list(normal = c(1, 0, 0), offset = 0, space = "view"))), "world")
})

test_that("world-space clip planes cut the same place for any camera", {
    mesh <- cube_mesh()
    opts <- clip_options(list(clip_plane(normal = c(0, 0, 1), offset = 0)))

    img0 <- render_scene(list(mesh), ortho_cam(0), opts)
    img4 <- render_scene(list(mesh), ortho_cam(4), opts)

    # The cube spans z in [-1, 1]; the cut must sit at world z = 0.
    z0 <- visible_world_z(img0, 0)
    expect_false(is.null(z0))
    expect_lt(abs(z0["min"] - 0), 0.2)
    expect_lt(abs(z0["max"] - 0.95), 0.15)

    # Moving the camera along the plane normal does not move the cut.  (The
    # cube moves within the image because the camera slides sideways, so the
    # world-space cut position is what has to stay put.)
    z4 <- visible_world_z(img4, 4)
    expect_false(is.null(z4))
    expect_lt(abs(unname(z4["min"]) - unname(z0["min"])), 0.1)
    expect_lt(abs(unname(z4["max"]) - unname(z0["max"])), 0.1)
})

test_that("eye-space clip planes follow the camera (explicit opt-in)", {
    mesh <- cube_mesh()
    opts <- clip_options(list(clip_plane(normal = c(0, 0, 1), offset = 0,
                                         space = "eye")))

    # At eye_z = 0 the eye-space plane happens to coincide with world z = 0.
    img0 <- render_scene(list(mesh), ortho_cam(0), opts)
    z0 <- visible_world_z(img0, 0)
    expect_false(is.null(z0))
    expect_lt(abs(z0["min"] - 0), 0.2)

    # At eye_z = 4 the plane has travelled with the camera, so everything in
    # front of it is clipped away.
    img4 <- render_scene(list(mesh), ortho_cam(4), opts)
    expect_null(visible_world_z(img4, 4))
})

test_that("multiple clip planes are combined with a logical AND", {
    mesh <- cube_mesh()
    lower <- clip_plane(normal = c(0, 0, 1), offset = 0.5)   # keep z >= -0.5
    upper <- clip_plane(normal = c(0, 0, -1), offset = 0.5)  # keep z <= 0.5

    slab <- render_scene(list(mesh), ortho_cam(0), clip_options(list(lower, upper)))
    z <- visible_world_z(slab, 0)
    expect_false(is.null(z))
    expect_lt(abs(z["min"] - (-0.45)), 0.2)
    expect_lt(abs(z["max"] - 0.45), 0.2)

    # Each plane alone keeps more of the cube than both together.
    only_lower <- render_scene(list(mesh), ortho_cam(0), clip_options(list(lower)))
    expect_gt(fg_count(only_lower), fg_count(slab))
})

test_that("a zero-length normal is rejected instead of blanking the scene", {
    mesh <- cube_mesh()
    expect_error(render_options(clip_planes = list(
        clip_plane(normal = c(0, 0, 0), offset = 1))), "zero")

    # Without clipping, the whole cube is drawn.
    img <- render_scene(list(mesh), ortho_cam(0), clip_options(NULL))
    expect_gt(fg_count(img), 0)
})
