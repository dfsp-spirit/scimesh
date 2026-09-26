# Per-vertex and per-mesh transparency (alpha blending) through the R API.
#
# Transparency used to need a manual flag in C++ and the blended pass ignored
# the depth buffer (and sorted front-to-back), so translucent geometry behind
# opaque geometry was drawn over it and translucent surfaces were blended in
# the wrong order.  These tests pin down the fixed behaviour.

alpha_cam <- function(distance = 4) camera(eye = c(0, 0, distance),
                                           center = c(0, 0, 0))

alpha_options <- function(width = 64L, height = 64L) {
    render_options(width = width, height = height,
                   backface_culling = FALSE,
                   background_color = c(1, 1, 1, 1),
                   threads = 1L)
}

render_alpha_scene <- function(meshes, cam = alpha_cam(),
                               opts = alpha_options()) {
    image_to_array(render_scene(scene(meshes, cam, opts)))
}

center_pixel <- function(arr) as.numeric(arr[33, 33, 1:4])

read_alpha_example <- function(arr) as.numeric(arr[, , 4])

test_that("set_mesh_alpha validates its input", {
    sphere <- generate_sphere(c(0, 0, 0), 1)
    expect_error(set_mesh_alpha(sphere, -0.1), "alpha")
    expect_error(set_mesh_alpha(sphere, 1.5), "alpha")
    expect_error(set_mesh_alpha(sphere, "0.5"), "alpha")
    expect_error(set_mesh_alpha(sphere, c(0.1, 0.2)), "alpha")
    expect_error(set_mesh_alpha(sphere, NA_real_), "alpha")
})

test_that("set_mesh_alpha sets the alpha of every vertex", {
    sphere <- generate_sphere(c(0, 0, 0), 1)
    ghost <- set_mesh_alpha(sphere, 0.25)

    expect_equal(nrow(ghost$colors), nrow(sphere$vertices))
    expect_true(all(ghost$colors[, 4] == 0.25))
    # RGB (the appearance) is untouched, the input mesh is not modified.
    expect_equal(ghost$colors[, 1:3], sphere$colors[, 1:3])
    expect_true(all(sphere$colors[, 4] == 1))
})

test_that("set_mesh_alpha works without existing colors", {
    quad <- list(vertices = rbind(c(-1, -1, 0), c(1, -1, 0), c(1, 1, 0), c(-1, 1, 0)),
                 triangles = rbind(c(1L, 2L, 3L), c(1L, 3L, 4L)))

    ghost <- set_mesh_alpha(quad, 0.5)
    expect_equal(nrow(ghost$colors), 4L)
    expect_true(all(ghost$colors[, 4] == 0.5))
    # A neutral color, so the mesh looks like it did without colors.
    expect_true(all(ghost$colors[, 1:3] == 0.7))

    # ... and per-face colors are updated as well.
    quad$face_colors <- matrix(rep(c(1, 0, 0, 1), 2), nrow = 2, byrow = TRUE)
    ghost2 <- set_mesh_alpha(quad, 0.2)
    expect_true(all(ghost2$face_colors[, 4] == 0.2))
})

test_that("a translucent mesh is blended with what is behind it", {
    back <- generate_plane(center = c(0, 0, -1), normal = c(0, 0, 1),
                           half_size_x = 3, half_size_y = 3,
                           color = c(0, 0, 1, 1))          # opaque blue
    front_opaque <- generate_plane(center = c(0, 0, 0), normal = c(0, 0, 1),
                                   color = c(1, 0, 0, 1))
    front_ghost <- set_mesh_alpha(front_opaque, 0.5)

    opaque <- center_pixel(render_alpha_scene(list(back, front_opaque)))
    ghost <- center_pixel(render_alpha_scene(list(back, front_ghost)))

    expect_equal(opaque[1], 1, tolerance = 0.02)   # solid red
    expect_gt(ghost[3], 0.2)                       # blue shines through
    expect_gt(ghost[1], 0.2)                       # red is still there
    expect_false(isTRUE(all.equal(opaque, ghost)))
})

test_that("translucent geometry behind an opaque mesh stays hidden", {
    cube <- generate_cuboid(center = c(0, 0, 0), half_extents = c(1, 1, 1))
    behind <- generate_plane(center = c(0, 0, -3), normal = c(0, 0, 1),
                             half_size_x = 0.4, half_size_y = 0.4,
                             color = c(0, 1, 0, 0.5))

    without <- render_alpha_scene(list(cube))
    with <- render_alpha_scene(list(cube, behind))
    expect_equal(without, with)

    # Moved in front of the cube it does change the image.
    in_front <- generate_plane(center = c(0, 0, 3), normal = c(0, 0, 1),
                               half_size_x = 0.4, half_size_y = 0.4,
                               color = c(0, 1, 0, 0.5))
    expect_false(isTRUE(all.equal(without,
                                  render_alpha_scene(list(cube, in_front)))))
})

test_that("translucent surfaces are blended back to front", {
    near_red <- set_mesh_alpha(generate_plane(center = c(0, 0, 1),
                                              normal = c(0, 0, 1),
                                              half_size_x = 2, half_size_y = 2,
                                              color = c(1, 0, 0, 1)), 0.5)
    far_green <- set_mesh_alpha(generate_plane(center = c(0, 0, 0),
                                               normal = c(0, 0, 1),
                                               half_size_x = 2, half_size_y = 2,
                                               color = c(0, 1, 0, 1)), 0.5)

    red_front <- center_pixel(render_alpha_scene(list(far_green, near_red)))
    green_first <- center_pixel(render_alpha_scene(list(near_red, far_green)))

    # The scene order does not matter (the renderer sorts by depth) ...
    expect_equal(red_front, green_first)
    # ... and the nearer surface dominates the blend.
    expect_gt(red_front[1], red_front[2])

    nearer_green <- center_pixel(render_alpha_scene(list(
        set_mesh_alpha(generate_plane(center = c(0, 0, 1), normal = c(0, 0, 1),
                                      half_size_x = 2, half_size_y = 2,
                                      color = c(0, 1, 0, 1)), 0.5),
        set_mesh_alpha(generate_plane(center = c(0, 0, 0), normal = c(0, 0, 1),
                                      half_size_x = 2, half_size_y = 2,
                                      color = c(1, 0, 0, 1)), 0.5))))
    expect_gt(nearer_green[2], nearer_green[1])
})

test_that("color alpha < 1 is honoured for generated meshes", {
    # A single surface, so exactly one layer is blended (a closed translucent
    # mesh stacks its front, back and side faces, which is correct but makes the
    # pixel value harder to predict).
    opaque <- generate_plane(center = c(0, 0, 0), normal = c(0, 0, 1),
                             half_size_x = 2, half_size_y = 2,
                             color = c(1, 0, 0, 1))
    ghost <- generate_plane(center = c(0, 0, 0), normal = c(0, 0, 1),
                            half_size_x = 2, half_size_y = 2,
                            color = c(1, 0, 0, 0.3))

    opaque_px <- center_pixel(render_alpha_scene(list(opaque)))
    ghost_px <- center_pixel(render_alpha_scene(list(ghost)))

    # 30% red over the white background leaves a pinkish pixel.  Note that
    # generate_plane() creates a double-sided plane (front *and* back face, both
    # coplanar), so the background is attenuated by two 30% layers: 0.7 * 0.7.
    # Each of the two faces consists of two triangles sharing a diagonal, and a
    # pixel sitting exactly on such a diagonal is rasterized by exactly one of
    # them (the edge fill rule of the rasterizer), so every pixel gets exactly
    # those two layers.  Which of the two faces is blended first is a tie in the
    # depth sort, so only the (order-independent) channel values are checked
    # exactly.
    expect_equal(ghost_px[2], 0.7^2, tolerance = 0.03)
    expect_gt(ghost_px[1], 0.5)          # still dominated by the red surface
    expect_lt(opaque_px[2], 0.1)
})

test_that("per-vertex alpha is interpolated across a triangle", {
    quad <- list(vertices = rbind(c(-1, -1, 0), c(1, -1, 0),
                                  c(1, 1, 0), c(-1, 1, 0)),
                 triangles = rbind(c(1L, 2L, 3L), c(1L, 3L, 4L)))
    quad$colors <- matrix(rep(c(1, 0, 0, 1), 4), ncol = 4, byrow = TRUE)
    quad$colors[, 4] <- c(0, 1, 1, 0)   # transparent left edge, opaque right

    arr <- render_alpha_scene(list(quad), opts = alpha_options(128L, 128L))
    columns <- seq(40, 88, by = 6)
    green <- as.numeric(arr[64, columns, 2])   # background show-through

    expect_gt(green[1], green[length(green)] + 0.2)
    expect_true(all(diff(green) <= 1e-6))    # monotonically falling
})

test_that("fully transparent geometry is invisible", {
    quad <- set_mesh_alpha(generate_plane(center = c(0, 0, 0),
                                          normal = c(0, 0, 1),
                                          half_size_x = 3, half_size_y = 3,
                                          color = c(1, 1, 1, 1)), 0)
    empty <- render_alpha_scene(list())
    holes <- render_alpha_scene(list(quad))

    expect_equal(empty, holes)
})
