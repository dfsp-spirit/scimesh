#' Convert a rendered image to an RGBA array
#'
#' Converts the output of \code{render_mesh()} or \code{render_scene()}
#' into a 3-dimensional R array of dimensions (height x width x 4) with
#' RGBA channels.
#'
#' @param image An image list returned by \code{render_mesh()} or
#'   \code{render_scene()}.
#' @return A 3D array of dimensions (height, width, 4) with values in
#'   \code{[0, 1]}.
#'
#' @export
image_to_array <- function(image) {
    if (!is.list(image) || is.null(image$pixels)) {
        stop("image must be a renderer output list with 'pixels' component")
    }
    vals <- as.numeric(image$pixels) / 255.0
    w <- image$width
    h <- image$height
    # C++ data is row-major RGBA interleaved: pixel(y,x) at (y*W+x)*4.
    # array(vals, dim=c(4,W,H)) matches this layout, then aperm to (H,W,4).
    arr <- array(vals, dim = c(4L, w, h))
    aperm(arr, c(3L, 2L, 1L))
}

#' Write a rendered image to a PNG file
#'
#' Writes the output of \code{render_mesh()} or \code{render_scene()}
#' to a PNG file. Requires the \code{png} package.
#'
#' @param image An image list returned by \code{render_mesh()} or
#'   \code{render_scene()}.
#' @param filename Output PNG file path.
#'
#' @export
write_png <- function(image, filename) {
    if (!requireNamespace("png", quietly = TRUE)) {
        stop("The 'png' package is required to write PNG files. ",
             "Install it with: install.packages(\"png\")")
    }
    arr <- image_to_array(image)
    png::writePNG(arr, filename)
    invisible(filename)
}
