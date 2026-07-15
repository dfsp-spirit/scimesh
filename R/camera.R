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
#' @examples
#' cam <- camera(eye = c(0, 0, 5), center = c(0, 0, 0))
#' cam$eye
#'
#' @export
camera <- function(eye, center, up = c(0, 1, 0),
                   projection = c("perspective", "orthographic"),
                   fov = 45) {
    projection <- match.arg(projection)
    structure(list(
        eye = as.numeric(eye),
        center = as.numeric(center),
        up = as.numeric(up),
        projection = projection,
        fov = as.numeric(fov)
    ), class = "scimesh_camera")
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
#' @param margin Extra margin factor (1.0 = tight fit, 1.1 = 10\% margin).
#' @param projection Projection type: \code{"perspective"} (default) or
#'   \code{"orthographic"}. When orthographic, the camera distance is
#'   computed to tightly frame the mesh regardless of FOV.
#' @return A camera list, with S3 class \code{"scimesh_camera"}.
#'
#' @examples
#' verts <- matrix(c(-1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
#'                    -1,-1, 1, 1,-1, 1, 1,1, 1, -1,1, 1), ncol = 3, byrow = TRUE)
#' tris <- matrix(c(0L,3L,2L, 0L,2L,1L, 4L,5L,6L, 4L,6L,7L,
#'                   0L,1L,5L, 0L,5L,4L, 2L,3L,7L, 2L,7L,6L,
#'                   0L,4L,7L, 0L,7L,3L, 1L,2L,6L, 1L,6L,5L), ncol = 3, byrow = TRUE)
#' mesh <- list(vertices = verts, triangles = tris)
#' cam <- camera_auto(mesh, direction = c(1, 1, 1))
#'
#' @export
camera_auto <- function(mesh, direction = c(0, 0, -1), up = c(0, 1, 0),
                        fov = 45, margin = 1.1,
                        projection = c("perspective", "orthographic")) {
    projection <- match.arg(projection)
    if (is.list(mesh) && !is.null(mesh$vertices)) {
        mesh_data <- mesh
    } else if (is.matrix(mesh) && ncol(mesh) == 3L) {
        mesh_data <- list(
            vertices = mesh,
            triangles = matrix(integer(0), nrow = 0, ncol = 3)
        )
    } else if (is.list(mesh) && !is.null(mesh[[1]]$vertices)) {
        all_verts <- do.call(rbind, lapply(mesh, function(m) m$vertices))
        all_tris <- do.call(rbind, lapply(seq_along(mesh), function(i) {
            m <- mesh[[i]]
            if (!is.null(m$triangles) && nrow(m$triangles) > 0) {
                offset <- if (i == 1) 0L else sum(sapply(mesh[seq_len(i - 1)],
                    function(mm) nrow(mm$vertices)))
                m$triangles + offset
            } else {
                matrix(integer(0), nrow = 0, ncol = 3)
            }
        }))
        mesh_data <- list(vertices = all_verts, triangles = all_tris)
    } else {
        stop(
            "mesh must be an Nx3 matrix, a mesh descriptor list, or a list of mesh descriptors"
        )
    }

    scimesh_camera_fit_mesh(mesh_data, direction, up, fov, margin, projection)
}

#' @rdname camera
#' @export
camera_orbit <- function(camera, axis = c(0, 0, 1), angle_degrees) {
    axis <- as.numeric(axis) / sqrt(sum(axis^2))
    angle <- angle_degrees * pi / 180

    rotate <- function(v) {
        cos_a <- cos(angle)
        sin_a <- sin(angle)
        dot <- sum(v * axis)
        cross <- c(
            v[2] * axis[3] - v[3] * axis[2],
            v[3] * axis[1] - v[1] * axis[3],
            v[1] * axis[2] - v[2] * axis[1]
        )
        v * cos_a + cross * sin_a + axis * dot * (1 - cos_a)
    }

    cam <- camera
    cam$eye <- cam$center + rotate(cam$eye - cam$center)
    cam$up  <- rotate(cam$up)
    cam
}
