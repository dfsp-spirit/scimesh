#' Apply a 4x4 transformation matrix to a mesh
#'
#' Transforms all vertex positions in a mesh by a 4x4 homogeneous
#' matrix (applied as \code{M * (x, y, z, 1)^T}).  Vertex colors and
#' normals are untouched.
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
#' \dontrun{ write_png(img, "spheres.png") }
#'
#' @export
render_spheres <- function(centers, radii, colors, camera,
                           options = render_options(),
                           segments = 16L) {
    if (!is.matrix(centers) || ncol(centers) != 3L) {
        stop("centers must be an Nx3 numeric matrix")
    }
    n <- nrow(centers)
    if (length(radii) == 1L) radii <- rep(radii, n)
    if (length(radii) != n) stop("radii must be length N or 1")
    if (is.vector(colors) && length(colors) %in% c(3L, 4L)) {
        if (length(colors) == 3L) colors <- c(colors, 1)
        colors <- matrix(colors, nrow = n, ncol = 4L, byrow = TRUE)
    }
    if (!is.matrix(colors) || nrow(colors) != n || ncol(colors) < 3L) {
        stop("colors must be an Nx4 numeric matrix")
    }
    mesh <- scimesh_generate_multi_spheres(centers, radii, colors, segments)
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
#' \dontrun{ write_png(img, "lines.png") }
#'
#' @export
render_lines <- function(from, to, radii = 0.1, colors, camera,
                         options = render_options(),
                         segments = 12L) {
    if (!is.matrix(from) || ncol(from) != 3L) {
        stop("from must be an Nx3 numeric matrix")
    }
    if (!is.matrix(to) || ncol(to) != 3L) {
        stop("to must be an Nx3 numeric matrix")
    }
    n <- nrow(from)
    if (nrow(to) != n) stop("from and to must have the same number of rows")
    if (length(radii) == 1L) radii <- rep(radii, n)
    if (length(radii) != n) stop("radii must be length N or 1")
    if (is.vector(colors) && length(colors) %in% c(3L, 4L)) {
        if (length(colors) == 3L) colors <- c(colors, 1)
        colors <- matrix(colors, nrow = n, ncol = 4L, byrow = TRUE)
    }
    if (!is.matrix(colors) || nrow(colors) != n || ncol(colors) < 3L) {
        stop("colors must be an Nx4 numeric matrix")
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
#' @examples
#' pts <- matrix(c(0, 1, 2, 0, 1, 2, 0, 0, 0),
#'  ncol = 3)
#' colors = matrix(c(0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1), ncol = 4)
#' img <- render_points(pts, colors = colors, radius = 5)
#' \dontrun{ write_png(img, "points.png") }
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
#' Both end caps are included.
#'
#' @param start Length-3 vector: cylinder start point.
#' @param end Length-3 vector: cylinder end point.
#' @param radius Cylinder radius.
#' @param segments Subdivision count (default 32).
#' @param color Length-4 RGBA colour.
#' @return A mesh descriptor list.
#'
#' @examples
#' mesh <- generate_cylinder(c(0, -1, 0), c(0, 1, 0), radius = 0.5)
#' nrow(mesh$vertices)
#'
#' @export
generate_cylinder <- function(start, end, radius = 0.5, segments = 32,
                              color = c(1, 1, 1, 1)) {
    scimesh_generate_multi_cylinders(
        rbind(start), rbind(end), radius, matrix(color, nrow = 1), segments)
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
#' \dontrun{
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' write_stl(mesh, "output.stl")
#' }
#'
#' @export
write_stl <- function(mesh, path, format = c("binary", "ascii")) {
    format <- match.arg(format)
    scimesh_write_stl(mesh, path, format)
    invisible(NULL)
}

#' Read a Wavefront OBJ file
#'
#' Reads a Wavefront OBJ file (with optional UV coordinates and normals)
#' and returns a scimesh mesh descriptor list with \code{vertices},
#' \code{triangles}, and optionally \code{uv} and \code{normals}.
#'
#' @param path Path to the OBJ file.
#' @return A mesh descriptor list.
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
#' \code{triangles}, and optionally \code{colors}.
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
