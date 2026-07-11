test_that("wireframe renders edges only, not filled polygons", {
    verts <- cbind(c(-2, -2, 2), c(-2, 2, 2), c(-1, -1, -1))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(c(0, 0, 10), c(0, 0, 0))

    opts <- render_options(width = 200, height = 200,
        wireframe = TRUE, backface_culling = FALSE,
        wireframe_color = c(1, 0, 0, 1),
        background_color = c(1, 1, 1, 1))

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    arr <- image_to_array(img)

    n_red <- sum(arr[, , 1] > 0.5 & arr[, , 2] < 0.1 & arr[, , 3] < 0.1)
    n_bg  <- sum(arr[, , 1] > 0.9 & arr[, , 2] > 0.9 & arr[, , 3] > 0.9)

    expect_true(n_red > 30)
    expect_true(n_bg > 39000)
})

test_that("wireframe with custom color uses that color", {
    verts <- cbind(c(-2, -2, 2), c(-2, 2, 2), c(-1, -1, -1))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(c(0, 0, 10), c(0, 0, 0))

    opts <- render_options(width = 100, height = 100,
        wireframe = TRUE, backface_culling = FALSE,
        wireframe_color = c(0, 0.5, 1, 1),
        background_color = c(0, 0, 0, 0))

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    arr <- image_to_array(img)

    non_bg <- which(arr[, , 4] > 0.01, arr.ind = TRUE)
    expect_true(nrow(non_bg) > 10)
    i <- non_bg[1, ]
    expect_true(arr[i[1], i[2], 3] > 0.4)
})

test_that("semi-transparent mesh blends with background", {
    verts <- cbind(c(-1, -1, 1, 1) * 1.5, c(-1, 1, -1, 1) * 1.5, c(0, 0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L), c(2L, 4L, 3L))
    colors <- matrix(rep(c(1, 0, 0, 0.3), 4), nrow = 4, byrow = TRUE)
    cam <- camera(c(0, 0, 10), c(0, 0, 0))

    opts <- render_options(width = 100, height = 100,
        backface_culling = FALSE, background_color = c(0, 0, 1, 1))

    img <- render_mesh(verts, tris, colors = colors, camera = cam, options = opts)
    arr <- image_to_array(img)
    mean_r <- mean(arr[, , 1])
    mean_b <- mean(arr[, , 3])

    expect_true(mean_r > 0.005 && mean_r < 0.5)
    expect_true(mean_b > 0.5)
})

test_that("fully transparent mesh shows only background", {
    verts <- cbind(c(-1, -1, 1, 1) * 1.5, c(-1, 1, -1, 1) * 1.5, c(0, 0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L), c(2L, 4L, 3L))
    colors <- matrix(rep(c(1, 0, 0, 0), 4), nrow = 4, byrow = TRUE)
    cam <- camera(c(0, 0, 10), c(0, 0, 0))

    opts <- render_options(width = 64, height = 64,
        backface_culling = FALSE, background_color = c(0, 0, 1, 1))

    img <- render_mesh(verts, tris, colors = colors, camera = cam, options = opts)
    arr <- image_to_array(img)

    mean_b <- mean(arr[, , 3])
    expect_equal(mean_b, 1.0, tolerance = 0.01)
})
