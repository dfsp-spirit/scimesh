#' Create a camera specification
#'
#' Defines a camera for rendering by specifying the eye position,
#' look-at center, up vector, projection type, and field of view.
#'
#' @param eye Numeric vector of length 3: camera position.
#' @param center Numeric vector of length 3: point the camera looks at.
#' @param up Numeric vector of length 3: camera up direction.
#' @param projection Projection type: \code{"perspective"} (default)
#'   or \code{"orthographic"}.
#' @param fov Field of view in degrees (perspective only).
#' @return A camera list suitable for \code{render_mesh()} or
#'   \code{render_scene()}.
#'
#' @export
camera <- function(eye, center, up = c(0, 1, 0),
                   projection = c("perspective", "orthographic"),
                   fov = 45) {
    projection <- match.arg(projection)
    list(
        eye = as.numeric(eye),
        center = as.numeric(center),
        up = as.numeric(up),
        projection = projection,
        fov = as.numeric(fov)
    )
}

#' Auto-frame a camera to fit a mesh or vertex set
#'
#' Computes a camera position that frames the entire mesh in view.
#' The camera looks along the given direction, positioned at a distance
#' that ensures the mesh fits within the field of view.
#'
#' @param mesh Either an Nx3 numeric matrix of vertex positions, or a
#'   mesh descriptor list with a \code{vertices} component.
#' @param direction The view direction as a length-3 vector. For
#'   example, \code{c(0, 0, -1)} looks along the negative Z axis.
#' @param up The up vector as a length-3 vector. Default \code{c(0, 1, 0)}.
#' @param fov Field of view in degrees.
#' @param margin Extra margin factor (1.0 = tight fit, 1.1 = 10% margin).
#' @return A camera list.
#'
#' @export
camera_auto <- function(mesh, direction = c(0, 0, -1), up = c(0, 1, 0),
                        fov = 45, margin = 1.1) {
    if (is.list(mesh) && !is.null(mesh$vertices)) {
        mesh_data <- mesh
    } else if (is.matrix(mesh) && ncol(mesh) == 3L) {
        mesh_data <- list(
            vertices = mesh,
            triangles = matrix(integer(0), nrow = 0, ncol = 3)
        )
    } else {
        stop(
            "mesh must be an Nx3 matrix of vertices or a mesh descriptor list"
        )
    }

    scimesh_camera_fit_mesh(mesh_data, direction, up, fov, margin)
}
