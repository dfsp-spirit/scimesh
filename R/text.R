# Text labels.
#
# A text layer is the annotation counterpart of mesh and line layers: it draws
# strings ("anterior", "C", a figure title) as billboards that always face the
# camera and keep their pixel size.  Labels belong to a scene (see
# scene(texts = ...)) and are drawn after the meshes and lines, so they appear
# on top of the geometry — except when depth_test is on and the annotated point
# is hidden behind a surface.
#
# Nothing is turned into geometry: glyphs are rasterized from a TrueType file
# (the bundled Inter-Regular.ttf by default, replaceable via font_file).

#' Create a text label layer (2D annotations for a scene)
#'
#' Bundles strings with anchor positions into a layer that can be added to a
#' scene (see the \code{texts} argument of \code{\link{scene}()}), or drawn
#' directly with \code{\link{render_text}}.
#'
#' Labels are \emph{billboards}: they always face the camera and keep the size
#' given in \code{size} (in output pixels), so they stay readable from any
#' viewpoint — unlike text that is turned into 3D geometry, which skews as the
#' camera moves.  This is the screen-friendly counterpart of
#' \code{rgl::text3d()}, and the intended way to label brain regions, atoms,
#' panels or figure axes.
#'
#' Positions are given either in \strong{world space} (the default) or in
#' \strong{screen space}: with \code{space = "screen"} the coordinates are
#' pixels of the output image, measured from the top left corner, which is what
#' you want for titles, panel tags and captions.  World-space labels are
#' projected with the camera of the scene, so they stick to the annotated
#' location, and they are hidden by geometry in front of them unless
#' \code{depth_test = FALSE}.
#'
#' @param positions Anchor positions: an Nx3 numeric matrix for world space, or
#'   an Nx2/Nx3 numeric matrix for \code{space = "screen"} (third column
#'   ignored).  A single point may be given as a numeric vector of length 2 or 3.
#' @param text Character vector of labels (UTF-8), recycled to
#'   \code{nrow(positions)}.  Use \code{"\n"} for line breaks.
#' @param colors RGBA colour(s): a single vector applied to all labels, or an
#'   Nx4 numeric matrix (values in \code{[0, 1]}, alpha optional).  The default
#'   \code{NULL} uses the \code{default_color} of the render options.
#' @param size Text height in output pixels (default 18).  Independent of the
#'   anti-aliasing setting: a label keeps its physical size when
#'   \code{aa_samples} is raised.
#' @param font_file Path to a \code{.ttf} file to use.  \code{NULL} (the
#'   default) uses the bundled Inter font, see \code{\link{default_font}()}.
#' @param space \code{"world"} (default) for positions in the 3D scene, or
#'   \code{"screen"} for positions in output pixels.
#' @param adj Numeric vector of length 2 giving where the position sits on the
#'   text box, in \code{[0, 1]}: \code{c(0, 0)} is the bottom left corner,
#'   \code{c(1, 1)} the top right one, and the default \code{c(0.5, 0.5)}
#'   centres the text on the position.
#' @param offset Numeric vector of length 2: extra offset in output pixels
#'   (positive x = right, positive y = down), applied after anchoring.  Handy to
#'   push an atom label next to the atom instead of onto it.
#' @param line_spacing Distance between two lines of a multi-line label, as a
#'   multiple of the font's glyph box height (default 1.2).
#' @param rotation Rotation of the label in degrees, counter-clockwise, about
#'   the anchor position (default 0).  Use 90 to write along a vertical axis
#'   (the usual orientation of a y-axis label), 180 for an upside-down label,
#'   or any other angle to follow an annotation line.  The anchor stays fixed
#'   while the text turns around it.
#' @param depth_test Whether a label is hidden by geometry in front of its
#'   anchor (default \code{TRUE}).  Set to \code{FALSE} to always draw the
#'   labels on top of everything, which is the right choice for direction
#'   annotations ("anterior") and for labels on a surface.
#' @param halo_color RGBA colour of the halo (outline) drawn behind the glyphs,
#'   which keeps labels readable on dark or busy geometry.  \code{NULL} (the
#'   default) draws no halo.
#' @param halo_width Halo thickness in pixels (default 1.5).
#' @return A text layer object (a list with class \code{scimesh_text}) for use
#'   in \code{\link{scene}()} or \code{\link{render_text}}.
#'
#' @examples
#' sph <- generate_sphere(c(0, 0, 0), radius = 1)
#' # world-space label above the sphere
#' labels <- text_layer(matrix(c(0, 1.4, 0), ncol = 3), "top", size = 20)
#' labels
#'
#' # screen-space panel tag, positioned by its top left corner
#' tag <- text_layer(c(10, 12), "A", space = "screen", adj = c(0, 1), size = 28,
#'                   halo_color = c(1, 1, 1, 0.9))
#' tag
#'
#' # multiple labels with per-label colors and positions
#' multi <- text_layer(matrix(c(-1, 0, 0, 1, 0, 0), ncol = 3, byrow = TRUE),
#'                     c("left", "right"), colors = matrix(c(1, 0, 0, 1,
#'                                                          0, 0, 1, 1),
#'                                                         ncol = 4, byrow = TRUE))
#' multi
#'
#' @seealso \code{\link{render_text}}, \code{\link{text_extent}},
#'   \code{\link{scene}}, \code{\link{line_layer}}
#' @export
text_layer <- function(positions, text, colors = NULL, size = 18,
                       font_file = NULL, space = c("world", "screen"),
                       adj = c(0.5, 0.5), offset = c(0, 0),
                       line_spacing = 1.2, depth_test = TRUE,
                       halo_color = NULL, halo_width = 1.5, rotation = 0) {
    space <- match.arg(space)
    positions <- check_text_positions(positions, space)

    if (!is.character(text) || length(text) < 1L) {
        stop("text must be a character vector with at least one entry")
    }
    if (any(is.na(text))) {
        stop("text must not contain NA")
    }
    if (length(text) != nrow(positions)) {
        if (length(text) == 1L || nrow(positions) %% length(text) == 0L) {
            text <- rep(text, length.out = nrow(positions))
        } else {
            stop(sprintf("text has %d entry/entries but there are %d positions",
                         length(text), nrow(positions)))
        }
    }

    if (!is.numeric(size) || length(size) != 1L || is.na(size) || size <= 0) {
        stop("size must be a single positive number")
    }
    check_unit_interval(adj, 2L, "adj")
    if (!is.numeric(offset) || length(offset) != 2L || any(is.na(offset))) {
        stop("offset must be a numeric vector of length 2")
    }
    if (!is.numeric(line_spacing) || length(line_spacing) != 1L ||
        is.na(line_spacing) || line_spacing <= 0) {
        stop("line_spacing must be a single positive number")
    }
    if (!is.null(halo_color)) {
        halo_color <- check_rgba(halo_color, "halo_color")
    }
    if (!is.numeric(halo_width) || length(halo_width) != 1L ||
        is.na(halo_width) || halo_width < 0) {
        stop("halo_width must be a single non-negative number")
    }
    if (!is.numeric(rotation) || length(rotation) != 1L ||
        is.na(rotation) || !is.finite(rotation)) {
        stop("rotation must be a single finite number (degrees)")
    }

    structure(list(strings = text,
                   positions = positions,
                   colors = recycle_colors(colors, nrow(positions)),
                   size = as.double(size),
                   font_file = resolve_font_file(font_file),
                   space = space,
                   adj = as.double(adj),
                   offset = as.double(offset),
                   line_spacing = as.double(line_spacing),
                   depth_test = isTRUE(depth_test),
                   halo_color = halo_color,
                   halo_width = as.double(halo_width),
                   rotation = as.double(rotation)),
              class = "scimesh_text")
}

#' @export
print.scimesh_text <- function(x, ...) {
    n <- nrow(x$positions)
    cat(sprintf("scimesh text layer with %d label(s), %g px, %s space%s%s%s\n",
                n, x$size, x$space,
                if (isTRUE(x$depth_test)) "" else ", depth test off",
                if (is.null(x$halo_color)) "" else ", halo",
                if (abs(x$rotation) > 1e-6) sprintf(", rotated %g deg", x$rotation) else ""))
    labels <- x$strings[seq_len(min(3L, length(x$strings)))]
    labels <- gsub("\n", " / ", labels, fixed = TRUE)
    if (nchar(labels[1]) > 40L) {
        labels[1] <- paste0(substr(labels[1], 1L, 37L), "...")
    }
    cat("  ", paste(sprintf("'%s'", labels), collapse = ", "), "\n", sep = "")
    invisible(x)
}

#' Render text labels to an image
#'
#' Convenience wrapper around \code{\link{text_layer}} for the case where no
#' meshes are involved: the labels are drawn onto the background described by
#' the render options.  Screen-space labels (\code{space = "screen"}) do not use
#' a camera at all, so this is the quick way to turn a label into an image or to
#' decorate an empty canvas; world-space labels need a \code{camera}.
#'
#' @param positions Anchor positions, see \code{\link{text_layer}}.
#' @param text Character vector of labels, see \code{\link{text_layer}}.
#' @param camera A camera list from \code{\link{camera}()} or
#'   \code{\link{camera_auto}()}.  Ignored for screen-space labels; when
#'   \code{NULL}, a default camera is used.
#' @param options Render options from \code{\link{render_options}()}.
#' @param ... Further arguments passed to \code{\link{text_layer}} (for example
#'   \code{size}, \code{space}, \code{color}, \code{halo_color}).
#' @return A list with components \code{width}, \code{height}, and
#'   \code{pixels} (raw vector of RGBA values).
#'
#' @examples
#' img <- render_text(c(20, 20), "figure A", space = "screen",
#'                    adj = c(0, 1), size = 24,
#'                    options = render_options(width = 300, height = 80))
#' tmp <- tempfile(fileext = ".png")
#' write_png(img, tmp)
#'
#' @seealso \code{\link{text_layer}}, \code{\link{render_scene}}
#' @export
render_text <- function(positions, text, camera = NULL, options = NULL, ...) {
    layer <- text_layer(positions = positions, text = text, ...)
    if (is.null(options)) {
        options <- render_options()
    }
    if (is.null(camera)) {
        camera <- camera(eye = c(0, 0, 10), center = c(0, 0, 0))
    }
    render_scene(scene(list(), camera = camera, options = options,
                       texts = layer))
}

#' Measure text
#'
#' Computes the size of the text box that \code{\link{text_layer}} uses, which
#' is needed to place labels relative to each other (for example to right-align
#' a caption, or to keep two labels from overlapping).  Multi-line labels are
#' measured as a whole, using the same line spacing as the renderer.
#'
#' @param text Character vector of labels (UTF-8; \code{"\n"} for line breaks).
#' @param size Text height in output pixels (default 18).
#' @param font_file Path to a \code{.ttf} file, or \code{NULL} for the bundled
#'   font (see \code{\link{default_font}()}).
#' @param line_spacing Distance between lines, as a multiple of the glyph box
#'   height (default 1.2).
#' @return A data frame with one row per input string and the columns
#'   \code{text}, \code{width} (widest line), \code{height} (whole block),
#'   \code{ascent}, \code{descent} and \code{lines}.
#'
#' @examples
#' text_extent("anterior", size = 20)
#' text_extent(c("left hemisphere", "right hemisphere"), size = 16)
#'
#' @seealso \code{\link{text_layer}}
#' @export
text_extent <- function(text, size = 18, font_file = NULL, line_spacing = 1.2) {
    if (!is.character(text) || length(text) < 1L) {
        stop("text must be a character vector with at least one entry")
    }
    if (!is.numeric(size) || length(size) != 1L || is.na(size) || size <= 0) {
        stop("size must be a single positive number")
    }
    if (!is.numeric(line_spacing) || length(line_spacing) != 1L ||
        is.na(line_spacing) || line_spacing <= 0) {
        stop("line_spacing must be a single positive number")
    }
    res <- scimesh_text_extent(text, as.double(size),
                               resolve_font_file(font_file),
                               as.double(line_spacing))
    data.frame(text = text,
               width = res[, 1L],
               height = res[, 2L],
               ascent = res[, 3L],
               descent = res[, 4L],
               lines = res[, 5L],
               stringsAsFactors = FALSE)
}

#' Information about the font used for text labels
#'
#' Reports which font file is used (and where it lives), its family name and
#' its vertical metrics.  Useful to check that the bundled font was found and to
#' see what a custom \code{.ttf} file contains.
#'
#' @param font_file Path to a \code{.ttf} file, or \code{NULL} for the bundled
#'   font (see \code{\link{default_font}()}).
#' @param size Text height in output pixels (default 18).
#' @return A list with components \code{family}, \code{path}, \code{size},
#'   \code{ascent}, \code{descent} and \code{line_gap}.
#'
#' @examples
#' font_info()
#'
#' @seealso \code{\link{default_font}}, \code{\link{text_layer}}
#' @export
font_info <- function(font_file = NULL, size = 18) {
    if (!is.numeric(size) || length(size) != 1L || is.na(size) || size <= 0) {
        stop("size must be a single positive number")
    }
    scimesh_font_info(resolve_font_file(font_file), as.double(size))
}

#' Path of the font used for text labels
#'
#' Text labels are drawn with a TrueType font that ships with the package
#' (\code{inst/extdata/Inter-Regular.ttf}, SIL Open Font License 1.1), so labels
#' look the same everywhere and no system font is required.  This function
#' returns the path of the font that will be used by default.
#'
#' It can be overridden without touching any code by setting the
#' \code{SCIMESH_FONT} environment variable to the path of another \code{.ttf}
#' file, or per call by passing \code{font_file} to
#' \code{\link{text_layer}()}, \code{\link{text_extent}()} and friends.
#'
#' @return A character scalar: the path to an existing font file.
#'
#' @examples
#' default_font()
#'
#' @seealso \code{\link{font_info}}, \code{\link{text_layer}}
#' @export
default_font <- function() {
    env <- Sys.getenv("SCIMESH_FONT", unset = "")
    if (nzchar(env) && file.exists(env)) {
        return(env)
    }
    bundled <- system.file("extdata", "Inter-Regular.ttf", package = "scimesh")
    if (nzchar(bundled) && file.exists(bundled)) {
        return(bundled)
    }
    fallback <- scimesh_default_font_path()
    if (nzchar(fallback) && file.exists(fallback)) {
        return(fallback)
    }
    stop("No font found. Please provide font_file = <path to a .ttf file>, ",
         "or set the SCIMESH_FONT environment variable.")
}

#' Project world coordinates to image pixels
#'
#' Runs the same view and projection as the renderer, so the result lands
#' exactly on the rendered image.  Useful to place screen-space annotations
#' (\code{\link{text_layer}(space = "screen")}) next to a 3D location, or to
#' draw callout lines with \code{\link{line_layer}} in image coordinates.
#'
#' @param points An Nx3 numeric matrix of world coordinates (or a length-3
#'   vector for a single point).
#' @param camera A camera list from \code{\link{camera}()} or
#'   \code{\link{camera_auto}()}.
#' @param width,height Size of the rendered image in pixels.
#' @param options Render options, used for the projection type and the clipping
#'   planes.  \code{NULL} uses default options with the given size.
#' @return A data frame with one row per input point and the columns \code{x},
#'   \code{y} (pixels, origin top left), \code{depth} (NDC depth, smaller is
#'   closer) and \code{in_front} (whether the point is in front of the camera;
#'   for \code{FALSE} the pixel coordinates are not meaningful).
#'
#' @examples
#' sph <- generate_sphere(c(0, 0, 0), radius = 1)
#' cam <- camera_auto(list(sph), direction = c(0, 0, 1))
#' opts <- render_options(width = 400, height = 300)
#' world_to_screen(matrix(c(0, 1, 0), ncol = 3), cam, 400, 300, opts)
#'
#' @seealso \code{\link{text_layer}}
#' @export
world_to_screen <- function(points, camera, width, height, options = NULL) {
    points <- check_points_matrix(points, "points")
    if (is.null(camera)) {
        stop("camera must be provided")
    }
    if (!is.numeric(width) || length(width) != 1L || width <= 0 ||
        !is.numeric(height) || length(height) != 1L || height <= 0) {
        stop("width and height must be single positive numbers")
    }
    if (is.null(options)) {
        options <- render_options(width = as.integer(width),
                                  height = as.integer(height))
    }
    scimesh_world_to_screen(points, camera, as.integer(width),
                            as.integer(height), options)
}

# ---- Internal helpers -------------------------------------------------------

#' Check and normalize the positions of a text layer
#'
#' Accepts a numeric vector of length 2 (screen) or 3 (world) for a single
#' label, or a matrix with 2 or 3 columns.
#'
#' @param positions The user-supplied positions.
#' @param space Either "world" or "screen".
#' @return A numeric matrix with 2 or 3 columns.
#' @keywords internal
#' @noRd
check_text_positions <- function(positions, space) {
    if (is.null(positions)) {
        stop("positions must not be NULL")
    }
    if (is.numeric(positions) && is.null(dim(positions))) {
        if (length(positions) %in% c(2L, 3L)) {
            positions <- matrix(as.double(positions), nrow = 1L)
        } else {
            stop("positions must be an Nx2 or Nx3 numeric matrix, or a single ",
                 "point given as a numeric vector of length 2 or 3")
        }
    }
    if (!is.matrix(positions) || !is.numeric(positions) ||
        !ncol(positions) %in% c(2L, 3L)) {
        stop("positions must be an Nx2 (screen) or Nx3 (world) numeric matrix")
    }
    if (nrow(positions) < 1L) {
        stop("positions must contain at least one point")
    }
    if (any(is.na(positions))) {
        stop("positions must not contain NA")
    }
    if (space == "world" && ncol(positions) != 3L) {
        stop("world-space labels need Nx3 positions; use space = \"screen\" ",
             "for pixel coordinates")
    }
    storage.mode(positions) <- "double"
    positions
}

#' Check a colour given as an RGB or RGBA vector
#'
#' @param x Numeric vector of length 3 or 4.
#' @param arg_name Name used in error messages.
#' @return A numeric vector of length 4 (alpha defaults to 1).
#' @keywords internal
#' @noRd
check_rgba <- function(x, arg_name = "color") {
    if (!is.numeric(x) || !length(x) %in% c(3L, 4L) || any(is.na(x))) {
        stop(arg_name, " must be an RGB or RGBA numeric vector (length 3 or 4)")
    }
    if (length(x) == 3L) {
        x <- c(x, 1)
    }
    as.double(x)
}

#' Check a numeric vector that must lie in [0, 1]
#'
#' @param x Numeric vector.
#' @param n Required length.
#' @param arg_name Name used in error messages.
#' @return The validated vector, as doubles.
#' @keywords internal
#' @noRd
check_unit_interval <- function(x, n, arg_name) {
    if (!is.numeric(x) || length(x) != n || any(is.na(x))) {
        stop(sprintf("%s must be a numeric vector of length %d", arg_name, n))
    }
    if (any(x < 0) || any(x > 1)) {
        stop(sprintf("%s values must lie in [0, 1]", arg_name))
    }
    as.double(x)
}

#' Resolve a font_file argument to an existing file
#'
#' @param font_file NULL (use the default font) or a path.
#' @return A path to an existing .ttf file.
#' @keywords internal
#' @noRd
resolve_font_file <- function(font_file) {
    if (is.null(font_file)) {
        return(default_font())
    }
    if (!is.character(font_file) || length(font_file) != 1L || is.na(font_file)) {
        stop("font_file must be a single file path, or NULL for the bundled font")
    }
    if (!file.exists(font_file)) {
        stop(sprintf("font file not found: %s", font_file))
    }
    path.expand(font_file)
}
