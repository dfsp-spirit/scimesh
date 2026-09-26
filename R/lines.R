# Screen-space line layers.
#
# A line layer is the cheap counterpart of tube meshes (generate_tubes()): it
# draws many independent segments with a width measured in pixels and creates no
# geometry at all.  Layers belong to a scene (see scene(lines = ...)) and share
# the camera and the depth buffer with the meshes of that scene.

#' Create a line layer (screen-space lines, no geometry)
#'
#' Bundles a set of independent line segments with a width measured in pixels
#' into a layer that can be added to a scene (see the \code{lines} argument of
#' \code{\link{scene}()}), or drawn directly with \code{\link{render_segments}}.
#' In contrast to tube meshes (\code{\link{generate_tubes}}), no geometry is
#' created: the renderer draws the segments itself, so thousands of lines cost
#' almost nothing, and a width of 1 stays 1 pixel wide no matter how far away
#' the geometry is.  This is what hardware line rendering (and
#' \code{rgl::segments3d()}) does.
#'
#' Line layers are drawn together with the meshes of the scene, after them and
#' against the same depth buffer, so opaque meshes can hide lines and opaque
#' lines can hide meshes.  Segments whose colors have an alpha value below 1
#' are drawn in the blended pass, back to front, exactly like translucent
#' triangles.
#'
#' Lines usually *are* the content of a figure (graph or connectome edges,
#' streamlines, trajectories), so by default a layer contributes to the bounding
#' box of its scene, exactly like a mesh does: it defines the extent that the
#' camera has to cover.  Set \code{affects_bounds = FALSE} for a layer that is
#' decoration rather than content (a leader line to a label, an axis cross, a
#' scale bar drawn as segments), so that it can never push the camera away from
#' the data.  A scene that contains no mesh at all is framed by its line layers
#' even when they all opted out, since there would otherwise be no geometry to
#' derive a camera from.
#'
#' @param from Nx3 numeric matrix of segment start points (or a length-3
#'   vector for a single segment).
#' @param to Nx3 numeric matrix of segment end points (same number of rows as
#'   \code{from}).
#' @param colors RGBA colour(s): a single vector applied to all segments, or an
#'   Nx4 numeric matrix (values in \code{[0, 1]}, alpha optional).  The default
#'   \code{NULL} uses the \code{default_color} of the render options.
#' @param width Line width in pixels (default 1).
#' @param depth_test Whether to test the lines against the depth buffer
#'   (default \code{TRUE}).  Set to \code{FALSE} to draw them on top of
#'   everything, which is only useful for opaque lines.
#' @param lit Whether to apply lighting to the lines (default \code{FALSE},
#'   i.e. a flat colour, like hardware-rendered lines).
#' @param affects_bounds Whether this layer contributes to the bounding box of
#'   the scene, and thus to the camera fitted to it (default \code{TRUE}, see
#'   the description).  Set to \code{FALSE} for decorational lines.  The flag of
#'   a layer that is already part of a scene can be changed with
#'   \code{\link{scene_set_line_affects_bounds}}.
#' @return A line layer object (a list with class \code{scimesh_lines}) for use
#'   in \code{\link{scene}()} or \code{\link{render_segments}}.
#'
#' @examples
#' from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
#' to   <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)
#' layer <- line_layer(from, to, colors = c(0.2, 0.2, 0.2, 0.8), width = 2)
#' sc <- scene(list(generate_sphere(c(0, 0, 0), 0.5)), lines = layer)
#'
#' @seealso \code{\link{render_segments}}, \code{\link{generate_tubes}}
#' @export
line_layer <- function(from, to, colors = NULL, width = 1, depth_test = TRUE,
                       lit = FALSE, affects_bounds = TRUE) {
    from <- check_points_matrix(from, "from")
    to <- check_points_matrix(to, "to")
    if (nrow(from) != nrow(to)) {
        stop(sprintf("from and to must have the same number of rows, got %d and %d",
                     nrow(from), nrow(to)))
    }
    if (!is.numeric(width) || length(width) != 1L || width <= 0) {
        stop("width must be a single positive number")
    }
    structure(list(from = from, to = to,
                   colors = recycle_colors(colors, nrow(from)),
                   width = as.double(width),
                   depth_test = isTRUE(depth_test),
                   lit = isTRUE(lit),
                   affects_bounds = isTRUE(affects_bounds)),
              class = "scimesh_lines")
}

#' @export
print.scimesh_lines <- function(x, ...) {
    cat(sprintf("scimesh line layer with %d segment(s), width %g px%s%s\n",
                nrow(x$from), x$width,
                if (isTRUE(x$lit)) ", lit" else "",
                if (isFALSE(x$affects_bounds)) ", decoration (does not affect the scene bounds)" else ""))
    invisible(x)
}

#' Render line segments directly to an image
#'
#' Convenience wrapper around \code{\link{line_layer}} for the case where no
#' meshes are involved: the segments are drawn (with a screen-space width) into
#' an image and nothing else.  To combine lines with meshes, add the layer to a
#' scene instead and render that scene.
#'
#' @param from Nx3 numeric matrix of segment start points (or a length-3 vector).
#' @param to Nx3 numeric matrix of segment end points (same number of rows as
#'   \code{from}).
#' @param colors RGBA colour(s): a single vector applied to all segments, or an
#'   Nx4 numeric matrix.  \code{NULL} (the default) uses the \code{default_color}
#'   of the render options.
#' @param width Line width in pixels (default 1).
#' @param camera A camera list, e.g. from \code{\link{camera}} or
#'   \code{\link{camera_auto}}.  Defaults to a camera framing the segments.
#' @param options Render options, see \code{\link{render_options}}.
#' @param lit Whether to apply lighting (default \code{FALSE}, flat colour).
#' @return An image list, see \code{\link{render_scene}}.
#'
#' @examples
#' from <- matrix(c(-1, 0, 0, 0, -1, 0), ncol = 3, byrow = TRUE)
#' to   <- matrix(c(1, 0, 0, 0, 1, 0), ncol = 3, byrow = TRUE)
#' img <- render_segments(from, to, colors = c(1, 0, 0, 1), width = 3)
#' tmp_file <- tempfile(fileext = ".png")
#' write_png(img, tmp_file)
#'
#' @seealso \code{\link{line_layer}}, \code{\link{render_points}}
#' @export
render_segments <- function(from, to, colors = NULL, width = 1, camera = NULL,
                            options = render_options(), lit = FALSE) {
    layer <- line_layer(from, to, colors = colors, width = width, lit = lit)
    if (is.null(camera)) {
        camera <- camera_auto(rbind(layer$from, layer$to))
    }
    scimesh_render_lines_raw(layer$from, layer$to, layer$colors,
                             layer$width, camera, options, layer$lit)
}
