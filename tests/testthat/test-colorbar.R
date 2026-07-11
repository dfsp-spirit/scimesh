test_that("colorbar_horizontal creates valid array", {
    skip_if_not_installed("png")
    cbar <- colorbar_horizontal(viridis_colormap, n_colors = 16,
                                 width = 200, height = 40,
                                 ticks = c(0, 0.5, 1),
                                 tick_labels = c("0", "0.5", "1"))
    expect_equal(length(dim(cbar)), 3L)
    expect_equal(dim(cbar)[3], 4L)
    expect_true(all(cbar >= 0 & cbar <= 1))
})

test_that("colorbar_horizontal works with color vector", {
    skip_if_not_installed("png")
    cbar <- colorbar_horizontal(c("red", "blue"), n_colors = 8,
                                 width = 200, height = 30)
    expect_equal(dim(cbar)[3], 4L)
})

test_that("colorbar_horizontal works with color function", {
    skip_if_not_installed("png")
    cbar <- colorbar_horizontal(
        function(n) grDevices::hcl.colors(n, palette = "Reds"),
        n_colors = 8, width = 200, height = 30
    )
    expect_equal(dim(cbar)[3], 4L)
})

test_that("colorbar_horizontal without ticks", {
    skip_if_not_installed("png")
    cbar <- colorbar_horizontal(viridis_colormap, width = 200,
                                 height = 30)
    expect_equal(length(dim(cbar)), 3L)
})

test_that("colorbar_horizontal with custom data_range", {
    skip_if_not_installed("png")
    cbar <- colorbar_horizontal(viridis_colormap,
                                 width = 200, height = 40,
                                 ticks = c(-2, 0, 2),
                                 data_range = c(-3, 3))
    expect_equal(dim(cbar)[3], 4L)
})

test_that("colorbar_vertical creates valid array", {
    skip_if_not_installed("png")
    cbar <- colorbar_vertical(viridis_colormap, n_colors = 16,
                               width = 40, height = 200,
                               ticks = c(0, 0.5, 1),
                               tick_labels = c("0", "0.5", "1"))
    expect_equal(length(dim(cbar)), 3L)
    expect_equal(dim(cbar)[3], 4L)
})

test_that("viridis_colormap returns correct number of colors", {
    cols <- viridis_colormap(10)
    expect_length(cols, 10L)
    expect_true(all(grepl("^#", cols)))
})

test_that("viridis_colormap respects direction", {
    fwd <- viridis_colormap(4, direction = 1)
    rev <- viridis_colormap(4, direction = -1)
    expect_equal(fwd[1], rev[4])
})

test_that("diverging_colormap returns correct number of colors", {
    cols <- diverging_colormap(10)
    expect_length(cols, 10L)
    expect_true(all(grepl("^#", cols)))
})

test_that("ensure_rgba_array converts 3-channel to 4-channel", {
    arr3 <- array(0.5, dim = c(10, 10, 3))
    arr4 <- ensure_rgba_array(arr3)
    expect_equal(dim(arr4), c(10L, 10L, 4L))
    expect_equal(arr4[, , 1], arr3[, , 1])
    expect_equal(arr4[, , 4], array(1, dim = c(10L, 10L)))
})

test_that("ensure_rgba_array passes through 4-channel arrays", {
    arr4 <- array(0.5, dim = c(10, 10, 4))
    result <- ensure_rgba_array(arr4)
    expect_equal(dim(result), c(10L, 10L, 4L))
})
