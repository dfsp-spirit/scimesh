# Tests for the fog coordinate space: world units (default) vs the legacy
# normalized-device-depth (NDC) fog.

# The camera sits at (0, 0, 10) looking at the origin with an orthographic
# projection, so the ortho half-height is 10 world units and world (x, y)
# maps linearly onto pixels.
fog_camera <- function() {
    camera(eye = c(0, 0, 10), center = c(0, 0, 0), projection = "orthographic")
}

quad_mesh <- function(z, half, color) {
    list(vertices = cbind(c(-half, half, half, -half),
                          c(-half, -half, half, half),
                          rep(z, 4)),
         triangles = rbind(c(1L, 2L, 3L), c(1L, 3L, 4L)),
         colors = matrix(color, nrow = 4, ncol = 4, byrow = TRUE))
}

# Near quad (2 x 2 at z = 0, i.e. 10 units from the camera) in front of a
# larger backdrop (6 x 6 at z = -20, i.e. 30 units from the camera).
fog_scene <- function() {
    list(quad_mesh(0, 1, c(1, 0, 0, 1)),
         quad_mesh(-20, 3, c(1, 0, 0, 1)))
}

fog_options <- function(fog_enabled, near_plane = 0.1, far_plane = 1000) {
    render_options(width = 100, height = 100, projection = "orthographic",
                   near_plane = near_plane, far_plane = far_plane,
                   background_color = c(0, 0, 0, 1), threads = 1L,
                   fog_enabled = fog_enabled,
                   fog_start = 5, fog_end = 15,
                   fog_color = c(0, 1, 0, 1))  # green
}

# Colour at world position (x, y), as a length-4 vector (RGBA, 0-1).
pixel_at <- function(img, x, y, dist = 10) {
    arr <- image_to_array(img)
    h <- dim(arr)[1]
    w <- dim(arr)[2]
    col <- floor((x / dist + 1) / 2 * w) + 1
    row <- floor((1 - y / dist) / 2 * h) + 1
    arr[row, col, ]
}

max_pixel_diff <- function(a, b) {
    max(abs(as.integer(a$pixels) - as.integer(b$pixels)))
}

test_that("fog defaults to world units", {
    expect_equal(render_options()$fog_space, "world")

    scene <- fog_scene()
    default_space <- render_scene(scene, fog_camera(), fog_options(TRUE))
    explicit <- render_scene(scene, fog_camera(),
                             modifyList(fog_options(TRUE),
                                        list(fog_space = "world")))
    expect_identical(default_space$pixels, explicit$pixels)
})

test_that("render_options validates fog settings", {
    expect_error(render_options(fog_space = "eye"), "arg")
    expect_error(render_options(fog_start = 10, fog_end = 10), "greater")
    expect_error(render_options(fog_start = 10, fog_end = 5), "greater")
    expect_error(render_options(fog_start = "a"), "finite")
    expect_error(render_options(fog_end = c(1, 2)), "finite")
    # Valid: the same settings in both spaces
    expect_equal(render_options(fog_space = "ndc")$fog_space, "ndc")
    expect_equal(render_options(fog_start = 0, fog_end = 1)$fog_end, 1)

    # The depth range defines the NDC fog unit, so it is validated too.
    expect_error(render_options(near_plane = 0), "positive")
    expect_error(render_options(near_plane = 5, far_plane = 1), "larger")
    expect_equal(render_options(near_plane = 2, far_plane = 500)$far_plane, 500)
})

test_that("world-space fog fades geometry by its distance from the camera", {
    scene <- fog_scene()
    unfogged <- render_scene(scene, fog_camera(), fog_options(FALSE))
    fogged <- render_scene(scene, fog_camera(), fog_options(TRUE))

    # Near quad: 10 units away, fog spans 5..15 world units -> factor 0.5.
    near_base <- pixel_at(unfogged, 0, 0)
    near_fogged <- pixel_at(fogged, 0, 0)
    expect_lt(abs(near_fogged[1] - (0.5 * near_base[1] + 0.5 * 0)), 0.02)
    expect_lt(abs(near_fogged[2] - (0.5 * near_base[2] + 0.5 * 1)), 0.02)
    expect_lt(abs(near_fogged[3] - (0.5 * near_base[3] + 0.5 * 0)), 0.02)

    # Backdrop: 30 units away, beyond fog_end -> completely fogged.
    far_base <- pixel_at(unfogged, 2, 2)
    far_fogged <- pixel_at(fogged, 2, 2)
    expect_gt(far_base[1], 0.2)   # sanity: the backdrop is drawn there
    expect_lt(abs(far_fogged[1] - 0), 0.01)
    expect_lt(abs(far_fogged[2] - 1), 0.01)
    expect_lt(abs(far_fogged[3] - 0), 0.01)
})

test_that("world-space fog ignores near/far, NDC fog does not", {
    scene <- fog_scene()

    # World units: the fade only depends on the camera distance.
    world_long <- render_scene(scene, fog_camera(), fog_options(TRUE, 0.1, 1000))
    world_short <- render_scene(scene, fog_camera(), fog_options(TRUE, 0.1, 60))
    # Identical up to the +/-1 rounding of the fog blend.
    expect_lt(max_pixel_diff(world_long, world_short), 2)

    # NDC: the same fog settings now depend on the depth range.  The quads are
    # 10 and 30 world units away, but their depth-buffer values are -0.98 /
    # -0.94 for far = 1000 and -0.67 / -0.002 for far = 60, so the NDC band
    # [-0.8, 0] contains no geometry in the first case and almost all of it in
    # the second.
    long_opts <- modifyList(fog_options(TRUE, 0.1, 1000),
                            list(fog_space = "ndc", fog_start = -0.8, fog_end = 0))
    short_opts <- modifyList(long_opts, list(far_plane = 60))
    ndc_long <- render_scene(scene, fog_camera(), long_opts)
    ndc_short <- render_scene(scene, fog_camera(), short_opts)
    expect_gt(max_pixel_diff(ndc_long, ndc_short), 100)

    # With far = 60 the distant backdrop is fully fogged while the near quad
    # is barely affected ...
    near_px <- pixel_at(ndc_short, 0, 0)
    far_px <- pixel_at(ndc_short, 2, 2)
    expect_gt(near_px[1], 0.7)
    expect_gt(far_px[2], 0.99)
    expect_lt(far_px[1], 0.01)

    # ... whereas with far = 1000 nothing is inside the fog band at all.
    expect_gt(pixel_at(ndc_long, 2, 2)[1], 0.9)
})

test_that("legacy NDC fog uses raw depth-buffer values", {
    scene <- fog_scene()
    # near = 5, far = 40: the quads sit at z_ndc = -0.71 (10 units) and
    # z_ndc = +0.43 (30 units).  With NDC fog from -0.5 to 0.5 the near quad
    # is not fogged at all while the backdrop is almost completely fogged -
    # the opposite of what the same world distances produce in world space.
    opts <- modifyList(fog_options(TRUE, 5, 40),
                       list(fog_space = "ndc", fog_start = -0.5, fog_end = 0.5))
    img <- render_scene(scene, fog_camera(), opts)

    near_px <- pixel_at(img, 0, 0)
    expect_gt(near_px[1], 0.5)   # still red: not fogged
    expect_lt(near_px[2], 0.1)

    far_px <- pixel_at(img, 2, 2)
    expect_gt(far_px[2], 0.85)   # almost green: fully fogged
    expect_lt(far_px[1], 0.15)
})
