#' Create a clip plane specification
#'
#' Defines a clipping plane for \code{render_options()}.  Geometry on the
#' negative side of the plane is removed, i.e. a point \code{p} is kept when
#' \code{dot(normal, p) + offset >= 0}.
#'
#' By default the plane is defined in \strong{world space}, so the cut is a
#' fixed feature of the scene: it does not move when the camera is moved
#' around, which is what you want for cross-sections and multi-view figures.
#' Set \code{space = "eye"} for a camera-relative plane that travels with the
#' camera (the classic OpenGL \code{glClipPlane} behaviour), e.g. for
#' cutaway views.
#'
#' @param normal Numeric vector of length 3: the plane normal.  It points
#'   toward the side of the plane that is \emph{kept}.  It does not have to be
#'   unit-length; it is normalized internally and \code{offset} is always
#'   interpreted as a distance in world units.
#' @param offset Numeric scalar: the signed distance of the plane from the
#'   origin along \code{normal}, in world units.  For example
#'   \code{offset = -d} places the plane at distance \code{d} from the origin
#'   (in the direction of \code{normal}).
#' @param space Character, either \code{"world"} (default) or \code{"eye"}:
#'   \describe{
#'     \item{\code{"world"}}{The plane is fixed in world coordinates and does
#'       not move when the camera moves.  This is the convention used by
#'       rgl's \code{clipplanes3d()}, VTK/PyVista, ParaView and three.js.}
#'     \item{\code{"eye"}}{The plane is defined relative to the camera, i.e.
#'       \code{offset} is a distance from the camera, and the plane moves and
#'       rotates with it.}
#'   }
#' @return A list with components \code{normal}, \code{offset} and
#'   \code{space}, suitable for the \code{clip_planes} argument of
#'   \code{render_options()}.
#'
#' @examples
#' # World space (default): keep the half of the scene with x <= 0.
#' # The cut stays at x = 0, no matter where the camera is placed.
#' clip_plane(normal = c(-1, 0, 0), offset = 0)
#'
#' # Keep only the part with z >= -0.5 (remove everything below z = -0.5):
#' clip_plane(normal = c(0, 0, 1), offset = 0.5)
#'
#' # Eye space: remove everything closer than 2 units to the camera
#' # (a camera-attached cutaway).
#' clip_plane(normal = c(0, 0, -1), offset = -2, space = "eye")
#'
#' # A world-space cut through a cuboid; the cut stays at x = 0 for any camera
#' cuboid <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' cut_opts <- render_options(clip_planes = list(
#'     clip_plane(normal = c(-1, 0, 0), offset = 0)))
#'
#' mesh_pixels <- function(img) sum(image_to_array(img)[, , 1] < 1)
#' cam <- camera(eye = c(3, 3, 3), center = c(0, 0, 0))
#'
#' full <- render_mesh(cuboid$vertices, cuboid$triangles, camera = cam)
#' cut <- render_mesh(cuboid$vertices, cuboid$triangles, camera = cam,
#'     options = cut_opts)
#'
#' mesh_pixels(cut) < mesh_pixels(full)   # TRUE: half of the cuboid is gone
#'
#' @seealso \code{\link{render_options}}
#' @export
clip_plane <- function(normal, offset = 0, space = c("world", "eye")) {
    space <- match.arg(space)
    check_clip_plane(list(normal = normal, offset = offset, space = space), 1L)
}

#' @keywords internal
#' @noRd
check_clip_plane <- function(plane, index) {
    where <- sprintf("clip_planes[[%d]]", index)
    if (!is.list(plane)) {
        stop(sprintf(paste0("%s must be a list, e.g. ",
            "clip_plane(c(-1, 0, 0), offset = 0); see ?clip_plane"),
            where), call. = FALSE)
    }
    if (is.null(plane$normal)) {
        stop(sprintf(paste0("%s has no 'normal' component; ",
            "see ?clip_plane"), where), call. = FALSE)
    }
    normal <- suppressWarnings(as.numeric(plane$normal))
    if (length(normal) != 3L || anyNA(normal) || any(!is.finite(normal))) {
        stop(sprintf("%s$normal must be a numeric vector of length 3", where),
             call. = FALSE)
    }
    if (sum(normal^2) <= 0) {
        stop(sprintf(paste0("%s$normal must not be all zero ",
            "(a zero normal does not define a plane)"), where), call. = FALSE)
    }
    offset <- if (is.null(plane$offset)) 0 else suppressWarnings(as.numeric(plane$offset))
    if (length(offset) != 1L || anyNA(offset) || !is.finite(offset)) {
        stop(sprintf("%s$offset must be a single finite number", where),
             call. = FALSE)
    }
    space <- if (is.null(plane$space)) "world" else plane$space
    if (!is.character(space) || length(space) != 1L ||
        !(space %in% c("world", "eye"))) {
        stop(sprintf(paste0("%s$space must be either \"world\" (default) ",
            "or \"eye\""), where), call. = FALSE)
    }
    list(normal = normal, offset = offset, space = space)
}

#' @keywords internal
#' @noRd
check_clip_planes <- function(planes) {
    if (is.null(planes)) {
        return(NULL)
    }
    if (!is.list(planes)) {
        stop(paste0("clip_planes must be a list of clip planes ",
            "(see ?clip_plane) or NULL"), call. = FALSE)
    }
    if (length(planes) == 0L) {
        return(NULL)
    }
    lapply(seq_along(planes), function(i) check_clip_plane(planes[[i]], i))
}

#' @keywords internal
#' @noRd
check_fog_options <- function(fog_start, fog_end) {
    if (!is.numeric(fog_start) || length(fog_start) != 1L ||
        !is.finite(fog_start)) {
        stop("fog_start must be a single finite number", call. = FALSE)
    }
    if (!is.numeric(fog_end) || length(fog_end) != 1L ||
        !is.finite(fog_end)) {
        stop("fog_end must be a single finite number", call. = FALSE)
    }
    if (fog_end <= fog_start) {
        stop("fog_end must be greater than fog_start", call. = FALSE)
    }
    invisible(NULL)
}

#' @keywords internal
#' @noRd
check_planes_near_far <- function(near_plane, far_plane) {
    if (!is.numeric(near_plane) || length(near_plane) != 1L ||
        !is.finite(near_plane) || near_plane <= 0) {
        stop("near_plane must be a single positive number", call. = FALSE)
    }
    if (!is.numeric(far_plane) || length(far_plane) != 1L ||
        !is.finite(far_plane) || far_plane <= near_plane) {
        stop("far_plane must be a single number larger than near_plane",
             call. = FALSE)
    }
    invisible(NULL)
}
