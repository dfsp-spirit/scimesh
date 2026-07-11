test_that("view_lateral_left returns valid camera", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- view_lateral_left(verts)
    expect_type(cam, "list")
    expect_equal(names(cam), c("eye", "center", "up", "projection", "fov"))
    expect_equal(cam$projection, "perspective")
    expect_equal(cam$up, c(0, 0, 1))
})

test_that("view_lateral_right returns valid camera", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- view_lateral_right(verts)
    expect_equal(cam$up, c(0, 0, 1))
})

test_that("view_dorsal returns valid camera", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- view_dorsal(verts)
    expect_equal(cam$up, c(0, 1, 0))
})

test_that("view_ventral returns valid camera", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- view_ventral(verts)
    expect_equal(cam$up, c(0, 1, 0))
})

test_that("view_anterior returns valid camera", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- view_anterior(verts)
    expect_equal(cam$up, c(0, 0, 1))
})

test_that("view_posterior returns valid camera", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- view_posterior(verts)
    expect_equal(cam$up, c(0, 0, 1))
})

test_that("all anatomical view cameras produce usable output", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    opts <- render_options(width = 16, height = 16)

    views <- list(
        view_lateral_left, view_lateral_right,
        view_medial_left, view_medial_right,
        view_dorsal, view_ventral,
        view_anterior, view_posterior
    )

    for (v in views) {
        cam <- v(verts)
        img <- render_mesh(verts, tris, camera = cam, options = opts)
        expect_equal(img$width, 16L)
        expect_equal(img$height, 16L)
        expect_equal(length(img$pixels), 16L * 16L * 4L)
    }
})

test_that("render_hemisphere_views produces lateral+medial layout", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    mesh <- list(vertices = verts, triangles = tris)

    result <- render_hemisphere_views(mesh, hemisphere = "left",
                                       width = 16, height = 16)
    expect_equal(result$width, 32L)
    expect_equal(result$height, 16L)
    expect_equal(length(result$pixels), 32L * 16L * 4L)
})

test_that("render_hemisphere_views with colorbar", {
    skip_if_not_installed("png")
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    mesh <- list(vertices = verts, triangles = tris)

    cbar <- colorbar_horizontal(viridis_colormap, width = 200, height = 30)

    result <- render_hemisphere_views(mesh, hemisphere = "left",
                                       width = 16, height = 16,
                                       colorbar = cbar,
                                       colorbar_height = 4L)
    expect_equal(result$width, 32L)
    expect_equal(result$height, 20L)  # 16 + 4
})
