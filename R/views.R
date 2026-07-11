#' Standard anatomical camera views for neuroimaging
#'
#' These functions compute camera parameters for standard anatomical
#' views in RAS (Right-Anterior-Superior) coordinate space, as used
#' by Freesurfer and standard neuroimaging software. Each function
#' returns a camera list suitable for \code{render_mesh()} or
#' \code{render_scene()}.
#'
#' The camera is auto-framed to fit the mesh and oriented according to
#' the view direction. In RAS space:
#' \itemize{
#'   \item +X = Right
#'   \item +Y = Anterior
#'   \item +Z = Superior
#' }
#'
#' @param mesh A mesh descriptor list with a \code{vertices} component,
#'   or an Nx3 numeric matrix of vertex positions.
#' @param fov Field of view in degrees.
#' @param margin Extra margin factor for camera distance.
#' @return A camera list with \code{eye}, \code{center}, \code{up},
#'   \code{projection}, and \code{fov} components.
#'
#' @name anatomical_views
NULL

#' @rdname anatomical_views
#' @export
view_lateral_left <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(-1, 0, 0), up = c(0, 0, 1),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_lateral_right <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(1, 0, 0), up = c(0, 0, 1),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_medial_left <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(1, 0, 0), up = c(0, 0, 1),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_medial_right <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(-1, 0, 0), up = c(0, 0, 1),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_dorsal <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(0, 0, 1), up = c(0, 1, 0),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_ventral <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(0, 0, -1), up = c(0, 1, 0),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_anterior <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(0, 1, 0), up = c(0, 0, 1),
                fov = fov, margin = margin)
}

#' @rdname anatomical_views
#' @export
view_posterior <- function(mesh, fov = 45, margin = 1.1) {
    camera_auto(mesh, direction = c(0, -1, 0), up = c(0, 0, 1),
                fov = fov, margin = margin)
}

#' Render standard neuroimaging figure layout
#'
#' Renders a hemisphere mesh from lateral and medial views and
#' composes them side by side, with an optional colorbar below.
#'
#' @param mesh A mesh descriptor list with \code{vertices},
#'   \code{triangles}, and optionally \code{colors}.
#' @param hemisphere Which hemisphere: \code{"left"} or \code{"right"}.
#' @param width Width of each sub-image.
#' @param height Height of each sub-image.
#' @param options Additional render options (passed to
#'   \code{render_options()}).
#' @param colorbar Optional colorbar array from
#'   \code{colorbar_horizontal()}.
#' @param colorbar_height Height of the colorbar in pixels.
#' @param background Background RGBA color.
#' @param fov Field of view in degrees.
#' @param margin Camera margin factor.
#' @param ... Additional arguments passed to \code{render_options()}.
#' @return A composed image list with lateral and medial views side by
#'   side, with optional colorbar.
#'
#' @export
render_hemisphere_views <- function(mesh, hemisphere = c("left", "right"),
                                     width = 800L, height = 600L,
                                     options = NULL,
                                     colorbar = NULL,
                                     colorbar_height = 80L,
                                     background = c(1, 1, 1, 1),
                                     fov = 45, margin = 1.1,
                                     ...) {
    hemisphere <- match.arg(hemisphere)

    if (is.null(options)) {
        options <- render_options(width = width, height = height,
                                  background_color = background, ...)
    } else {
        options$width <- width
        options$height <- height
        options$background_color <- background
    }

    lat_view <- if (hemisphere == "left") view_lateral_left else view_lateral_right
    med_view <- if (hemisphere == "left") view_medial_left else view_medial_right

    cam_lat <- lat_view(mesh, fov = fov, margin = margin)
    cam_med <- med_view(mesh, fov = fov, margin = margin)

    img_lat <- render_mesh(
        mesh$vertices, mesh$triangles,
        colors = mesh$colors, normals = mesh$normals,
        camera = cam_lat, options = options
    )
    img_med <- render_mesh(
        mesh$vertices, mesh$triangles,
        colors = mesh$colors, normals = mesh$normals,
        camera = cam_med, options = options
    )

    compose_layout(
        list(img_lat, img_med),
        nrow = 1L, ncol = 2L,
        colorbar = colorbar,
        colorbar_height = colorbar_height,
        background = background
    )
}
