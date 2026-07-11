# Internal: scale a 3D (HxWxC) array to new dimensions using bilinear
# interpolation along each axis. Handles edge cases where source
# dimensions are 1.
.scale_image_2d <- function(img, new_h, new_w) {
    old_h <- dim(img)[1L]
    old_w <- dim(img)[2L]
    chs <- dim(img)[3L]

    if (old_h == new_h && old_w == new_w) {
        return(img)
    }

    result <- array(0, dim = c(new_h, new_w, chs))
    old_x <- seq_len(old_w)
    old_y <- seq_len(old_h)
    new_x <- seq(1, old_w, length.out = new_w)
    new_y <- seq(1, old_h, length.out = new_h)

    for (ch in seq_len(chs)) {
        channel <- img[, , ch]

        if (old_h == 1L && old_w == 1L) {
            result[, , ch] <- channel[1L, 1L]
            next
        }
        if (old_h == 1L) {
            result[, , ch] <- matrix(
                approx(old_x, channel[1L, ], xout = new_x)$y,
                nrow = new_h, ncol = new_w, byrow = TRUE)
            next
        }
        if (old_w == 1L) {
            result[, , ch] <- matrix(
                approx(old_y, channel[, 1L], xout = new_y)$y,
                nrow = new_h, ncol = new_w, byrow = FALSE)
            next
        }

        interp_rows <- matrix(0, nrow = old_h, ncol = new_w)
        for (r in seq_len(old_h)) {
            interp_rows[r, ] <- approx(old_x, channel[r, ],
                                       xout = new_x)$y
        }
        for (c in seq_len(new_w)) {
            result[, c, ch] <- approx(old_y, interp_rows[, c],
                                      xout = new_y)$y
        }
    }
    pmax(0, pmin(1, result))
}

#' Compose multiple images into a single figure
#'
#' Arranges rendered images (from \code{render_mesh()} or
#' \code{render_scene()}) into a grid layout and optionally appends
#' a colorbar. All composition is done with pure R array operations.
#'
#' @param images A list of images, each a list with \code{width},
#'   \code{height}, \code{pixels} as returned by \code{render_mesh()}.
#' @param nrow Number of rows in the grid layout.
#' @param ncol Number of columns in the grid layout. If both
#'   \code{nrow} and \code{ncol} are \code{NULL}, a square-ish layout
#'   is chosen automatically.
#' @param colorbar Optional colorbar array (from
#'   \code{colorbar_horizontal()} or \code{colorbar_vertical()}).
#'   Placed below if horizontal, to the right if vertical.
#' @param colorbar_height Height of the colorbar row in pixels. Only
#'   used when appending a horizontal colorbar.
#' @param colorbar_width Width of the colorbar column in pixels. Only
#'   used when appending a vertical colorbar.
#' @param background Background RGBA color for padding (0-1 scale).
#' @return A list with \code{width}, \code{height}, \code{pixels}
#'   suitable for \code{write_png()} or \code{image_to_array()}.
#'
#' @export
compose_layout <- function(images, nrow = NULL, ncol = NULL,
                           colorbar = NULL,
                           colorbar_height = 80L,
                           colorbar_width = 80L,
                           background = c(0, 0, 0, 0)) {
    if (!is.list(images) || length(images) == 0L) {
        stop("images must be a non-empty list of renderer output images")
    }

    arrays <- lapply(images, image_to_array)

    if (length(arrays) == 1L && is.null(colorbar)) {
        return(images[[1L]])
    }

    cell_h <- dim(arrays[[1L]])[1L]
    cell_w <- dim(arrays[[1L]])[2L]

    for (i in seq_along(arrays)) {
        if (dim(arrays[[i]])[1L] != cell_h || dim(arrays[[i]])[2L] != cell_w) {
            stop("All images must have the same dimensions for composition. ",
                 "Image ", i, " has dimensions ",
                 paste(dim(arrays[[i]]), collapse = "x"),
                 " but expected ", cell_h, "x", cell_w)
        }
    }

    if (is.null(nrow) && is.null(ncol)) {
        ncol <- ceiling(sqrt(length(arrays)))
        nrow <- ceiling(length(arrays) / ncol)
    } else if (is.null(nrow)) {
        nrow <- ceiling(length(arrays) / ncol)
    } else if (is.null(ncol)) {
        ncol <- ceiling(length(arrays) / nrow)
    }

    bg_color <- as.numeric(background)
    bg_r <- bg_color[1]; bg_g <- bg_color[2]
    bg_b <- bg_color[3]; bg_a <- if (length(bg_color) > 3) bg_color[4] else 1

    full_w <- ncol * cell_w
    full_h <- nrow * cell_h

    composite <- array(c(bg_r, bg_g, bg_b, bg_a),
                       dim = c(full_h, full_w, 4L))
    for (r in seq_len(nrow)) {
        for (c_idx in seq_len(ncol)) {
            i <- (r - 1L) * ncol + c_idx
            if (i > length(arrays)) break
            row_start <- (r - 1L) * cell_h + 1L
            row_end <- r * cell_h
            col_start <- (c_idx - 1L) * cell_w + 1L
            col_end <- c_idx * cell_w
            composite[row_start:row_end, col_start:col_end, ] <-
                arrays[[i]]
        }
    }

    if (!is.null(colorbar)) {
        if (!is.array(colorbar) || length(dim(colorbar)) != 3L ||
            dim(colorbar)[3L] < 3L) {
            stop("colorbar must be a 3D array with at least 3 channels")
        }

        colorbar <- ensure_rgba_array(colorbar)

        cbar_h <- dim(colorbar)[1L]
        cbar_w <- dim(colorbar)[2L]
        is_horizontal <- cbar_w > cbar_h

        if (is_horizontal) {
            cbar_scaled_w <- full_w
            cbar_scaled_h <- colorbar_height
            scaled_cbar <- .scale_image_2d(
                colorbar, cbar_scaled_h, cbar_scaled_w)

            new_h <- full_h + cbar_scaled_h
            new_composite <- array(c(bg_r, bg_g, bg_b, bg_a),
                                   dim = c(new_h, full_w, 4L))
            new_composite[1L:full_h, , ] <- composite
            new_composite[(full_h + 1L):new_h, , ] <- scaled_cbar
            composite <- new_composite
        } else {
            cbar_scaled_h <- full_h
            cbar_scaled_w <- colorbar_width
            scaled_cbar <- .scale_image_2d(
                colorbar, cbar_scaled_h, cbar_scaled_w)

            new_w <- full_w + cbar_scaled_w
            new_composite <- array(c(bg_r, bg_g, bg_b, bg_a),
                                   dim = c(full_h, new_w, 4L))
            new_composite[, 1L:full_w, ] <- composite
            new_composite[, (full_w + 1L):new_w, ] <- scaled_cbar
            composite <- new_composite
        }
    }

    out_h <- dim(composite)[1L]
    out_w <- dim(composite)[2L]
    pixels <- as.raw(round(composite * 255))

    list(
        width = out_w,
        height = out_h,
        pixels = pixels
    )
}

#' Stack images vertically
#'
#' Stacks a list of rendered images vertically (one below another).
#' A convenience wrapper around \code{compose_layout()}.
#'
#' @param ... Images from \code{render_mesh()} or
#'   \code{render_scene()}, or a list of images.
#' @param colorbar Optional colorbar from \code{colorbar_horizontal()}
#'   or \code{colorbar_vertical()}.
#' @param background Background RGBA color for padding.
#' @return A composed image list.
#'
#' @export
stack_vertical <- function(..., colorbar = NULL,
                           colorbar_height = 80L,
                           colorbar_width = 80L,
                           background = c(0, 0, 0, 0)) {
    images <- list(...)
    if (length(images) == 1L && is.list(images[[1L]]) &&
        !is.null(images[[1L]]$pixels)) {
        images <- list(images[[1L]])
    } else if (length(images) == 1L && is.list(images[[1L]])) {
        images <- images[[1L]]
    }
    compose_layout(images, ncol = 1L, colorbar = colorbar,
                   colorbar_height = colorbar_height,
                   colorbar_width = colorbar_width,
                   background = background)
}

#' Stack images horizontally
#'
#' Stacks a list of rendered images side by side. A convenience
#' wrapper around \code{compose_layout()}.
#'
#' @param ... Images from \code{render_mesh()} or
#'   \code{render_scene()}, or a list of images.
#' @param colorbar Optional colorbar.
#' @param background Background RGBA color for padding.
#' @return A composed image list.
#'
#' @export
stack_horizontal <- function(..., colorbar = NULL,
                              colorbar_height = 80L,
                              colorbar_width = 80L,
                              background = c(0, 0, 0, 0)) {
    images <- list(...)
    if (length(images) == 1L && is.list(images[[1L]]) &&
        !is.null(images[[1L]]$pixels)) {
        images <- list(images[[1L]])
    } else if (length(images) == 1L && is.list(images[[1L]])) {
        images <- images[[1L]]
    }
    compose_layout(images, nrow = 1L, colorbar = colorbar,
                   colorbar_height = colorbar_height,
                   colorbar_width = colorbar_width,
                   background = background)
}
