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
#' @examples
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' img <- render_mesh(mesh$vertices, mesh$triangles)
#' arr <- image_to_array(img)
#' dim(arr)  # height x width x 4
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
#' to a PNG file using the built-in C++ PNG writer (stb_image_write).
#' No additional R packages are required.
#'
#' @param image An image list returned by \code{render_mesh()} or
#'   \code{render_scene()}.
#' @param filename Output PNG file path.
#'
#' @examples
#' \dontrun{
#' mesh <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))
#' img <- render_mesh(mesh$vertices, mesh$triangles)
#' write_png(img, "cuboid.png")
#' }
#'
#' @export
write_png <- function(image, filename) {
    invisible(scimesh_write_png(image, filename))
}
