#' Apply a 4x4 transformation matrix to a mesh
#'
#' Transforms all vertex positions in a mesh by a 4x4 homogeneous
#' matrix (applied as \code{M * (x, y, z, 1)^T}).  Vertex colors are
#' kept as they are; vertex normals (if the mesh has any) are
#' transformed by the inverse transpose of \code{M}, so that shading
#' stays correct for shearing and non-uniform scaling.  Use
#' \code{compute_vertex_normals()} if the mesh has no normals yet.
#'
#' @param mesh A mesh descriptor list with \code{vertices} and
#'   \code{triangles}, as returned by \code{render_mesh()} or built
#'   by \code{scimesh_generate_multi_spheres()} etc.
#' @param matrix A 4x4 numeric matrix.
#' @return A new mesh descriptor list with transformed vertices.
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' mat <- diag(4)
#' mat[1:3, 4] <- c(2, 3, 4)
#' translated <- transform_mesh(mesh, mat)
#' translated$vertices[1, ]
#'
#' @export
transform_mesh <- function(mesh, matrix) {
    if (!is.matrix(matrix) || nrow(matrix) != 4L || ncol(matrix) != 4L) {
        stop("matrix must be a 4x4 numeric matrix")
    }
    scimesh_transform_mesh(mesh, matrix)
}

#' Translate a mesh
#'
#' Vertex colors and normals are untouched: a translation does not change
#' the orientation of a surface.
#'
#' @param mesh A mesh descriptor list.
#' @param translation Length-3 numeric vector (x, y, z).
#' @return A new mesh descriptor list with translated vertices.
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' moved <- translate_mesh(mesh, c(5, 0, 0))
#' colMeans(moved$vertices)
#'
#' @export
translate_mesh <- function(mesh, translation) {
    if (length(translation) != 3L) {
        stop("translation must be a numeric vector of length 3")
    }
    scimesh_translate_mesh(mesh, translation)
}

#' Scale a mesh uniformly or per-axis
#'
#' Per-vertex normals (if the mesh has any) are scaled as well, using the
#' inverse transpose of the scaling matrix: a non-uniform scale would
#' otherwise leave normals pointing in a direction that no longer matches
#' the surface, which shows up as wrong shading.
#'
#' @param mesh A mesh descriptor list.
#' @param scale A single numeric scale factor (uniform) or a
#'   length-3 numeric vector for per-axis scaling (x, y, z).
#' @return A new mesh descriptor list with scaled vertices.
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' big <- scale_mesh(mesh, 3)
#' flat <- scale_mesh(mesh, c(2, 0.5, 1))
#'
#' @export
scale_mesh <- function(mesh, scale) {
    if (!is.numeric(scale)) stop("scale must be numeric")
    n <- length(scale)
    if (n == 1L) {
        scimesh_scale_mesh(mesh, scale)
    } else if (n == 3L) {
        scimesh_scale_mesh_nonuniform(mesh, scale)
    } else {
        stop("scale must be a single value or a length-3 vector")
    }
}

#' Rotate a mesh around an axis
#'
#' Vertex normals (if the mesh has any) are rotated with the mesh.
#'
#' @param mesh A mesh descriptor list.
#' @param angle_rad Rotation angle in radians.
#' @param axis Length-3 numeric vector defining the rotation axis.
#' @return A new mesh descriptor list with rotated vertices.
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' rotated <- rotate_mesh(mesh, pi / 4, axis = c(0, 1, 0))
#' rotated$vertices[1, ]
#'
#' @export
rotate_mesh <- function(mesh, angle_rad, axis = c(0, 0, 1)) {
    if (length(axis) != 3L) stop("axis must be a numeric vector of length 3")
    if (length(angle_rad) != 1L) stop("angle_rad must be a single numeric value")
    scimesh_rotate_mesh(mesh, angle_rad, axis)
}

#' Set the transparency of a whole mesh
#'
#' Returns a copy of the mesh in which every vertex (and every face, if
#' per-face colors are used) has the given alpha value.  The renderer blends
#' meshes whose colors are not fully opaque automatically, so this is all that
#' is needed to draw a mesh translucently - for example a brain surface at
#' 10 percent opacity for spatial reference.
#'
#' A mesh without colors gets uniform colors first (its \code{default_color}
#' if it has one, light gray otherwise), so the mesh keeps its appearance and
#' only becomes see-through.  Use \code{alpha = 0} for completely invisible
#' geometry and \code{alpha = 1} to make a mesh opaque again.
#'
#' @param mesh A mesh descriptor list (see \code{as_scimesh_mesh}), e.g. as
#'   returned by \code{generate_sphere()} or \code{read_ply()}.
#' @param alpha Alpha value in \code{[0, 1]}: 0 = fully transparent,
#'   1 = fully opaque.
#' @return A mesh descriptor list with the alpha applied.
#'
#' @examples
#' sphere <- generate_sphere(c(0, 0, 0), 1)
#' ghost  <- set_mesh_alpha(sphere, 0.2)
#'
#' # Per-vertex alpha (here: every other vertex transparent) can be set
#' # directly on the color matrix:
#' cols <- sphere$colors
#' cols[, 4] <- rep(c(0, 1), length.out = nrow(cols))
#' sphere$colors <- cols
#'
#' @seealso \code{\link{render_mesh}}, \code{\link{scene}}
#' @export
set_mesh_alpha <- function(mesh, alpha) {
    mesh <- as_scimesh_mesh(mesh)
    if (!is.numeric(alpha) || length(alpha) != 1L || !is.finite(alpha) ||
        alpha < 0 || alpha > 1) {
        stop("alpha must be a single number in [0, 1]")
    }
    nv <- nrow(mesh$vertices)
    if (is.null(mesh$colors)) {
        base <- mesh$default_color
        if (is.null(base)) base <- c(0.7, 0.7, 0.7, 1)
        if (length(base) < 4L) base <- c(base[1:3], 1)
        mesh$colors <- matrix(rep(base[1:4], each = nv), nrow = nv)
    }
    mesh$colors[, 4] <- alpha
    if (!is.null(mesh$face_colors)) {
        mesh$face_colors[, 4] <- alpha
    }
    mesh
}

#' Flip the texture coordinates of a mesh vertically
#'
#' scimesh stores texture coordinates in \strong{image space}, with \code{v = 0}
#' at the \emph{top} edge of the texture image — the same rule as every other
#' coordinate in scimesh (\code{c(0, 0)} addresses the top-left pixel of the
#' texture image, \code{c(1, 1)} the bottom-right one).  OBJ and PLY files,
#' OpenGL, rgl and tools like Blender and MeshLab use the opposite convention
#' (\code{v = 0} at the bottom), so UVs taken from those sources have to be
#' converted once; this function does that, instead of you having to rewrite the
#' second column by hand.
#'
#' Geometry, colors and normals are untouched.  A mesh without texture
#' coordinates is returned unchanged, so calling this is safe either way.
#'
#' @param mesh A mesh descriptor (scimesh or rgl format, see
#'   \code{\link{as_scimesh_mesh}()}).
#' @return The mesh with flipped UVs.
#'
#' @examples
#' quad <- list(vertices = matrix(c(-1, -1, 0, 1, -1, 0, 1, 1, 0,
#'                                  -1, -1, 0, 1, 1, 0, -1, 1, 0),
#'                                ncol = 3, byrow = TRUE),
#'              triangles = matrix(c(1, 2, 3, 1, 3, 4), ncol = 3, byrow = TRUE),
#'              # UVs with v = 0 at the bottom (OBJ/OpenGL convention)
#'              uv = matrix(c(0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0),
#'                          ncol = 2, byrow = TRUE))
#' flipped <- flip_uvs(quad)
#' flipped$uv[, 2]  # v is now measured from the top of the texture
#'
#' @seealso \code{\link{render_mesh}} (the \code{uv} and \code{texture}
#'   arguments)
#' @export
flip_uvs <- function(mesh) {
    mesh <- as_scimesh_mesh(mesh)
    if (is.null(mesh$uv) || length(mesh$uv) == 0L) {
        return(mesh)
    }
    if (!is.matrix(mesh$uv) || ncol(mesh$uv) != 2L) {
        stop("mesh$uv must be an Nx2 numeric matrix")
    }
    mesh$uv[, 2L] <- 1 - mesh$uv[, 2L]
    mesh
}

#' Render multiple spheres from point data
#'
#' Generates a merged sphere mesh from a set of center points,
#' radii, and colors, then renders it with the given camera and
#' options.
#'
#' @param centers Nx3 numeric matrix of sphere centre coordinates.
#' @param radii Numeric vector of sphere radii (length N, or 1
#'   recycled to N).
#' @param colors Nx4 numeric matrix of RGBA colours (0-1 scale), or
#'   a single colour recycled to N.
#' @param camera A camera list from \code{camera()} or
#'   \code{camera_auto()}.
#' @param options Render options from \code{render_options()}.
#' @param segments Number of latitude/longitude segments per sphere
#'   (default 16).
#' @return An image list with \code{width}, \code{height},
#'   \code{pixels}.
#'
#' @examples
#' centers <- matrix(c(0, 2, 4, 0, 0, 0, 0, 0, 0), ncol = 3)
#' img <- render_spheres(centers, radii = 0.5,
#'                       colors = c(1, 0, 0, 1),
#'                       camera = camera_auto(centers))
#' tmp_file <- tempfile(fileext = ".png")
#' write_png(img, tmp_file)
#'
#' @export
render_spheres <- function(centers, radii, colors, camera,
                           options = render_options(),
                           segments = 16L) {
    mesh <- generate_multi_spheres(centers, radii, colors, segments = segments)
    render_scene(list(mesh), camera, options)
}

#' Render line segments as thin cylinders
#'
#' Generates a merged cylinder mesh from start/end point pairs,
#' radii, and colors, then renders it.
#'
#' @param from Nx3 numeric matrix of segment start points.
#' @param to Nx3 numeric matrix of segment end points.
#' @param radii Numeric vector of cylinder radii (length N, or 1
#'   recycled to N).
#' @param colors Nx4 numeric matrix of RGBA colours, or a single
#'   colour recycled to N.
#' @param camera A camera list.
#' @param options Render options.
#' @param segments Number of sides around the cylinder (default 12).
#' @return An image list.
#'
#' @examples
#' from <- matrix(c(0, 0, 0, 1, 1, 1), ncol = 3, byrow = TRUE)
#' to   <- matrix(c(3, 0, 0, 0, 3, 0), ncol = 3, byrow = TRUE)
#' img <- render_lines(from, to, radii = 0.05,
#'                     colors = c(0, 0, 1, 1),
#'                     camera = camera_auto(rbind(from, to)))
#' tmp_file <- tempfile(fileext = ".png")
#' write_png(img, tmp_file)
#'
#' @export
render_lines <- function(from, to, radii = 0.1, colors, camera,
                         options = render_options(),
                         segments = 12L) {
    mesh <- generate_multi_cylinders(from, to, radii = radii, colors = colors,
                                     segments = segments)
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
#' @examples
#' pts <- matrix(c(0, 1, 2, 0, 1, 2, 0, 0, 0),
#'  ncol = 3)
#' colors = matrix(c(0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1), ncol = 4)
#' img <- render_points(pts, colors = colors, radius = 5)
#' tmp_file <- tempfile(fileext = ".png")
#' write_png(img, tmp_file)
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
#' package -- any list with components \code{vb} (4xN
#' homogeneous coordinates) and \code{it} (3xM index matrix)
#' works.
#'
#' @param tmesh A list with components \code{vb} and
#'   \code{it}, as produced by \code{rgl::tmesh3d()}.
#' @return A mesh descriptor list with \code{vertices} (Nx3)
#'   and \code{triangles} (Mx3, 1-based indices).
#'
#' @examples
#' fake <- list(vb = rbind(0:3, 0:3, 0:3, rep(1, 4)),
#'              it = matrix(1:6, nrow = 3))
#' m <- mesh_from_rgl(fake)
#' m$vertices
#' m$triangles
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

# ---- internal helpers for transparent rgl interop --------------------------

#' Validate and normalize a set of 3D points
#'
#' Internal helper shared by the generator and render functions.  Accepts an
#' Nx3 numeric matrix or a single length-3 numeric vector (which is treated as
#' a single point) and returns an Nx3 numeric matrix of storage mode double,
#' as expected by the C++ layer.  An empty (0-row) matrix is allowed and means
#' "no points"; the generators then return an empty mesh.
#'
#' @param x A numeric matrix with 3 columns, or a length-3 numeric vector.
#' @param arg_name Name of the argument, used in error messages.
#' @return An Nx3 numeric matrix.
#' @keywords internal
check_points_matrix <- function(x, arg_name = "x") {
    if (is.null(x)) {
        stop(arg_name, " must not be NULL")
    }
    if (is.numeric(x) && is.null(dim(x)) && length(x) == 3L) {
        x <- matrix(as.double(x), nrow = 1L)
    }
    if (!is.matrix(x) || !is.numeric(x) || ncol(x) != 3L) {
        stop(arg_name, " must be an Nx3 numeric matrix or a length-3 numeric vector")
    }
    storage.mode(x) <- "double"
    return(x)
}

#' Recycle a per-primitive radius vector to the requested length
#'
#' A single value is applied to all primitives, a vector of the exact length is
#' used as-is, and an empty (or NULL) input means "no radii given", which the
#' C++ generators interpret as radius 1.0.
#'
#' @param radii Numeric vector of radii, or NULL.
#' @param n Number of primitives.
#' @param arg_name Name of the argument, used in error messages.
#' @return Numeric vector of length `n`, or a zero-length vector.
#' @keywords internal
recycle_radii <- function(radii, n, arg_name = "radii") {
    if (is.null(radii) || length(radii) == 0L) {
        return(numeric(0))
    }
    if (!is.numeric(radii)) {
        stop(arg_name, " must be numeric")
    }
    if (length(radii) == 1L) {
        return(rep(as.double(radii), n))
    }
    if (length(radii) != n) {
        stop(sprintf("%s must be of length 1 or %d, got %d", arg_name, n,
                     length(radii)))
    }
    return(as.double(radii))
}

#' Recycle per-primitive colors to an Nx4 matrix
#'
#' Accepts a single RGB/RGBA vector (applied to all primitives), a single-row
#' matrix (recycled), or an Nx4 (or Nx3, alpha is set to 1) matrix.  An empty
#' input means "no colors given", which the C++ generators interpret as white.
#'
#' @param colors Numeric vector or matrix of RGB/RGBA colors, or NULL.
#' @param n Number of primitives.
#' @param arg_name Name of the argument, used in error messages.
#' @return An Nx4 numeric matrix, or a 0x4 matrix.
#' @keywords internal
recycle_colors <- function(colors, n, arg_name = "colors") {
    if (is.null(colors) || length(colors) == 0L) {
        return(matrix(numeric(0), nrow = 0L, ncol = 4L))
    }
    if (is.numeric(colors) && is.null(dim(colors))) {
        if (!length(colors) %in% c(3L, 4L)) {
            stop(arg_name, " must be an RGB or RGBA vector (length 3 or 4)")
        }
        if (length(colors) == 3L) {
            colors <- c(colors, 1)
        }
        return(matrix(rep(as.double(colors), length.out = n * 4L),
                      nrow = n, byrow = TRUE))
    }
    if (!is.matrix(colors) || !is.numeric(colors) || ncol(colors) < 3L) {
        stop(arg_name, " must be an Nx4 numeric matrix or an RGB(A) vector")
    }
    colors <- as.matrix(colors)
    if (ncol(colors) == 3L) {
        colors <- cbind(colors, 1)
    }
    if (nrow(colors) == 1L) {
        colors <- colors[rep(1L, n), , drop = FALSE]
    } else if (nrow(colors) != n) {
        stop(sprintf("%s must have 1 or %d rows, got %d", arg_name, n,
                     nrow(colors)))
    }
    storage.mode(colors) <- "double"
    return(unname(colors))
}

#' Convert rgl or scimesh mesh to canonical scimesh format
#'
#' Internal helper that transparently accepts either an rgl-style mesh
#' (list with \code{vb}/\code{it}) or a scimesh mesh descriptor
#' (list with \code{vertices}/\code{triangles}) and returns the
#' canonical scimesh format.
#'
#' @param x A mesh-like object (rgl tmesh3d or scimesh mesh descriptor).
#' @return A scimesh mesh descriptor list with \code{vertices} and
#'   \code{triangles}.
#' @keywords internal
as_scimesh_mesh <- function(x) {
    if (!is.list(x)) {
        stop("Expected a mesh descriptor list, got ", class(x)[1])
    }
    # Already in scimesh format?
    if (!is.null(x$vertices) && !is.null(x$triangles)) {
        return(x)
    }
    # rgl tmesh3d format?
    if (!is.null(x$vb) && !is.null(x$it)) {
        return(mesh_from_rgl(x))
    }
    stop("Cannot interpret as mesh: expected 'vertices'/'triangles' ",
         "(scimesh format) or 'vb'/'it' (rgl format)")
}

#' Convert a scimesh mesh to rgl tmesh3d format
#'
#' Builds an rgl-compatible triangular mesh from a scimesh mesh
#' descriptor so that the result can be used with
#' \code{rgl::shade3d()} or other rgl functions.
#'
#' @param mesh A scimesh mesh descriptor list with \code{vertices}
#'   (Nx3 matrix) and \code{triangles} (Mx3 integer matrix,
#'   1-based).
#' @param color Optional per-vertex colour, either a single length-4
#'   RGBA vector (applied to all vertices) or an Nx4 matrix.
#' @param face_color Optional per-face colour (Mx4 matrix).
#' @return A list with components \code{vb} (4xN homogeneous
#'   coordinates), \code{it} (3xM 1-based index matrix), and
#'   optionally \code{normals} and \code{mat} (material), suitable
#'   for use with rgl's \code{tmesh3d()} and \code{shade3d()}.
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' rgl_mesh <- mesh_to_rgl(mesh)
#' str(rgl_mesh)
#'
#' @export
mesh_to_rgl <- function(mesh, color = NULL, face_color = NULL) {
    if (!is.list(mesh) || is.null(mesh$vertices) || is.null(mesh$triangles)) {
        stop("mesh must be a scimesh mesh descriptor with 'vertices' and 'triangles'")
    }
    verts <- mesh$vertices
    tris  <- mesh$triangles
    if (!is.matrix(verts) || ncol(verts) != 3L) {
        stop("mesh$vertices must be an Nx3 numeric matrix")
    }
    if (!is.matrix(tris) || ncol(tris) != 3L) {
        stop("mesh$triangles must be an Mx3 integer matrix")
    }

    # Build rgl-format vb: 4xN homogeneous, column-major
    nv <- nrow(verts)
    vb <- rbind(t(verts), rep(1, nv))

    # Build rgl-format it: 3xM, 1-based, column-major
    it <- t(tris)
    storage.mode(it) <- "integer"

    result <- list(vb = vb, it = it)

    # Optional normals
    if (!is.null(mesh$normals)) {
        result$normals <- t(mesh$normals)
    }

    # Optional material (colours)
    mat <- list()
    if (!is.null(face_color)) {
        if (is.vector(face_color) && length(face_color) == 4L) {
            mat$color <- matrix(face_color, nrow = 4, ncol = ncol(it))
        } else if (is.matrix(face_color)) {
            mat$color <- t(face_color)
        }
    } else if (!is.null(color)) {
        if (is.vector(color) && length(color) == 4L) {
            mat$color <- matrix(color, nrow = 4, ncol = nv)
        } else if (is.matrix(color)) {
            mat$color <- t(color)
        }
    } else if (!is.null(mesh$colors)) {
        mat$color <- t(mesh$colors)
    } else if (!is.null(mesh$face_colors)) {
        mat$color <- t(mesh$face_colors)
    }
    if (length(mat) > 0L) {
        result$mat <- mat
    }

    result
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
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 2, 0.5))
#' nrow(mesh$vertices)
#' nrow(mesh$triangles)
#'
#' @export
generate_cuboid <- function(center, half_extents, color = c(0.7, 0.7, 0.7, 1)) {
    scimesh_generate_cuboid(center, half_extents, color)
}

#' Generate a sphere mesh
#'
#' Creates a UV sphere centred at \code{center} with the given
#' \code{radius}.  The sphere is subdivided into \code{segments}
#' rings and segments per ring.
#'
#' @param center Length-3 vector: sphere centre.
#' @param radius Sphere radius.
#' @param segments Subdivision count (default 32).
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @examples
#' mesh <- generate_sphere(c(0, 0, 0), radius = 1.5, segments = 32)
#' nrow(mesh$vertices)
#'
#' @export
generate_sphere <- function(center, radius = 1, segments = 32,
                            color = c(1, 1, 1, 1)) {
    scimesh_generate_multi_spheres(
        rbind(center), radius, matrix(color, nrow = 1), segments)
}

#' Generate a cylinder mesh
#'
#' Creates a cylinder from \code{start} to \code{end} with the given
#' \code{radius}, subdivided into \code{segments} around the axis.
#' Both end caps are included unless \code{caps = FALSE} is passed.
#'
#' @param start Length-3 vector: cylinder start point.
#' @param end Length-3 vector: cylinder end point.
#' @param radius Cylinder radius.
#' @param segments Subdivision count (default 32).
#' @param color Length-4 RGBA colour.
#' @param caps Whether to close both ends with caps (default TRUE).  Pass FALSE
#'   for an open tube, which roughly halves the number of vertices and
#'   triangles.  Useful for edges whose ends are hidden by other geometry.
#' @return A mesh descriptor list.
#'
#' @examples
#' mesh <- generate_cylinder(c(0, -1, 0), c(0, 1, 0), radius = 0.5)
#' nrow(mesh$vertices)
#' open <- generate_cylinder(c(0, -1, 0), c(0, 1, 0), radius = 0.5, caps = FALSE)
#' nrow(open$vertices)
#'
#' @export
generate_cylinder <- function(start, end, radius = 0.5, segments = 32,
                              color = c(1, 1, 1, 1), caps = TRUE) {
    scimesh_generate_multi_cylinders(
        check_points_matrix(start, "start"), check_points_matrix(end, "end"),
        as.double(radius), matrix(recycle_colors(color, 1L), nrow = 1L),
        as.integer(segments), isTRUE(caps))
}

#' Generate multiple spheres as a single mesh
#'
#' Batched variant of \code{generate_sphere()}: all spheres are generated into
#' one mesh with a single vertex/triangle array, which is much faster than
#' generating and merging them one by one.  This is the function to use for
#' thousands of nodes, e.g. the nodes of a network graph or a point cloud.  The
#' returned mesh can be added to a scene and rendered with \code{render_scene()}
#' or \code{render_mesh()}.
#'
#' @param centers Nx3 numeric matrix of sphere centres (or a length-3 vector for
#'   a single sphere).
#' @param radii Numeric vector of radii (length 1, recycled; or one per sphere).
#' @param colors RGBA colour(s): a single vector applied to all spheres, or an
#'   Nx4 numeric matrix (values in \code{[0, 1]}, alpha optional).
#' @param segments Subdivision count per sphere (default 16).
#' @return A mesh descriptor list with \code{vertices}, \code{triangles} and
#'   \code{colors}.
#'
#' @examples
#' centers <- matrix(c(0, 0, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
#' mesh <- generate_multi_spheres(centers, radii = c(0.5, 0.3),
#'                                colors = c(1, 0, 0, 1), segments = 12)
#' nrow(mesh$vertices) > 0
#'
#' @seealso \code{\link{generate_sphere}}, \code{\link{generate_multi_cylinders}}
#' @export
generate_multi_spheres <- function(centers, radii = 1, colors = c(1, 1, 1, 1),
                                   segments = 16L) {
    centers <- check_points_matrix(centers, "centers")
    n <- nrow(centers)
    scimesh_generate_multi_spheres(centers, recycle_radii(radii, n),
                                   recycle_colors(colors, n),
                                   as.integer(segments))
}

#' Generate multiple cylinders as a single mesh
#'
#' Batched variant of \code{generate_cylinder()}: all cylinders are generated
#' into one mesh.  This is the function to use for thousands of straight edges,
#' e.g. the edges of a network graph or a connectome.  Pass \code{caps = FALSE}
#' to leave the ends open, which is usually what you want when the ends are
#' hidden inside spherical nodes (and roughly halves the geometry).
#'
#' @param from Nx3 numeric matrix of start points (or a length-3 vector).
#' @param to Nx3 numeric matrix of end points (same number of rows as
#'   \code{from}).
#' @param radii Numeric vector of radii (length 1, recycled; or one per
#'   cylinder).
#' @param colors RGBA colour(s): a single vector applied to all cylinders, or an
#'   Nx4 numeric matrix (values in \code{[0, 1]}, alpha optional).
#' @param segments Subdivision count around the circumference (default 12).
#' @param caps Whether to close both ends of every cylinder (default TRUE).
#' @return A mesh descriptor list with \code{vertices}, \code{triangles} and
#'   \code{colors}.
#'
#' @examples
#' from <- matrix(c(0, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE)
#' to   <- matrix(c(0, 3, 0, 1, 3, 0), ncol = 3, byrow = TRUE)
#' mesh <- generate_multi_cylinders(from, to, radii = 0.1,
#'                                  colors = c(0.7, 0.7, 0.7, 1), caps = FALSE)
#' nrow(mesh$vertices) > 0
#'
#' @seealso \code{\link{generate_cylinder}}, \code{\link{generate_tubes}}
#' @export
generate_multi_cylinders <- function(from, to, radii = 0.1,
                                     colors = c(1, 1, 1, 1),
                                     segments = 12L, caps = TRUE) {
    from <- check_points_matrix(from, "from")
    to <- check_points_matrix(to, "to")
    if (nrow(from) != nrow(to)) {
        stop(sprintf("from and to must have the same number of rows, got %d and %d",
                     nrow(from), nrow(to)))
    }
    n <- nrow(from)
    scimesh_generate_multi_cylinders(from, to, recycle_radii(radii, n),
                                     recycle_colors(colors, n),
                                     as.integer(segments), isTRUE(caps))
}

#' Generate a tube (generalized cylinder) along a path
#'
#' Sweeps a circular cross-section along the points of \code{path}, which allows
#' for curved shapes such as arcs, Bezier samples of network edges or
#' streamlines.  A path of exactly two points produces the same mesh as
#' \code{generate_cylinder()}.
#'
#' The cross-section frames are computed by parallel transport
#' (rotation-minimizing frames), so the tube does not twist around its own axis.
#' Consecutive duplicate points are removed; a path with fewer than two distinct
#' points yields an empty mesh.
#'
#' @param path Nx3 numeric matrix of path points (or a length-3 vector).
#' @param radius Tube radius (default 0.1).
#' @param segments Subdivision count around the circumference (default 12).
#' @param color Length-4 RGBA colour.
#' @param cap_start Whether to close the beginning of the tube (default TRUE).
#' @param cap_end Whether to close the end of the tube (default TRUE).
#' @return A mesh descriptor list.
#'
#' @examples
#' path <- matrix(c(0, 0, 0, 1, 1, 0, 2, 0, 0), ncol = 3, byrow = TRUE)
#' arc <- generate_tube(path, radius = 0.1, segments = 12,
#'                      cap_start = FALSE, cap_end = FALSE)
#' nrow(arc$vertices) > 0
#'
#' @seealso \code{\link{generate_tubes}}, \code{\link{generate_cylinder}}
#' @export
generate_tube <- function(path, radius = 0.1, segments = 12L,
                          color = c(1, 1, 1, 1), cap_start = TRUE,
                          cap_end = TRUE) {
    path <- check_points_matrix(path, "path")
    scimesh_generate_tube(path, as.double(radius), as.integer(segments),
                          as.double(recycle_colors(color, 1L)),
                          isTRUE(cap_start), isTRUE(cap_end))
}

#' Generate multiple tubes as a single mesh
#'
#' Batched variant of \code{generate_tube()}: all tubes are generated into one
#' mesh.  Paths may differ in length.  This is the function to use for curved
#' edges, e.g. connectome edges drawn as arcs.
#'
#' @param paths List of Nx3 numeric matrices (one per tube).  Each path needs at
#'   least two distinct points to produce geometry.
#' @param radii Numeric vector of radii (length 1, recycled; or one per tube).
#' @param colors RGBA colour(s): a single vector applied to all tubes, or an Nx4
#'   numeric matrix (values in \code{[0, 1]}, alpha optional).
#' @param segments Subdivision count around the circumference (default 12).
#' @param caps Whether to close both ends of every tube (default FALSE, since
#'   batched tubes are typically connected at their ends).
#' @return A mesh descriptor list.
#'
#' @examples
#' paths <- list(matrix(c(0, 0, 0, 1, 1, 0), ncol = 3, byrow = TRUE),
#'               matrix(c(0, 0, 2, 1, 1, 2, 2, 0, 2), ncol = 3, byrow = TRUE))
#' mesh <- generate_tubes(paths, radii = 0.05, segments = 8)
#' nrow(mesh$vertices) > 0
#'
#' @seealso \code{\link{generate_tube}}, \code{\link{generate_multi_cylinders}}
#' @export
generate_tubes <- function(paths, radii = 0.1, colors = c(1, 1, 1, 1),
                           segments = 12L, caps = FALSE) {
    if (!is.list(paths)) {
        stop("paths must be a list of Nx3 numeric matrices")
    }
    paths <- lapply(seq_along(paths), function(i) {
        check_points_matrix(paths[[i]], sprintf("paths[[%d]]", i))
    })
    n <- length(paths)
    if (n == 0L) {
        return(scimesh_generate_multi_tubes(list(), numeric(0),
                                            matrix(numeric(0), nrow = 0L, ncol = 4L),
                                            as.integer(segments), isTRUE(caps)))
    }
    scimesh_generate_multi_tubes(paths, recycle_radii(radii, n),
                                 recycle_colors(colors, n),
                                 as.integer(segments), isTRUE(caps))
}

#' Generate a cone mesh
#'
#' Creates a cone from \code{base} to \code{tip} with the given base
#' \code{radius}, subdivided into \code{segments} around the axis.
#' The base cap is included.
#'
#' @param base Length-3 vector: centre of the circular base.
#' @param tip Length-3 vector: tip of the cone.
#' @param radius Base radius.
#' @param segments Subdivision count (default 32).
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @examples
#' mesh <- generate_cone(c(0, -1, 0), c(0, 1, 0), radius = 0.8)
#' nrow(mesh$vertices)
#'
#' @export
generate_cone <- function(base, tip, radius = 0.5, segments = 32,
                          color = c(1, 1, 1, 1)) {
    scimesh_generate_cone(base, tip, radius, as.integer(segments), color)
}

#' Generate an arrow mesh
#'
#' Creates a 3D arrow from \code{from} to \code{to}, with a cylindrical
#' shaft and a conical head.
#'
#' @param from Length-3 start point.
#' @param to Length-3 end point (tip of the arrowhead).
#' @param shaft_radius Radius of the shaft cylinder.
#' @param head_radius Radius at the base of the conical head.
#' @param head_length Length of the arrowhead.
#' @param segments Subdivision count (default 32).
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @examples
#' mesh <- generate_arrow(c(0, 0, 0), c(0, 2, 0))
#' nrow(mesh$vertices)
#'
#' @export
generate_arrow <- function(from, to, shaft_radius = 0.1, head_radius = 0.3,
                           head_length = 0.6, segments = 32,
                           color = c(1, 1, 1, 1)) {
    scimesh_generate_arrow(from, to, shaft_radius, head_radius,
                           head_length, as.integer(segments), color)
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
#' @examples
#' mesh <- generate_pyramid(c(0, 0, 0), c(0, 2, 0), half_width = 1)
#' mesh$vertices
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
#' @param p0 Length-3 vector: first vertex.
#' @param p1 Length-3 vector: second vertex.
#' @param p2 Length-3 vector: third vertex.
#' @param p3 Length-3 vector: fourth vertex.
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @examples
#' mesh <- generate_tetrahedron(
#'   c(0, 0, 0), c(1, 0, 0),
#'   c(0.5, 1, 0), c(0.5, 0.5, 1))
#' nrow(mesh$vertices)
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
#' @examples
#' mesh <- generate_torus(major_radius = 2, minor_radius = 0.5)
#' nrow(mesh$vertices)
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
#' @examples
#' mesh <- generate_plane(c(0, 0, 0), normal = c(0, 1, 0),
#'                        half_size_x = 2, half_size_y = 1)
#' nrow(mesh$vertices)
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
#' @examples
#' \dontrun{
#' mesh <- read_stl("model.stl")
#' nrow(mesh$vertices)
#' }
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
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' tmp_file <- tempfile(fileext = ".stl")
#' write_stl(mesh, tmp_file, format = "binary")
#'
#' @return invisible NULL, called for side effects of writing the file.
#' @export
write_stl <- function(mesh, path, format = c("binary", "ascii")) {
    format <- match.arg(format)
    scimesh_write_stl(mesh, path, format)
    invisible(NULL)
}

#' Read a Wavefront OBJ file
#'
#' Reads the geometry (vertices and triangles) of a Wavefront OBJ file and
#' returns a scimesh mesh descriptor list with \code{vertices} and
#' \code{triangles}.  Normals and texture coordinates in the file are ignored:
#' call \code{\link{compute_vertex_normals}()} if you need normals, and assign
#' \code{uv} yourself (see \code{\link{render_mesh}}) if you want to render the
#' mesh with a texture.
#'
#' @param path Path to the OBJ file.
#' @return A mesh descriptor list with \code{vertices} and \code{triangles}.
#'
#' @examples
#' \dontrun{
#' mesh <- read_obj("model.obj")
#' nrow(mesh$vertices)
#' }
#'
#' @export
read_obj <- function(path) {
    scimesh_read_obj(path)
}

#' Read a Stanford PLY file
#'
#' Reads a PLY file (ASCII or binary) with optional per-vertex colors
#' and returns a scimesh mesh descriptor list with \code{vertices},
#' \code{triangles}, and optionally \code{colors}.  Texture coordinates in the
#' file are ignored.
#'
#' @param path Path to the PLY file.
#' @return A mesh descriptor list.
#'
#' @examples
#' \dontrun{
#' mesh <- read_ply("model.ply")
#' nrow(mesh$vertices)
#' }
#'
#' @export
read_ply <- function(path) {
    scimesh_read_ply(path)
}

#' Compute per-vertex normals for a mesh
#'
#' Computes smooth vertex normals by averaging face normals. Returns
#' the same mesh with a \code{normals} component (Nx3 numeric matrix).
#' Useful for imported meshes that lack pre-computed normals.
#'
#' @param mesh A mesh descriptor list with \code{vertices} and
#'   \code{triangles}.
#' @return The mesh with a \code{normals} component added.
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' mesh <- compute_vertex_normals(mesh)
#' nrow(mesh$normals)
#'
#' @export
compute_vertex_normals <- function(mesh) {
    scimesh_compute_vertex_normals(mesh)
}

#' Compute the axis-aligned bounding box of a mesh
#'
#' @param mesh A mesh descriptor list with \code{vertices}.
#' @return A list with \code{min} and \code{max} (each length-3 numeric).
#'
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 2, 3))
#' bb <- mesh_bbox(mesh)
#' bb$min
#' bb$max
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
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 2, 3))
#' bbox_mesh <- generate_bbox(mesh)
#' nrow(bbox_mesh$vertices)
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
#' @examples
#' axes_mesh <- generate_axes(size = 2)
#' nrow(axes_mesh$vertices)
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
