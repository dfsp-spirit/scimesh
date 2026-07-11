#' Render a 3D mesh to an image
#'
#' Renders a single mesh using the scimesh software renderer.
#'
#' @param vertices Nx3 numeric matrix of vertex positions.
#' @param triangles Mx3 integer matrix of triangle indices (1-based).
#' @param colors Optional Nx4 numeric matrix of RGBA vertex colors (0-1).
#' @param normals Optional Nx3 numeric matrix of vertex normals.
#' @param camera A camera list from \code{camera()} or \code{camera_auto()}.
#' @param options A render options list from \code{render_options()}.
#' @return A list with components \code{width}, \code{height}, and
#'   \code{pixels} (raw vector of RGBA values).
#'
#' @export
render_mesh <- function(vertices, triangles, colors = NULL, normals = NULL,
                        camera = camera_auto(vertices),
                        options = render_options()) {
    if (!is.matrix(vertices) || ncol(vertices) != 3L) {
        stop("vertices must be an Nx3 numeric matrix")
    }
    if (!is.matrix(triangles) || ncol(triangles) != 3L) {
        stop("triangles must be an Mx3 integer matrix")
    }

    mesh <- list(
        vertices = vertices,
        triangles = triangles
    )
    if (!is.null(colors)) {
        if (!is.matrix(colors) || ncol(colors) < 3L || ncol(colors) > 4L) {
            stop("colors must be an Nx3 or Nx4 numeric matrix")
        }
        mesh$colors <- colors
    }
    if (!is.null(normals)) {
        if (!is.matrix(normals) || ncol(normals) != 3L) {
            stop("normals must be an Nx3 numeric matrix")
        }
        mesh$normals <- normals
    }

    scimesh_render_mesh(mesh, camera, options)
}

#' Render multiple meshes to an image
#'
#' Renders a list of meshes as a single scene using the scimesh
#' software renderer.
#'
#' @param meshes A list of mesh descriptors. Each element is a list
#'   with components \code{vertices} (Nx3 matrix), \code{triangles}
#'   (Mx3 integer matrix), and optionally \code{colors}, \code{normals},
#'   and \code{default_color}.
#' @param camera A camera list from \code{camera()} or \code{camera_auto()}.
#' @param options A render options list from \code{render_options()}.
#' @return A list with components \code{width}, \code{height}, and
#'   \code{pixels} (raw vector of RGBA values).
#'
#' @export
render_scene <- function(meshes, camera, options = render_options()) {
    if (!is.list(meshes)) {
        stop("meshes must be a list of mesh descriptors")
    }
    for (i in seq_along(meshes)) {
        m <- meshes[[i]]
        if (!is.list(m) || is.null(m$vertices) || is.null(m$triangles)) {
            stop("each mesh must be a list with 'vertices' and 'triangles'")
        }
    }

    scimesh_render_scene(meshes, camera, options)
}

#' Create render options
#'
#' @param width Output image width in pixels.
#' @param height Output image height in pixels.
#' @param shading Shading mode: \code{"smooth"} or \code{"flat"}.
#' @param backface_culling Whether to cull back-facing triangles.
#' @param background_color Background RGBA color as numeric vector of
#'   length 4 (values 0-1).
#' @param default_color Default vertex color when no colors are provided.
#' @param invert_normals Whether to invert surface normals.
#' @param wireframe Whether to render in wireframe mode.
#' @param wireframe_color RGBA color for wireframe edges (0-1
#'   scale).  Default \code{c(0, 0, 0, 1)} (black).
#' @param projection Projection type: \code{"perspective"} (default)
#'   or \code{"orthographic"}.  Orthographic gives a parallel
#'   projection (no perspective foreshortening), matching rgl's
#'   \code{view3d(fov=0)} convention.
#' @param specular_color Specular highlight color (0-1 scale).  When
#'   \code{shininess > 0}, a Blinn-Phong highlight in this colour is
#'   added where the surface faces the camera.  Default
#'   \code{c(0, 0, 0, 0)} (off).
#' @param shininess Specular exponent controlling highlight sharpness.
#'   Higher values produce a tighter spot.  Typical values: 32 (soft
#'   plastic), 64 (shiny), 128 (glass).  Default \code{0} (off).
#' @param aa_samples Anti-aliasing supersampling factor.  Renders
#'   internally at \code{width * aa_samples} x
#'   \code{height * aa_samples}, then downsamples to the requested
#'   size via box averaging.  Default \code{1} (no AA), \code{2} for
#'   2×2 SSAA, \code{4} for 4×4.
#' @return A render options list for use with \code{render_mesh()} or
#'   \code{render_scene()}.
#'
#' @export
render_options <- function(width = 800L, height = 600L,
                           shading = c("smooth", "flat"),
                           backface_culling = TRUE,
                           background_color = c(0, 0, 0, 0),
                           default_color = c(0.7, 0.7, 0.7, 1),
                           invert_normals = FALSE,
                           wireframe = FALSE,
                           wireframe_color = c(0, 0, 0, 1),
                           projection = c("perspective", "orthographic"),
                           specular_color = c(0, 0, 0, 0),
                           shininess = 0,
                           aa_samples = 1L) {
    shading <- match.arg(shading)
    list(
        width = as.integer(width),
        height = as.integer(height),
        shading = shading,
        backface_culling = isTRUE(backface_culling),
        background_color = as.numeric(background_color),
        default_color = as.numeric(default_color),
        invert_normals = isTRUE(invert_normals),
        wireframe = isTRUE(wireframe),
        wireframe_color = as.numeric(wireframe_color),
        projection = match.arg(projection),
        specular_color = as.numeric(specular_color),
        shininess = as.numeric(shininess),
        aa_samples = as.integer(aa_samples)
    )
}

#' Render raw triangles without index buffer
#'
#' Renders triangle geometry where positions and colours are given
#' as flat arrays with 3 vertices per triangle (no index buffer).
#' Useful for voxel renderings, misc3d isosurfaces, and other
#' dynamically generated geometry.
#'
#' @param positions N×3 numeric matrix of vertex positions, where N
#'   is a multiple of 3 (3 per triangle).
#' @param colors N×4 numeric matrix of RGBA colours (0-1 scale).
#' @param camera A camera list from \code{camera()} or
#'   \code{camera_auto()}.
#' @param options Render options from \code{render_options()}.
#' @return An image list with \code{width}, \code{height},
#'   \code{pixels}.
#'
#' @export
render_triangles <- function(positions, colors, camera,
                             options = render_options()) {
    if (!is.matrix(positions) || ncol(positions) != 3L) {
        stop("positions must be an Nx3 numeric matrix")
    }
    n <- nrow(positions)
    if (n %% 3L != 0L) {
        stop("positions must have a multiple of 3 rows")
    }
    if (!is.matrix(colors) || nrow(colors) != n || ncol(colors) < 3L) {
        stop("colors must be an Nx4 numeric matrix matching positions")
    }
    scimesh_render_triangles_raw(positions, colors, camera, options)
}
