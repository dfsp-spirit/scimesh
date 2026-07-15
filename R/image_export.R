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

#' Apply gamma correction to an image
#'
#' Applies gamma encoding to the RGB channels of a rendered image.
#' Formula: \code{value^(1/gamma)}.  Default 1.0 = no change.
#' Values > 1.0 (e.g. 2.2) brighten midtones, matching the sRGB
#' transfer function used by GPU renderers.
#'
#' @param image An image list returned by \code{render_mesh()} or
#'   \code{render_scene()}.
#' @param gamma Gamma value.  Default 2.2 for sRGB-like contrast.
#' @return A new image list with gamma-corrected pixel data.
#'
#' @examples
#' \dontrun{
#' img <- render_mesh(cube$vertices, cube$triangles)
#' img <- image_apply_gamma(img, gamma = 2.2)
#' }
#'
#' @export
image_apply_gamma <- function(image, gamma = 2.2) {
    scimesh_image_apply_gamma(image, as.numeric(gamma))
}

#' Crop an image to a rectangular region
#'
#' @param image An image list returned by \code{render_mesh()} or similar.
#' @param x Left edge of the crop region (0-based pixel coordinate).
#' @param y Top edge of the crop region (0-based pixel coordinate).
#' @param w Crop width in pixels.
#' @param h Crop height in pixels.
#' @return A new image list with the cropped dimensions.
#'
#' @examples
#' \dontrun{
#' img <- render_mesh(cube$vertices, cube$triangles)
#' img <- image_crop(img, 100, 50, 400, 300)
#' }
#'
#' @export
image_crop <- function(image, x, y, w, h) {
    scimesh_image_crop(image, as.integer(x), as.integer(y),
                       as.integer(w), as.integer(h))
}

#' Merge two images side by side or stacked
#'
#' Merges another image into this one at the specified edge.
#' For left/right merging, the heights must match. For top/bottom, the
#' widths must match.
#'
#' @param image An image list.
#' @param other Another image list.
#' @param direction One of \code{"left"}, \code{"right"}, \code{"top"}, \code{"bottom"}.
#' @return A new image list with the merged dimensions.
#'
#' @examples
#' \dontrun{
#' left  <- render_mesh(sphere$vertices, sphere$triangles)
#' right <- render_mesh(cube$vertices, cube$triangles)
#' merged <- image_merge(left, right, "right")
#' }
#'
#' @export
image_merge <- function(image, other, direction) {
    direction <- match.arg(direction, c("left", "right", "top", "bottom"))
    scimesh_image_merge(image, other, direction)
}

#' Grow an image by adding padding
#'
#' Expands the canvas by adding pixel rows/columns filled with a
#' background colour.
#'
#' @param image An image list.
#' @param top Number of pixel rows to add above.
#' @param bottom Number of pixel rows to add below.
#' @param left Number of pixel columns to add to the left.
#' @param right Number of pixel columns to add to the right.
#' @param background Numeric vector of length 4 with RGBA values in
#'   \code{[0, 1]}.
#' @return A new image list with the expanded dimensions.
#'
#' @examples
#' \dontrun{
#' img <- render_mesh(cube$vertices, cube$triangles)
#' img <- image_grow(img, 10, 10, 20, 20, c(1, 1, 1, 1))
#' }
#'
#' @export
image_grow <- function(image, top, bottom, left, right, background) {
    if (!is.numeric(background) || length(background) != 4) {
        stop("background must be a numeric vector of length 4 (RGBA)")
    }
    scimesh_image_grow(image, as.integer(top), as.integer(bottom),
                       as.integer(left), as.integer(right), background)
}

#' Rotate an image by 90 degrees
#'
#' @param image An image list.
#' @param clockwise Logical, if \code{TRUE} (default) rotates clockwise,
#'   otherwise counter-clockwise.
#' @return A new image list with width and height swapped.
#'
#' @export
image_rotate_90 <- function(image, clockwise = TRUE) {
    scimesh_image_rotate_90(image, isTRUE(clockwise))
}

#' Scale an image (nearest-neighbour)
#'
#' Resizes the image to the given dimensions using nearest-neighbour
#' interpolation.
#'
#' @param image An image list.
#' @param new_width Target width in pixels.
#' @param new_height Target height in pixels.
#' @return A new image list with the new dimensions.
#'
#' @export
image_scale <- function(image, new_width, new_height) {
    scimesh_image_scale(image, as.integer(new_width), as.integer(new_height))
}

#' Crop an image to its content bounding box
#'
#' Removes background-coloured margin from the specified edges of the
#' image. The first non-background pixel found on each edge defines the
#' crop boundary.
#'
#' @param image An image list.
#' @param direction One of \code{"left"}, \code{"right"},
#'   \code{"horizontal"} (both left and right), \code{"top"},
#'   \code{"bottom"}, \code{"vertical"} (both top and bottom), or
#'   \code{"all"} (all four sides).
#' @param background Numeric vector of length 4 with RGBA values in
#'   \code{[0, 1]} defining the background colour to crop away.
#' @return A new image list with cropped dimensions.
#'
#' @examples
#' \dontrun{
#' img <- render_mesh(cube$vertices, cube$triangles,
#'                    options = render_options(background_color = c(0, 0, 0, 0)))
#' img <- image_crop_to_content(img, "all", c(0, 0, 0, 0))
#' }
#'
#' @export
image_crop_to_content <- function(image, direction, background) {
    direction <- match.arg(direction, c("left", "right", "horizontal",
                                        "top", "bottom", "vertical", "all"))
    if (!is.numeric(background) || length(background) != 4) {
        stop("background must be a numeric vector of length 4 (RGBA)")
    }
    scimesh_image_crop_to_content(image, direction, background)
}
