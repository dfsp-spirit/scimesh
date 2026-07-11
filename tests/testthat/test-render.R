test_that("render_options returns valid defaults", {
    opts <- render_options()
    expect_type(opts, "list")
    expect_equal(opts$width, 800L)
    expect_equal(opts$height, 600L)
    expect_equal(opts$shading, "smooth")
    expect_true(opts$backface_culling)
    expect_equal(opts$background_color, c(0, 0, 0, 0))
    expect_equal(opts$default_color, c(0.7, 0.7, 0.7, 1))
    expect_false(opts$invert_normals)
    expect_false(opts$wireframe)
})

test_that("render_options validates shading", {
    expect_equal(render_options(shading = "flat")$shading, "flat")
    expect_equal(render_options(shading = "smooth")$shading, "smooth")
    expect_error(render_options(shading = "invalid"))
})

test_that("camera returns valid list", {
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    expect_type(cam, "list")
    expect_equal(cam$eye, c(0, 0, 5))
    expect_equal(cam$center, c(0, 0, 0))
    expect_equal(cam$up, c(0, 1, 0))
    expect_equal(cam$projection, "perspective")
    expect_equal(cam$fov, 45)
})

test_that("camera orthographic projection", {
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0),
                  projection = "orthographic")
    expect_equal(cam$projection, "orthographic")
})

test_that("camera_auto from vertices matrix", {
    verts <- cbind(c(-1, 1, 0, 0, 0, 0, 0, 0),
                   c(0, 0, -1, 1, 0, 0, 0, 0),
                   c(0, 0, 0, 0, -1, 1, 0, 0))
    cam <- camera_auto(verts, direction = c(0, 0, -1))
    expect_type(cam, "list")
    expect_equal(names(cam), c("eye", "center", "up", "projection", "fov"))
})

test_that("render_mesh produces valid image output", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 32, height = 32)

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    expect_type(img, "list")
    expect_equal(img$width, 32L)
    expect_equal(img$height, 32L)
    expect_true(is.raw(img$pixels))
    expect_equal(length(img$pixels), 32L * 32L * 4L)
})

test_that("render_mesh with vertex colors", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cols <- cbind(c(1, 0, 0), c(0, 1, 0), c(0, 0, 1), c(1, 1, 1))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 32, height = 32)

    img <- render_mesh(verts, tris, colors = cols, camera = cam, options = opts)
    expect_equal(img$width, 32L)
    expect_equal(img$height, 32L)
})

test_that("render_mesh with normals", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    nrms <- cbind(c(0, 0, 0), c(0, 0, 0), c(1, 1, 1))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 32, height = 32)

    img <- render_mesh(verts, tris, normals = nrms,
                       camera = cam, options = opts)
    expect_equal(img$width, 32L)
})

test_that("render_mesh validates input dimensions", {
    bad_verts <- cbind(1, 2)
    tris <- rbind(c(1L, 2L, 3L))
    expect_error(
        render_mesh(bad_verts, tris, camera = camera(eye = c(0, 0, 5),
                   center = c(0, 0, 0))),
        "3"
    )

    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    expect_error(
        render_mesh(verts, bad_verts,
                    camera = camera(eye = c(0, 0, 5), center = c(0, 0, 0))),
        "3"
    )
})

test_that("render_scene with multiple meshes", {
    verts1 <- cbind(c(-2, -1, -1.5), c(0, 0, 0), c(0, 0, 0.5))
    tris1 <- rbind(c(1L, 2L, 3L))
    verts2 <- cbind(c(1, 2, 1.5), c(0, 0, 0), c(0, 0, 0.5))
    tris2 <- rbind(c(1L, 2L, 3L))

    meshes <- list(
        list(vertices = verts1, triangles = tris1),
        list(vertices = verts2, triangles = tris2)
    )

    cam <- camera(eye = c(0, 5, 0), center = c(0, 0, 0),
                  up = c(0, 0, 1))
    opts <- render_options(width = 128, height = 64)

    img <- render_scene(meshes, cam, opts)
    expect_equal(img$width, 128L)
    expect_equal(img$height, 64L)
    expect_equal(length(img$pixels), 128L * 64L * 4L)
})

test_that("image_to_array converts correctly", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 8, height = 8)

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    arr <- image_to_array(img)

    expect_equal(dim(arr), c(8L, 8L, 4L))
    expect_true(all(arr >= 0 & arr <= 1))
})

test_that("render_mesh with changed options", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))

    opts_flat <- render_options(width = 16, height = 16, shading = "flat",
                                backface_culling = FALSE)
    img_flat <- render_mesh(verts, tris, camera = cam, options = opts_flat)
    expect_equal(img_flat$width, 16L)

    opts_wf <- render_options(width = 16, height = 16, wireframe = TRUE)
    img_wf <- render_mesh(verts, tris, camera = cam, options = opts_wf)
    expect_equal(img_wf$width, 16L)
})

test_that("render_mesh with background color", {
    verts <- cbind(c(-1, 1, -1), c(-1, -1, 1), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
    opts <- render_options(width = 8, height = 8,
                           background_color = c(1, 1, 1, 1))

    img <- render_mesh(verts, tris, camera = cam, options = opts)
    arr <- image_to_array(img)

    expect_equal(arr[1, 1, 1], 1.0)  # should be white
    expect_equal(arr[1, 1, 4], 1.0)  # fully opaque
})

test_that("specular highlight brightens surfaces facing camera", {
    verts <- cbind(c(-1, -1, 1, 1), c(-1, 1, 1, -1), c(0, 0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L), c(1L, 3L, 4L))
    cols <- matrix(rep(c(0, 0, 0, 1), 4), nrow = 4, byrow = TRUE)
    cam <- camera(c(0, 0, 5), c(0, 0, 0))

    img <- render_mesh(vertices = verts, triangles = tris,
        colors = cols, camera = cam,
        options = render_options(width = 64, height = 64,
            backface_culling = FALSE, invert_normals = TRUE,
            background_color = c(0, 0, 0, 1),
            specular_color = c(1, 1, 1, 1), shininess = 1))

    arr <- image_to_array(img)
    n_bright <- sum(arr[, , 1] > 0.01)
    expect_true(n_bright > 100)
})

test_that("specular off by default gives same result as no specular", {
    verts <- cbind(c(-1, -1), c(-1, 1), c(0, 0))
    tris <- rbind(c(1L, 2L, 1L))
    cam <- camera(c(0, 0, 5), c(0, 0, 0))
    opts <- render_options(width = 32, height = 32)

    img <- render_mesh(vertices = verts, triangles = tris,
        camera = cam, options = opts)
    expect_type(img, "list")
})

test_that("render_triangles renders raw triangle data", {
    cam <- camera(c(0, 0, 10), c(0, 0, 0))
    pos <- matrix(c(
        -1, -1, 0,  -1, 1, 0,  1, 1, 0,
        -1, -1, 0,   1, 1, 0,  1, -1, 0
    ), nrow = 6, byrow = TRUE)
    cols <- matrix(c(
        1, 0, 0, 1,  1, 0, 0, 1,  1, 0, 0, 1,
        0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1
    ), nrow = 6, byrow = TRUE)

    img <- render_triangles(pos, cols, cam,
        render_options(width = 64, height = 64,
            backface_culling = FALSE))

    expect_type(img, "list")
    expect_equal(img$width, 64)
    expect_equal(length(img$pixels), 64 * 64 * 4)
})
