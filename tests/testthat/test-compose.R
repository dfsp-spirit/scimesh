test_that("stack_horizontal arranges two images", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 16, height = 16)

    img1 <- render_mesh(verts, tris, camera = cam, options = opts)
    img2 <- render_mesh(verts, tris, camera = cam, options = opts)

    composed <- stack_horizontal(img1, img2)
    expect_equal(composed$width, 32L)
    expect_equal(composed$height, 16L)
    expect_equal(length(composed$pixels), 32L * 16L * 4L)
})

test_that("stack_vertical arranges two images", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 16, height = 16)

    img1 <- render_mesh(verts, tris, camera = cam, options = opts)
    img2 <- render_mesh(verts, tris, camera = cam, options = opts)

    composed <- stack_vertical(img1, img2)
    expect_equal(composed$width, 16L)
    expect_equal(composed$height, 32L)
    expect_equal(length(composed$pixels), 16L * 32L * 4L)
})

test_that("stack_horizontal with colorbar appends correctly", {
    skip_if_not_installed("png")
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 16, height = 16)

    img1 <- render_mesh(verts, tris, camera = cam, options = opts)
    cbar <- colorbar_horizontal(viridis_colormap, width = 200, height = 30)

    composed <- stack_horizontal(img1, colorbar = cbar,
                                 colorbar_height = 4L)
    expect_equal(composed$width, 16L)
    expect_equal(composed$height, 20L)  # 16 + 4
})

test_that("compose_layout validates image dimensions", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))

    img1 <- render_mesh(verts, tris, camera = cam,
                        options = render_options(width = 8, height = 8))
    img2 <- render_mesh(verts, tris, camera = cam,
                        options = render_options(width = 16, height = 16))

    expect_error(
        compose_layout(list(img1, img2)),
        "dimensions"
    )
})

test_that("compose_layout with 2x2 grid", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 8, height = 8)

    images <- lapply(1:4, function(i) {
        render_mesh(verts, tris, camera = cam, options = opts)
    })

    composed <- compose_layout(images, nrow = 2, ncol = 2)
    expect_equal(composed$width, 16L)
    expect_equal(composed$height, 16L)
})

test_that("compose_layout auto layout", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 8, height = 8)

    images <- lapply(1:3, function(i) {
        render_mesh(verts, tris, camera = cam, options = opts)
    })

    composed <- compose_layout(images)
    expect_equal(composed$width, 16L)   # ceil(sqrt(3)) = 2
    expect_equal(composed$height, 16L)  # ceil(3/2) = 2
})

test_that("stack_horizontal with single image returns original", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 8, height = 8)

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    composed <- stack_horizontal(img)
    expect_equal(composed$width, 8L)
})
