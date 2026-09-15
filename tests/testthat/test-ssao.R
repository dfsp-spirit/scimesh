# Screen-space ambient occlusion (SSAO) is a post-process that darkens pixels
# where a nearby fragment is closer to the camera.  It used to be inert (wrong
# depth range, wrong radius unit, inverted hemisphere test), so these tests do
# not check the exact shading - only that switching it on really does something.

# A large plane with a small cuboid hovering in front of it.  The part of the
# plane directly around the cuboid is the classic "contact shadow" case.
ssao_scene <- function() {
    wall <- generate_plane(center = c(0, 0, -1), normal = c(0, 0, 1),
                           half_size_x = 4, half_size_y = 4)
    box <- generate_cuboid(center = c(0, 0, -0.5), half_extents = c(0.2, 0.2, 0.2))
    list(wall = wall, box = box)
}

ssao_options <- function(ssao_enabled = FALSE, projection = "perspective") {
    render_options(width = 64, height = 64, projection = projection,
                   background_color = c(1, 1, 1, 1),
                   default_color = c(0.7, 0.7, 0.7, 1),
                   near_plane = 0.5, far_plane = 20,
                   ssao_enabled = ssao_enabled,
                   ssao_radius = 8,       # pixels
                   ssao_intensity = 1.0)
}

test_that("ssao_enabled changes the rendered image", {
    meshes <- ssao_scene()
    cam <- camera(eye = c(0, 0, 4), center = c(0, 0, 0))

    plain <- render_scene(scene(meshes, cam, ssao_options()))
    occluded <- render_scene(scene(meshes, cam, ssao_options(ssao_enabled = TRUE)))

    a_plain <- image_to_array(plain)
    a_occluded <- image_to_array(occluded)

    # Something was actually rendered (so the comparison below is meaningful).
    expect_true(any(a_plain[, , 1] < 0.9))

    expect_true(sum(abs(a_plain - a_occluded) > 1e-6) > 0)

    # SSAO only darkens; it never brightens pixels.
    expect_true(all(a_occluded <= a_plain + 1e-6))
})

test_that("ssao_enabled = FALSE renders exactly like the default", {
    meshes <- ssao_scene()
    cam <- camera(eye = c(0, 0, 4), center = c(0, 0, 0))

    off <- render_scene(scene(meshes, cam, ssao_options()))
    def <- render_scene(scene(meshes, cam, render_options(width = 64, height = 64,
                                                          near_plane = 0.5,
                                                          far_plane = 20)))

    expect_equal(image_to_array(off), image_to_array(def))
})

test_that("SSAO works with an orthographic projection", {
    meshes <- ssao_scene()
    cam <- camera(eye = c(0, 0, 6), center = c(0, 0, 0),
                  projection = "orthographic")

    plain <- render_scene(scene(meshes, cam,
                                ssao_options(projection = "orthographic")))
    occluded <- render_scene(scene(meshes, cam,
                                   ssao_options(ssao_enabled = TRUE,
                                                projection = "orthographic")))

    # The orthographic depth mapping is linear - it has to be inverted as such.
    a_plain <- image_to_array(plain)
    expect_true(any(a_plain[, , 1] < 0.9))
    expect_true(sum(abs(a_plain - image_to_array(occluded)) > 1e-6) > 0)
})
