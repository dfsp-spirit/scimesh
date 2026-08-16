test_that("scene() builds a scimesh_scene and print works", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
    tr <- diag(1, 4); tr[1, 4] <- 2
    sc <- scene(list(cube, list(mesh = cube, transform = tr, name = "b")),
                camera = camera(c(0, 0, 8), c(1, 0, 0)))
    expect_s3_class(sc, "scimesh_scene")
    expect_length(sc$meshes, 2)
    expect_equal(sc$meshes[[2]]$name, "b")
    expect_null(sc$meshes[[1]]$transform)
    out <- capture.output(print(sc))
    expect_true(any(grepl("2 mesh", out)))
})

test_that("scene() accepts a transforms/names override list", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
    tr <- diag(1, 4); tr[1, 4] <- 3
    sc <- scene(list(cube, cube), transforms = list(NULL, tr), names = c("a", "b"))
    expect_null(sc$meshes[[1]]$transform)
    expect_equal(sc$meshes[[2]]$transform, tr)
    expect_equal(sc$meshes[[2]]$name, "b")
})

test_that("render_scene accepts a scene object and equals baked render", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
    tr <- diag(1, 4); tr[1, 4] <- 2
    cam <- camera(c(0, 0, 8), c(1, 0, 0))
    opts <- render_options(width = 64, height = 64, backface_culling = FALSE, threads = 1L)
    sc <- scene(list(cube, list(mesh = cube, transform = tr)), camera = cam, options = opts)
    img1 <- render_scene(sc)
    img2 <- render_scene(list(cube, transform_mesh(cube, tr)), cam, opts)
    expect_identical(img1$pixels, img2$pixels)
})

test_that("render_scene still accepts a plain mesh list", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
    cam <- camera(c(0, 0, 8), c(0, 0, 0))
    opts <- render_options(width = 32, height = 32, threads = 1L)
    img <- render_scene(list(cube), cam, opts)
    expect_type(img, "list")
    expect_equal(img$width, 32)
    expect_equal(img$height, 32)
})

test_that("write_gltf writes .glb and .gltf with colors, names, transforms", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
    tr <- diag(1, 4); tr[1, 4] <- 2
    sc <- scene(list(cube, list(mesh = cube, transform = tr, name = "b")))

    f_glb <- tempfile(fileext = ".glb")
    write_gltf(sc, f_glb, format = "glb")
    expect_true(file.exists(f_glb))
    expect_gt(file.info(f_glb)$size, 20)
    rawb <- readBin(f_glb, "raw", n = 4)
    expect_equal(rawToChar(rawb), "glTF")

    f_gltf <- tempfile(fileext = ".gltf")
    write_gltf(sc, f_gltf)
    expect_true(file.exists(f_gltf))
    expect_true(file.exists(sub("\\.gltf$", ".bin", f_gltf)))
    json <- paste(readLines(f_gltf), collapse = "\n")
    expect_true(grepl('COLOR_0', json, fixed = TRUE))
    expect_true(grepl('"name"', json, fixed = TRUE))
    expect_true(grepl('"matrix"', json, fixed = TRUE))
})

test_that("write_gltf with camera exports a camera node", {
    cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
    cam <- camera(c(0, 0, 8), c(0, 0, 0))
    f <- tempfile(fileext = ".glb")
    write_gltf(list(cube), f, camera = cam, format = "glb")
    expect_true(file.exists(f))
})
