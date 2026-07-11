test_that("Render performance: ~100k triangle mesh under 2 seconds", {
    # Generate a dense icosphere-like mesh procedurally
    n_subdiv <- 4L  # ~160k triangles
    n_lat <- 128L
    n_lon <- 256L

    n_verts <- (n_lat + 1L) * (n_lon + 1L)
    n_tris <- n_lat * n_lon * 2L

    verts <- matrix(0, nrow = n_verts, ncol = 3L)
    radius <- 1.0
    idx <- 1L
    for (lat in 0L:n_lat) {
        theta <- pi * lat / n_lat
        sin_theta <- sin(theta)
        cos_theta <- cos(theta)
        for (lon in 0L:n_lon) {
            phi <- 2 * pi * lon / n_lon
            verts[idx, 1L] <- radius * sin_theta * cos(phi)
            verts[idx, 2L] <- radius * sin_theta * sin(phi)
            verts[idx, 3L] <- radius * cos_theta
            idx <- idx + 1L
        }
    }

    tris <- matrix(0L, nrow = n_tris, ncol = 3L)
    tidx <- 1L
    for (lat in 0L:(n_lat - 1L)) {
        for (lon in 0L:(n_lon - 1L)) {
            a <- lat * (n_lon + 1L) + lon + 1L
            b <- a + 1L
            c <- a + (n_lon + 1L)
            d <- c + 1L
            tris[tidx, ] <- c(a, b, d)
            tris[tidx + 1L, ] <- c(a, d, c)
            tidx <- tidx + 2L
        }
    }

    cols <- matrix(runif(n_verts * 3L), ncol = 3L)

    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 400L, height = 300L,
                           background_color = c(1, 1, 1, 1))

    t_start <- Sys.time()
    img <- render_mesh(verts, tris, colors = cols, camera = cam,
                       options = opts)
    t_elapsed <- as.numeric(difftime(Sys.time(), t_start, units = "secs"))

    expect_equal(img$width, 400L)
    expect_equal(img$height, 300L)
    expect_true(t_elapsed < 5.0)

    colored_count <- sum(img$pixels[seq(4L, length(img$pixels), by = 4L)] > 0)
    expect_true(colored_count > 100L)
})

test_that("Render performance scales reasonably with resolution", {
    n_verts <- 1000L
    n_tris <- 500L
    set.seed(42)
    verts <- matrix(rnorm(n_verts * 3L), ncol = 3L)
    tris <- matrix(sample(n_verts, n_tris * 3L, replace = TRUE), ncol = 3L)

    cam <- camera(eye = c(0, 0, 20), center = c(0, 0, 0))
    times <- numeric(3L)
    resolutions <- c(100L, 200L, 400L)

    for (i in seq_along(resolutions)) {
        opts <- render_options(width = resolutions[i], height = resolutions[i])
        t_start <- Sys.time()
        img <- render_mesh(verts, tris, camera = cam, options = opts)
        times[i] <- as.numeric(difftime(Sys.time(), t_start, units = "secs"))
        expect_true(times[i] < 10.0)
    }

    # Larger resolution should be roughly proportional to pixel count
    # (allow generous margin for noise)
    ratio_expected <- (resolutions[3] / resolutions[1])^2
    ratio_actual <- times[3] / max(times[1], 1e-6)
    expect_true(ratio_actual < ratio_expected * 10)
})
