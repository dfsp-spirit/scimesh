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
