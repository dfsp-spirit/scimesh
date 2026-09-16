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

test_that("transform_mesh applies translation matrix in standard convention", {
    verts <- cbind(c(0, 1), c(0, 1), c(0, 1))
    tris <- rbind(c(1L, 2L, 1L))
    mesh <- list(vertices = verts, triangles = tris)
    # Standard row-major homogeneous matrix: +2 along X, -1 along Y
    m <- diag(1, 4)
    m[1, 4] <- 2
    m[2, 4] <- -1
    result <- transform_mesh(mesh, m)
    expect_equal(result$vertices[1, ], c(2, -1, 0), tolerance = 1e-6)
    expect_equal(result$vertices[2, ], c(3, 0, 1), tolerance = 1e-6)
})

# ---- batched primitives and tubes -------------------------------------------

test_that("generate_cylinder caps flag controls the end caps", {
    closed <- generate_cylinder(c(0, 0, 0), c(0, 3, 0), radius = 0.5,
                                segments = 8, caps = TRUE)
    open <- generate_cylinder(c(0, 0, 0), c(0, 3, 0), radius = 0.5,
                              segments = 8, caps = FALSE)

    expect_equal(nrow(closed$vertices), 4 * 8 + 2)
    expect_equal(nrow(closed$triangles), 4 * 8)
    expect_equal(nrow(open$vertices), 2 * 8)
    expect_equal(nrow(open$triangles), 2 * 8)

    # Without caps every vertex sits exactly on the lateral surface.
    axial_dist <- sqrt(open$vertices[, 1]^2 + open$vertices[, 3]^2)
    expect_equal(axial_dist, rep(0.5, nrow(open$vertices)), tolerance = 1e-5)
})

test_that("generate_multi_spheres batches spheres into a single mesh", {
    segments <- 8
    centers <- matrix(c(0, 0, 0, 2, 0, 0, 0, 2, 0), ncol = 3, byrow = TRUE)
    radii <- c(1, 0.5, 0.25)
    colors <- matrix(rep(c(1, 0, 0, 1), 3), nrow = 3, byrow = TRUE)

    batched <- generate_multi_spheres(centers, radii = radii, colors = colors,
                                      segments = segments)
    per_sphere_verts <- segments * (segments - 1) + 2
    per_sphere_tris <- 2 * segments * (segments - 1)

    expect_equal(nrow(batched$vertices), 3 * per_sphere_verts)
    expect_equal(nrow(batched$triangles), 3 * per_sphere_tris)
    expect_equal(nrow(batched$colors), 3 * per_sphere_verts)

    # A single-element call must equal the single-sphere generator.
    single <- generate_multi_spheres(matrix(c(0, 0, 0), ncol = 3), radii = 1,
                                     colors = c(1, 0, 0, 1),
                                     segments = segments)
    expect_equal(single$vertices, generate_sphere(c(0, 0, 0), 1, segments = segments)$vertices)
})

test_that("generate_multi_cylinders batches cylinders into a single mesh", {
    from <- matrix(c(0, 0, 0, 1, 0, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
    to <- matrix(c(0, 3, 0, 1, 3, 0, 2, 3, 0), ncol = 3, byrow = TRUE)
    segments <- 6

    open <- generate_multi_cylinders(from, to, radii = 0.1, segments = segments,
                                     caps = FALSE)
    closed <- generate_multi_cylinders(from, to, radii = 0.1, segments = segments,
                                       caps = TRUE)

    expect_equal(nrow(open$vertices), 3 * 2 * segments)
    expect_equal(nrow(open$triangles), 3 * 2 * segments)
    expect_equal(nrow(closed$vertices), 3 * (4 * segments + 2))
    expect_equal(nrow(closed$triangles), 3 * 4 * segments)

    # Per-cylinder radii and colors are applied.
    colored <- generate_multi_cylinders(from, to, radii = c(0.1, 0.2, 0.3),
                                        colors = c(1, 0, 0, 1), segments = 3,
                                        caps = FALSE)
    expect_equal(nrow(colored$vertices), 3 * 2 * 3)
    expect_true(all(colored$colors[, 1] == 1))
    expect_true(all(colored$colors[, 2] == 0))
})

test_that("generate_multi_cylinders and generate_multi_spheres validate input", {
    from <- matrix(c(0, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE)
    to <- matrix(c(0, 3, 0, 1, 3, 0), ncol = 3, byrow = TRUE)

    expect_error(generate_multi_cylinders(from, to[1, , drop = FALSE]),
                 "same number of rows")
    expect_error(generate_multi_cylinders(from, to, radii = c(0.1, 0.2, 0.3)),
                 "length 1 or 2")
    expect_error(generate_multi_cylinders(from, to, colors = c(1, 2)),
                 "RGB or RGBA")
    expect_error(generate_multi_cylinders(from, "nope"), "Nx3 numeric matrix")
    expect_error(generate_multi_spheres(from, radii = 1, colors = c(1, 2)),
                 "RGB or RGBA")

    # Radii/colors are recycled from single values and single-row matrices.
    a <- generate_multi_cylinders(from, to, radii = 0.1, colors = c(1, 1, 1, 1),
                                  segments = 4)
    b <- generate_multi_cylinders(from, to, radii = c(0.1, 0.1),
                                  colors = matrix(c(1, 1, 1, 1), nrow = 1),
                                  segments = 4)
    expect_equal(a$vertices, b$vertices)
    expect_equal(a$colors, b$colors)
})

test_that("generate_tube sweeps a path and matches generate_cylinder for 2 points", {
    start <- c(1, -2, 3)
    end <- c(4, 2, -1)

    straight <- generate_tube(rbind(start, end), radius = 0.4, segments = 12,
                              cap_start = TRUE, cap_end = TRUE)
    cylinder <- generate_cylinder(start, end, radius = 0.4, segments = 12,
                                  caps = TRUE)
    expect_equal(straight$vertices, cylinder$vertices)
    expect_equal(straight$normals, cylinder$normals)

    path <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0), ncol = 3, byrow = TRUE)
    segments <- 8
    k <- nrow(path)

    closed <- generate_tube(path, radius = 0.1, segments = segments)
    expect_equal(nrow(closed$vertices), k * segments + 2 * (1 + segments))
    expect_equal(nrow(closed$triangles), (k - 1) * 2 * segments + 2 * segments)

    open <- generate_tube(path, radius = 0.1, segments = segments,
                          cap_start = FALSE, cap_end = FALSE)
    expect_equal(nrow(open$vertices), k * segments)
    expect_equal(nrow(open$triangles), (k - 1) * 2 * segments)

    # Every ring vertex is at the tube radius around its path point.
    for (i in seq_len(k)) {
        ring <- open$vertices[((i - 1) * segments + 1):(i * segments), , drop = FALSE]
        dists <- sqrt(rowSums((ring - matrix(path[i, ], nrow = segments,
                                             ncol = 3, byrow = TRUE))^2))
        expect_equal(dists, rep(0.1, segments), tolerance = 1e-5)
    }
})

test_that("generate_tube handles degenerate paths", {
    expect_equal(nrow(generate_tube(matrix(numeric(0), ncol = 3), radius = 0.1)$vertices), 0)
    expect_equal(nrow(generate_tube(matrix(c(1, 1, 1), ncol = 3), radius = 0.1)$vertices), 0)

    # Consecutive duplicates are removed.
    with_duplicates <- matrix(c(0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 2),
                              ncol = 3, byrow = TRUE)
    dedup <- generate_tube(with_duplicates, radius = 0.1, segments = 8,
                           cap_start = FALSE, cap_end = FALSE)
    expect_equal(nrow(dedup$vertices), 3 * 8)
})

test_that("generate_tubes batches paths of different lengths", {
    paths <- list(matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE),
                  matrix(c(0, 0, 2, 1, 1, 2), ncol = 3, byrow = TRUE))
    segments <- 8
    mesh <- generate_tubes(paths, radii = 0.05, segments = segments,
                           caps = FALSE)

    # 3 + 2 rings of 8 vertices, and (2 + 1) segments of 2 * 8 triangles.
    expect_equal(nrow(mesh$vertices), (3 + 2) * segments)
    expect_equal(nrow(mesh$triangles), (2 + 1) * 2 * segments)

    # Curve the geometry: paths are not required to be straight.
    expect_true(all(is.finite(mesh$vertices)))
})

test_that("generate_tubes accepts empty input", {
    empty <- generate_tubes(list(), segments = 8)
    expect_equal(nrow(empty$vertices), 0)
    expect_equal(nrow(empty$triangles), 0)

    expect_error(generate_tubes("not a list"), "list of Nx3")
})

test_that("batched primitives and tubes can be rendered", {
    from <- matrix(c(0, 0, 0, 1, 1, 0, -1, 1, 0), ncol = 3, byrow = TRUE)
    to <- from + matrix(c(0, 0, 1, 0, 0, 1, 0, 0, 1), ncol = 3, byrow = TRUE)
    edges <- generate_multi_cylinders(from, to, radii = 0.05,
                                      colors = c(0.7, 0.7, 0.7, 0.5),
                                      segments = 6, caps = FALSE)
    nodes <- generate_multi_spheres(from, radii = 0.1,
                                    colors = c(1, 0, 0, 1), segments = 8)
    tube <- generate_tube(matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE),
                          radius = 0.05, segments = 8, color = c(0, 0, 1, 1))

    scene <- list(edges, nodes, tube)
    img <- render_scene(scene, camera_auto(scene),
                        render_options(width = 64, height = 64,
                                       backface_culling = FALSE))
    expect_type(img, "list")
    expect_equal(img$width, 64)
    expect_equal(img$height, 64)
    expect_equal(length(img$pixels), 64 * 64 * 4)
})
