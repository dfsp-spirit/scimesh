test_that("vis.subject.morph.native t4 sulc cortex_only", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc", views = "t4",
                                     cortex_only = TRUE,
                                     width = 200L, height = 150L)
    expect_type(img, "list")
    expect_equal(img$width, 400L)   # 2x2 * 200
    expect_equal(img$height, 300L)  # 2x2 * 150
    expect_equal(length(img$pixels), 400L * 300L * 4L)
})

test_that("vis.subject.morph.native single view with colorbar", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc",
                                     views = "lateral_lh",
                                     cortex_only = TRUE,
                                     draw_colorbar = TRUE,
                                     width = 200L, height = 150L)
    expect_equal(img$width, 200L)
    expect_equal(img$height, 230L)  # 150 + 80 (colorbar)
    expect_equal(length(img$pixels), 200L * 230L * 4L)
})

test_that("vis.subject.morph.native lh only", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc", hemi = "lh",
                                     views = "lateral_lh",
                                     width = 200L, height = 150L)
    expect_type(img, "list")
    expect_equal(img$width, 200L)
    expect_equal(img$height, 150L)
})

test_that("vis.subject.morph.native rh only", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "rh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc", hemi = "rh",
                                     views = "lateral_rh",
                                     width = 200L, height = 150L)
    expect_type(img, "list")
    expect_equal(img$width, 200L)
    expect_equal(img$height, 150L)
})

test_that("vis.subject.morph.native without cortex_only", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc",
                                     views = "lateral_lh",
                                     cortex_only = FALSE,
                                     width = 200L, height = 150L)
    expect_type(img, "list")
})

test_that("vis.subject.morph.native view name aliases", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    for (v in c("sd_lateral_lh", "si_lateral_rh", "sr_medial_lh",
                "dorsal", "ventral", "anterior", "posterior")) {
        img <- vis.subject.morph.native(sjd, sj, "sulc",
                                         views = v, width = 100L,
                                         height = 75L)
        expect_type(img, "list")
    }
})

test_that("vis.subject.morph.native with custom colormap", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc",
                                     views = "lateral_lh",
                                     cortex_only = TRUE,
                                     makecmap_options = list(
                                         colFn = viridis_colormap,
                                         n = 128L,
                                         range = c(-10, 10),
                                         symm = FALSE
                                     ),
                                     width = 200L, height = 150L)
    expect_type(img, "list")
})

test_that("vis.subject.morph.native t9 view", {
    skip_if_not_installed("freesurferformats")
    sjd <- file.path("..", "..", "test_data", "freesurfer", "subjects_dir")
    sj  <- "subject1"
    if (!file.exists(file.path(sjd, sj, "surf", "lh.sulc"))) {
        skip("Test FreeSurfer data not found")
    }

    img <- vis.subject.morph.native(sjd, sj, "sulc",
                                     views = "t9",
                                     width = 100L, height = 75L)
    expect_type(img, "list")
    # t9 = 9 views, 3x3 grid
    expect_equal(img$width, 300L)
    expect_equal(img$height, 225L)
})

test_that("vis.subject.morph.native errors on missing data", {
    skip_if_not_installed("freesurferformats")
    expect_error(
        vis.subject.morph.native("/nonexistent", "subject1", "sulc",
                                  views = "lateral_lh"),
        "not found"
    )
})
