test_that("translate_mesh shifts vertices", {
    verts <- cbind(c(0, 1), c(0, 1), c(0, 0))
    tris <- rbind(c(1L, 2L, 1L))
    mesh <- list(vertices = verts, triangles = tris)
    result <- translate_mesh(mesh, c(5, -3, 2))
    expect_equal(result$vertices[1, ], c(5, -3, 2))
    expect_equal(result$vertices[2, ], c(6, -2, 2))
})

test_that("scale_mesh scales vertices uniformly", {
    verts <- cbind(c(1, 2), c(3, 4), c(5, 6))
    tris <- rbind(c(1L, 2L, 1L))
    mesh <- list(vertices = verts, triangles = tris)
    result <- scale_mesh(mesh, 2.0)
    expect_equal(result$vertices[1, ], c(2, 6, 10))
    expect_equal(result$vertices[2, ], c(4, 8, 12))
})

test_that("scale_mesh preserves triangles", {
    verts <- cbind(c(0, 1, 2), c(0, 1, 2), c(0, 0, 0))
    tris <- rbind(c(1L, 2L, 3L))
    mesh <- list(vertices = verts, triangles = tris)
    result <- scale_mesh(mesh, 3.0)
    expect_equal(nrow(result$triangles), 1L)
    expect_equal(result$triangles, tris)
})

test_that("rotate_mesh rotates around Z axis by pi/2", {
    verts <- cbind(c(1, 0), c(0, 1), c(0, 0))
    tris <- rbind(c(1L, 2L, 1L))
    mesh <- list(vertices = verts, triangles = tris)
    result <- rotate_mesh(mesh, pi / 2, c(0, 0, 1))
    expect_equal(result$vertices[1, 1], 0, tolerance = 1e-6)
    expect_equal(result$vertices[1, 2], 1, tolerance = 1e-6)
    expect_equal(result$vertices[2, 1], -1, tolerance = 1e-6)
    expect_equal(result$vertices[2, 2], 0, tolerance = 1e-6)
})

test_that("transform_mesh applies 4x4 matrix", {
    verts <- cbind(c(1, 2), c(0, 0), c(0, 0))
    tris <- rbind(c(1L, 2L, 1L))
    mesh <- list(vertices = verts, triangles = tris)
    # Identity matrix
    result <- transform_mesh(mesh, diag(4))
    expect_equal(result$vertices, verts)
})

test_that("render_spheres produces valid image", {
    cam <- camera(c(0, 0, 10), c(0, 0, 0))
    centers <- matrix(c(0, 0, 0), nrow = 1)
    radii <- 1
    colors <- matrix(c(1, 0, 0, 1), nrow = 1)
    img <- render_spheres(centers, radii, colors, cam,
        render_options(width = 64, height = 64, backface_culling = FALSE))
    expect_type(img, "list")
    expect_equal(img$width, 64)
    expect_equal(img$height, 64)
    expect_equal(length(img$pixels), 64 * 64 * 4)
})

test_that("render_lines produces valid image", {
    cam <- camera(c(0, 0, 10), c(0, 0, 0))
    from <- matrix(c(0, 0, 0), nrow = 1)
    to <- matrix(c(0, 3, 0), nrow = 1)
    colors <- matrix(c(0, 1, 0, 1), nrow = 1)
    img <- render_lines(from, to, radii = 0.3, colors, cam,
        render_options(width = 64, height = 64, backface_culling = FALSE))
    expect_type(img, "list")
    expect_equal(img$width, 64)
    expect_equal(img$height, 64)
})

test_that("transform_mesh with scaling matrix works", {
    verts <- cbind(c(1, 2), c(3, 4), c(5, 6))
    tris <- rbind(c(1L, 2L, 1L))
    mesh <- list(vertices = verts, triangles = tris)
    m <- diag(c(2, 3, 4, 1))
    result <- transform_mesh(mesh, m)
    expect_equal(result$vertices[1, ], c(2, 9, 20), tolerance = 1e-6)
})
