#' Apply a 4×4 transformation matrix to a mesh
#'
#' Transforms all vertex positions in a mesh by a 4×4 homogeneous
#' matrix (applied as \code{M * (x, y, z, 1)^T}).  Vertex colors and
#' normals are untouched.
#'
#' @param mesh A mesh descriptor list with \code{vertices} and
#'   \code{triangles}, as returned by \code{render_mesh()} or built
#'   by \code{scimesh_generate_multi_spheres()} etc.
#' @param matrix A 4×4 numeric matrix.
#' @return A new mesh descriptor list with transformed vertices.
#'
#' @export
transform_mesh <- function(mesh, matrix) {
    if (!is.matrix(matrix) || nrow(matrix) != 4L || ncol(matrix) != 4L) {
        stop("matrix must be a 4×4 numeric matrix")
    }
    scimesh_transform_mesh(mesh, matrix)
}

#' Translate a mesh
#'
#' @param mesh A mesh descriptor list.
#' @param translation Length-3 numeric vector (x, y, z).
#' @return A new mesh descriptor list with translated vertices.
#'
#' @export
translate_mesh <- function(mesh, translation) {
    if (length(translation) != 3L) {
        stop("translation must be a numeric vector of length 3")
    }
    scimesh_translate_mesh(mesh, translation)
}

#' Scale a mesh uniformly
#'
#' @param mesh A mesh descriptor list.
#' @param scale A single numeric scale factor.
#' @return A new mesh descriptor list with scaled vertices.
#'
#' @export
scale_mesh <- function(mesh, scale) {
    if (length(scale) != 1L || !is.numeric(scale)) {
        stop("scale must be a single numeric value")
    }
    scimesh_scale_mesh(mesh, scale)
}

#' Rotate a mesh around an axis
#'
#' @param mesh A mesh descriptor list.
#' @param angle_rad Rotation angle in radians.
#' @param axis Length-3 numeric vector defining the rotation axis.
#' @return A new mesh descriptor list with rotated vertices.
#'
#' @export
rotate_mesh <- function(mesh, angle_rad, axis = c(0, 0, 1)) {
    if (length(axis) != 3L) stop("axis must be a numeric vector of length 3")
    if (length(angle_rad) != 1L) stop("angle_rad must be a single numeric value")
    scimesh_rotate_mesh(mesh, angle_rad, axis)
}

#' Render multiple spheres from point data
#'
#' Generates a merged sphere mesh from a set of center points,
#' radii, and colors, then renders it with the given camera and
#' options.
#'
#' @param centers N×3 numeric matrix of sphere centre coordinates.
#' @param radii Numeric vector of sphere radii (length N, or 1
#'   recycled to N).
#' @param colors N×4 numeric matrix of RGBA colours (0-1 scale), or
#'   a single colour recycled to N.
#' @param camera A camera list from \code{camera()} or
#'   \code{camera_auto()}.
#' @param options Render options from \code{render_options()}.
#' @param segments Number of latitude/longitude segments per sphere
#'   (default 16).
#' @return An image list with \code{width}, \code{height},
#'   \code{pixels}.
#'
#' @export
render_spheres <- function(centers, radii, colors, camera,
                           options = render_options(),
                           segments = 16L) {
    if (!is.matrix(centers) || ncol(centers) != 3L) {
        stop("centers must be an N×3 numeric matrix")
    }
    n <- nrow(centers)
    if (length(radii) == 1L) radii <- rep(radii, n)
    if (length(radii) != n) stop("radii must be length N or 1")
    if (is.vector(colors) && length(colors) == 4L && ncol(centers) != n) {
        colors <- matrix(colors, nrow = n, ncol = 4L, byrow = TRUE)
    }
    if (!is.matrix(colors) || nrow(colors) != n || ncol(colors) < 3L) {
        stop("colors must be an N×4 numeric matrix")
    }
    mesh <- scimesh_generate_multi_spheres(centers, radii, colors, segments)
    render_scene(list(mesh), camera, options)
}

#' Render line segments as thin cylinders
#'
#' Generates a merged cylinder mesh from start/end point pairs,
#' radii, and colors, then renders it.
#'
#' @param from N×3 numeric matrix of segment start points.
#' @param to N×3 numeric matrix of segment end points.
#' @param radii Numeric vector of cylinder radii (length N, or 1
#'   recycled to N).
#' @param colors N×4 numeric matrix of RGBA colours, or a single
#'   colour recycled to N.
#' @param camera A camera list.
#' @param options Render options.
#' @param segments Number of sides around the cylinder (default 12).
#' @return An image list.
#'
#' @export
render_lines <- function(from, to, radii = 0.1, colors, camera,
                         options = render_options(),
                         segments = 12L) {
    if (!is.matrix(from) || ncol(from) != 3L) {
        stop("from must be an N×3 numeric matrix")
    }
    if (!is.matrix(to) || ncol(to) != 3L) {
        stop("to must be an N×3 numeric matrix")
    }
    n <- nrow(from)
    if (nrow(to) != n) stop("from and to must have the same number of rows")
    if (length(radii) == 1L) radii <- rep(radii, n)
    if (length(radii) != n) stop("radii must be length N or 1")
    if (is.vector(colors) && length(colors) == 4L && ncol(from) != n) {
        colors <- matrix(colors, nrow = n, ncol = 4L, byrow = TRUE)
    }
    if (!is.matrix(colors) || nrow(colors) != n || ncol(colors) < 3L) {
        stop("colors must be an N×4 numeric matrix")
    }
    mesh <- scimesh_generate_multi_cylinders(from, to, radii, colors, segments)
    render_scene(list(mesh), camera, options)
}

#' Render screen-space point primitives
#'
#' Renders points as fixed-size filled circles in screen space with
#' depth testing.  Unlike \code{render_spheres()}, point size is
#' measured in pixels and does not change with camera distance.
#'
#' @param positions Nx3 numeric matrix of point positions.
#' @param colors Nx4 numeric matrix of RGBA colours (0-1 scale).
#' @param radius Point radius in pixels.
#' @param camera A camera list.
#' @param options Render options.
#' @return An image list.
#'
#' @export
render_points <- function(positions, colors, radius = 3,
                          camera = camera_auto(positions),
                          options = render_options()) {
    if (!is.matrix(positions) || ncol(positions) != 3L) {
        stop("positions must be an Nx3 numeric matrix")
    }
    n <- nrow(positions)
    if (!is.matrix(colors) || nrow(colors) != n || ncol(colors) < 3L) {
        stop("colors must be an Nx4 numeric matrix")
    }
    scimesh_render_points_raw(positions, colors, radius, camera, options)
}

#' Convert an rgl tmesh3d to scimesh mesh format
#'
#' Extracts vertices and triangle indices from an rgl
#' \code{tmesh3d} object into the format expected by
#' \code{render_mesh()}.  Does not require the \code{rgl}
#' package — any list with components \code{vb} (4×N
#' homogeneous coordinates) and \code{it} (3×M index matrix)
#' works.
#'
#' @param tmesh A list with components \code{vb} and
#'   \code{it}, as produced by \code{rgl::tmesh3d()}.
#' @return A mesh descriptor list with \code{vertices} (N×3)
#'   and \code{triangles} (M×3, 1-based indices).
#'
#' @export
mesh_from_rgl <- function(tmesh) {
    if (!is.list(tmesh) || is.null(tmesh$vb) || is.null(tmesh$it)) {
        stop("tmesh must be a list with 'vb' and 'it' components")
    }
    verts <- t(tmesh$vb[1:3, , drop = FALSE])
    tris  <- t(tmesh$it)
    list(vertices = verts, triangles = tris)
}

#' Generate a cuboid mesh
#'
#' Creates an axis-aligned cuboid (box) centred at \code{center}
#' with the given half-extents along each axis.
#'
#' @param center Length-3 vector: centre of the cuboid.
#' @param half_extents Length-3 vector: half-width, half-height,
#'   half-depth.
#' @param color Length-4 RGBA colour (0-1 scale).
#' @return A mesh descriptor list.
#'
#' @export
generate_cuboid <- function(center, half_extents, color = c(0.7, 0.7, 0.7, 1)) {
    scimesh_generate_cuboid(center, half_extents, color)
}

#' Generate a square pyramid mesh
#'
#' Creates a pyramid with a square base centred at \code{base_center}
#' in the XZ plane, with the apex above it along Y.
#'
#' @param base_center Length-3 vector: centre of the square base.
#' @param apex Length-3 vector: position of the tip.
#' @param half_width Half-width of the square base.
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @export
generate_pyramid <- function(base_center, apex, half_width = 1,
                             color = c(0.7, 0.7, 0.7, 1)) {
    scimesh_generate_pyramid(base_center, apex, half_width, color)
}

#' Generate a tetrahedron mesh
#'
#' Creates a tetrahedron (triangular pyramid) from four arbitrary
#' 3D points.
#'
#' @param p0, p1, p2, p3 Length-3 vectors: the four vertices.
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @export
generate_tetrahedron <- function(p0, p1, p2, p3,
                                 color = c(0.7, 0.7, 0.7, 1)) {
    scimesh_generate_tetrahedron(p0, p1, p2, p3, color)
}

#' Generate a torus mesh
#'
#' Creates a torus (donut shape) centred at \code{center}, lying in
#' the XZ plane.
#'
#' @param center Length-3 vector: centre of the torus.
#' @param major_radius Radius of the ring (tube path).
#' @param minor_radius Radius of the tube cross-section.
#' @param major_segments Number of segments around the ring.
#' @param minor_segments Number of segments around the tube.
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @export
generate_torus <- function(center = c(0, 0, 0), major_radius = 1,
                           minor_radius = 0.3, major_segments = 32,
                           minor_segments = 16,
                           color = c(0.7, 0.7, 0.7, 1)) {
    scimesh_generate_torus(center, major_radius, minor_radius,
                           major_segments, minor_segments, color)
}

#' Generate a planar quad mesh
#'
#' Creates a flat rectangular plane centred at \code{center} and
#' oriented perpendicular to \code{normal}.
#'
#' @param center Length-3 vector: centre of the plane.
#' @param normal Length-3 vector: surface normal.
#' @param half_size_x Half-extent along the first tangent axis.
#' @param half_size_y Half-extent along the second tangent axis.
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @export
generate_plane <- function(center = c(0, 0, 0), normal = c(0, 1, 0),
                           half_size_x = 1, half_size_y = 1,
                           color = c(0.7, 0.7, 0.7, 1)) {
    scimesh_generate_plane(center, normal, half_size_x, half_size_y, color)
}

#' Read an STL file
#'
#' Reads an ASCII or binary STL file and returns a scimesh mesh
#' descriptor list with \code{vertices}, \code{triangles}, and
#' \code{normals}.
#'
#' @param path Path to the STL file.
#' @return A mesh descriptor list.
#'
#' @export
read_stl <- function(path) {
    scimesh_read_stl(path)
}

#' Write a mesh to an STL file
#'
#' Writes a scimesh mesh descriptor to an ASCII or binary STL file.
#'
#' @param mesh A mesh descriptor list.
#' @param path Path to the output STL file.
#' @param format \code{"binary"} (default) or \code{"ascii"}.
#'
#' @export
write_stl <- function(mesh, path, format = c("binary", "ascii")) {
    format <- match.arg(format)
    scimesh_write_stl(mesh, path, format)
    invisible(NULL)
}

#' Compute the axis-aligned bounding box of a mesh
#'
#' @param mesh A mesh descriptor list with \code{vertices}.
#' @return A list with \code{min} and \code{max} (each length-3 numeric).
#'
#' @export
mesh_bbox <- function(mesh) {
    if (!is.list(mesh) || is.null(mesh$vertices)) {
        stop("mesh must be a list with 'vertices'")
    }
    v <- mesh$vertices
    list(
        min = apply(v, 2, min),
        max = apply(v, 2, max)
    )
}

#' Generate a wireframe bounding box mesh
#'
#' Creates 12 edge segments around an axis-aligned bounding box.
#'
#' @param bbox A bounding box list from \code{mesh_bbox()}, or a mesh
#'   descriptor (in which case \code{mesh_bbox()} is called).
#' @param color RGBA colour for the edges (length 4, 0-1 scale).
#' @param radius Cylinder radius for the edges.
#' @return A mesh descriptor list suitable for \code{render_mesh()}
#'   or inclusion in a scene list.
#'
#' @export
generate_bbox <- function(bbox, color = c(0, 0, 0, 1), radius = 0.01) {
    if (is.list(bbox) && !is.null(bbox$vertices)) {
        bbox <- mesh_bbox(bbox)
    }
    if (!is.list(bbox) || is.null(bbox$min) || is.null(bbox$max)) {
        stop("bbox must be a list with 'min' and 'max'")
    }
    bmin <- bbox$min
    bmax <- bbox$max

    corners <- matrix(c(
        bmin[1], bmin[2], bmin[3],
        bmax[1], bmin[2], bmin[3],
        bmin[1], bmax[2], bmin[3],
        bmax[1], bmax[2], bmin[3],
        bmin[1], bmin[2], bmax[3],
        bmax[1], bmin[2], bmax[3],
        bmin[1], bmax[2], bmax[3],
        bmax[1], bmax[2], bmax[3]
    ), ncol = 3, byrow = TRUE)

    edges <- matrix(c(
        0, 1, 1, 3, 3, 2, 2, 0,
        4, 5, 5, 7, 7, 6, 6, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    ), ncol = 2, byrow = TRUE)

    edge_count <- nrow(edges)
    from <- matrix(0, nrow = edge_count, ncol = 3)
    to <- matrix(0, nrow = edge_count, ncol = 3)
    for (i in seq_len(edge_count)) {
        from[i, ] <- corners[edges[i, 1] + 1L, ]
        to[i, ] <- corners[edges[i, 2] + 1L, ]
    }

    edge_cols <- matrix(color, nrow = edge_count, ncol = 4, byrow = TRUE)
    scimesh_generate_multi_cylinders(from, to, rep(radius, edge_count),
                                     edge_cols, 8L)
}

#' Generate XYZ axis arrows as cylinder meshes
#'
#' Creates three coloured arrow meshes (red X, green Y, blue Z) from
#' a centre point.
#'
#' @param center Length-3 vector: origin of the axes.
#' @param size Length of each axis.
#' @param shaft_radius Cylinder radius for axis shafts.
#' @return A mesh descriptor list suitable for \code{render_mesh()}
#'   or inclusion in a scene list.
#'
#' @export
generate_axes <- function(center = c(0, 0, 0), size = 1,
                          shaft_radius = 0.02) {
    cx <- center[1]; cy <- center[2]; cz <- center[3]
    s <- size
    r <- shaft_radius

    x_tip  <- c(cx + s, cy, cz)
    y_tip  <- c(cx, cy + s, cz)
    z_tip  <- c(cx, cy, cz + s)

    nc <- 3L
    from <- matrix(c(
        cx, cy, cz,
        cx, cy, cz,
        cx, cy, cz
    ), ncol = 3, byrow = TRUE)
    to <- matrix(c(x_tip, y_tip, z_tip), ncol = 3, byrow = TRUE)
    radii <- c(r, r, r)
    cols <- matrix(c(
        1, 0, 0, 1,
        0, 1, 0, 1,
        0, 0, 1, 1
    ), ncol = 4, byrow = TRUE)

    scimesh_generate_multi_cylinders(from, to, radii, cols, 8L)
}
