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
#' The camera is placed on the side given by \code{direction} and looks
#' back at the mesh, at a distance that ensures the mesh fits within the
#' field of view.
#'
#' When \code{rgl_compat = TRUE}, the camera mimics rgl's default
#' auto-framing behaviour: a 30° FOV, 15° elevation, and the distance
#' is computed from the \emph{bounding sphere} of the mesh (the
#' half-diagonal of the axis-aligned bounding box), reproducing the
#' formula \code{distance = sphere_radius / sin(FOV/2)} used by rgl.
#'
#' @param mesh Either an Nx3 numeric matrix of vertex positions, or a
#'   mesh descriptor list with a \code{vertices} component.
#' @param direction Direction from the mesh towards the camera, as a
#'   length-3 vector: it selects the side you view the mesh from (the
#'   camera is placed at \code{center + direction * distance}).  For
#'   example, \code{c(0, 0, 1)} gives a front view of a mesh that faces
#'   +Z, and \code{c(1, 0, 0)} looks at it from its +X side.  Ignored
#'   when \code{rgl_compat = TRUE}.
#' @param up The up vector as a length-3 vector. Default \code{c(0, 1, 0)}.
#'   Ignored when \code{rgl_compat = TRUE}.
#' @param fov Field of view in degrees. Default 45° (30° when
#'   \code{rgl_compat = TRUE}).
#' @param margin Extra margin factor (1.0 = tight fit, 1.1 = 10\% margin).
#' @param rgl_compat Logical. If \code{TRUE}, use rgl's camera defaults
#'   and bounding-sphere distance formula. Default \code{FALSE}.
#' @param projection Projection type: \code{"perspective"} (default) or
#'   \code{"orthographic"}. When orthographic, the camera distance is
#'   computed to tightly frame the mesh regardless of FOV.
#' @return A camera list, with S3 class \code{"scimesh_camera"}.
#'
#' @note This function frames a mesh (or a set of vertices).  It does not know
#'   about scene contents such as line layers or text labels; use
#'   \code{\link{camera_fit_scene}} to fit a camera to a whole scene, including
#'   the line layers that contribute to the scene bounds.
#'
#' @examples
#' verts <- matrix(c(-1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
#'                    -1,-1, 1, 1,-1, 1, 1,1, 1, -1,1, 1), ncol = 3, byrow = TRUE)
#' tris <- matrix(c(0L,3L,2L, 0L,2L,1L, 4L,5L,6L, 4L,6L,7L,
#'                   0L,1L,5L, 0L,5L,4L, 2L,3L,7L, 2L,7L,6L,
#'                   0L,4L,7L, 0L,7L,3L, 1L,2L,6L, 1L,6L,5L), ncol = 3, byrow = TRUE)
#' mesh <- list(vertices = verts, triangles = tris)
#' cam <- camera_auto(mesh, direction = c(1, 1, 1))
#' cam_rgl <- camera_auto(mesh, rgl_compat = TRUE)
#'
#' @export
camera_auto <- function(mesh, direction = c(0, 0, -1), up = c(0, 1, 0),
                        fov = 45, margin = 1.1,
                        rgl_compat = FALSE,
                        projection = c("perspective", "orthographic")) {
    projection <- match.arg(projection)
    # Transparently accept rgl-style meshes (vb/it format)
    if (is.list(mesh) && !is.null(mesh$vb) && !is.null(mesh$it)) {
        mesh <- mesh_from_rgl(mesh)
    }
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

    if (isTRUE(rgl_compat)) {
        # --- rgl-compatible auto-framing ----------------------------------------
        # rgl defaults: FOV = 30°, phi = 15° elevation, theta = 0°
        # Distance = bounding_sphere_radius / sin(FOV / 2)
        #
        # The bounding sphere is the sphere that encloses the AABB, with
        # radius = half the length of the AABB diagonal.
        rgl_fov <- 30

        verts <- mesh_data$vertices
        vmin <- apply(verts, 2, min)
        vmax <- apply(verts, 2, max)
        half_diag <- (vmax - vmin) / 2
        sphere_radius <- sqrt(sum(half_diag^2))

        fov_half_rad <- (rgl_fov / 2) * pi / 180
        distance <- sphere_radius / sin(fov_half_rad)

        # rgl default orientation: theta = 0 (no azimuthal rotation),
        # phi = 15° elevation above the horizontal plane.
        # This is equivalent to looking along -Z tilted upward by phi.
        phi_rad <- 15 * pi / 180

        # Compute center: centroid of the bounding box
        center <- (vmin + vmax) / 2

        # Direction tilted up by phi from -Z axis
        direction <- c(0, sin(phi_rad), -cos(phi_rad))

        # Up vector: rotated with the direction
        up <- c(0, cos(phi_rad), sin(phi_rad))

        eye <- center + direction * distance

        camera(eye = eye, center = center, up = up,
               projection = projection, fov = rgl_fov)
    } else {
        scimesh_camera_fit_mesh(mesh_data, direction, up, fov, margin, projection)
    }
}


#' Fit a camera to a whole scene
#'
#' Computes a camera that frames the contents of a scene (see
#' \code{\link{scene}}), i.e. its meshes together with the line layers that
#' contribute to the scene bounds (see \code{\link{line_layer}}, parameter
#' \code{affects_bounds}).  Use this instead of \code{\link{camera_auto}} when
#' the camera has to consider something else than a mesh: a scene that contains
#' only line layers (e.g. a tractogram or a connectome without a brain surface)
#' is framed by those lines, and decorational layers
#' (\code{affects_bounds = FALSE}) are ignored.
#'
#' The camera is placed on the line from the center of the bounding box along
#' \code{direction}, at a distance that makes the content fit into the field of
#' view, exactly like \code{\link{camera_auto}} does it for a mesh.
#'
#' @param scene A scene descriptor list, see \code{\link{scene}()}.
#' @param direction Length-3 view direction, from the camera towards the scene
#'   (default \code{c(0, 0, -1)}, i.e. looking along -Z).
#' @param up Length-3 up vector (default \code{c(0, 1, 0)}).
#' @param fov Vertical field of view in degrees (default 45).
#' @param margin Scale factor applied to the fitted distance; values above 1
#'   leave a margin around the content (default 1.1).
#' @param projection Projection type, \code{"perspective"} (default) or
#'   \code{"orthographic"}.
#'
#' @return A camera list (see \code{\link{camera}()}) with class
#'   \code{"scimesh_camera"}.
#'
#' @seealso \code{\link{camera_auto}} for meshes, \code{\link{scene}},
#'   \code{\link{scene_set_line_affects_bounds}}
#' @examples
#' # A scene without any mesh is framed by its lines.
#' line <- line_layer(matrix(c(0, 0, 0), ncol = 3), matrix(c(2, 0, 0), ncol = 3))
#' sc <- scene(list(), lines = line)
#' cam <- camera_fit_scene(sc, direction = c(0, 0, -1))
#'
#' @export
camera_fit_scene <- function(scene, direction = c(0, 0, -1), up = c(0, 1, 0),
                             fov = 45, margin = 1.1,
                             projection = c("perspective", "orthographic")) {
    projection <- match.arg(projection)
    if (!inherits(scene, "scimesh_scene")) {
        stop("scene must be a scene descriptor, see scene()")
    }
    scimesh_camera_fit_scene(scene_data_from_scene(scene), direction, up, fov,
                             margin, projection)
}

#' Orbit a camera around an axis
#'
#' Rotates a camera's eye position and up vector around its center
#' by a given angle about a rotation axis.  Useful for generating
#' turntable-style frame sequences.
#'
#' @param camera A camera list from \code{camera()} or \code{camera_auto()}.
#' @param axis Rotation axis as a length-3 vector. Default \code{c(0, 0, 1)} (Z axis).
#' @param angle_degrees Rotation angle in degrees.
#' @return A camera list with S3 class \code{"scimesh_camera"}.
#'
#' @examples
#' mesh <- generate_torus(c(0, 0, 0))
#' cam <- camera_auto(mesh, direction = c(1, 1, 1))
#' cam2 <- camera_orbit(cam, axis = c(0, 0, 1), angle_degrees = 90)
#'
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
