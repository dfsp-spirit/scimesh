# Internal: crop transparent/empty border from an (H,W,4) array.
# Returns a cropped array and the bounding box list(t, b, l, r) in
# pixel coordinates (1-indexed, inclusive).
.crop_content <- function(arr, threshold = 0.01) {
    alpha <- arr[, , 4L]
    rows <- which(rowSums(alpha > threshold, na.rm = TRUE) > 0L)
    cols <- which(colSums(alpha > threshold, na.rm = TRUE) > 0L)

    if (length(rows) == 0L || length(cols) == 0L) {
        return(list(arr = arr, bbox = list(t = 1L, b = dim(arr)[1L],
                                            l = 1L, r = dim(arr)[2L])))
    }
    t <- min(rows); b <- max(rows)
    l <- min(cols); r <- max(cols)
    list(arr = arr[t:b, l:r, , drop = FALSE], bbox = list(t = t, b = b, l = l, r = r))
}

# Internal: pad a cropped array back to a target size, placing the
# content centred within the canvas.  background is an RGBA vector (0-1).
.pad_to_size <- function(arr, target_h, target_w, background = c(0, 0, 0, 0)) {
    h <- dim(arr)[1L]; w <- dim(arr)[2L]; ch <- dim(arr)[3L]
    if (h == target_h && w == target_w) return(arr)

    canvas <- array(background, dim = c(target_h, target_w, ch))
    # centre the content
    row_off <- max(1L, floor((target_h - h) / 2L) + 1L)
    col_off <- max(1L, floor((target_w - w) / 2L) + 1L)
    row_end <- row_off + h - 1L
    col_end <- col_off + w - 1L
    canvas[row_off:row_end, col_off:col_end, ] <- arr
    canvas
}

# Internal: crop each image in a list, find the maximum cropped
# dimensions, pad all to that size, and return the uniform-sized arrays
# along with the original bounding boxes.
.uniform_crop <- function(arrays, background = c(0, 0, 0, 0)) {
    cropped <- lapply(arrays, .crop_content)

    # find maximum cropped dimensions
    max_h <- max(vapply(cropped, function(x) dim(x$arr)[1L], 0L))
    max_w <- max(vapply(cropped, function(x) dim(x$arr)[2L], 0L))

    padded <- lapply(cropped, function(cr) {
        .pad_to_size(cr$arr, max_h, max_w, background)
    })
    list(arrays = padded, bboxes = lapply(cropped, `[[`, "bbox"))
}

# Internal: crop each image individually and pad to per-row height and
# per-column width.  This gives a tight grid where no cell has more
# padding than necessary, saving white space for publication figures.
#
# Returns padded arrays and the row/column dimension vectors used.
.grid_crop <- function(arrays, nrow, ncol, background = c(0, 0, 0, 0)) {
    n <- length(arrays)
    if (n == 0L) stop("arrays must be non-empty")

    cropped <- lapply(arrays, .crop_content)
    cropped_h <- vapply(cropped, function(x) dim(x$arr)[1L], 0L)
    cropped_w <- vapply(cropped, function(x) dim(x$arr)[2L], 0L)

    row_heights <- integer(nrow)
    for (r in seq_len(nrow)) {
        idx <- ((r - 1L) * ncol + 1L):min(r * ncol, n)
        row_heights[r] <- max(cropped_h[idx])
    }

    col_widths <- integer(ncol)
    for (c in seq_len(ncol)) {
        idx <- seq(c, n, by = ncol)
        col_widths[c] <- max(cropped_w[idx])
    }

    padded <- lapply(seq_len(n), function(i) {
        r <- (i - 1L) %/% ncol + 1L
        c <- (i - 1L) %% ncol + 1L
        .pad_to_size(cropped[[i]]$arr, row_heights[r], col_widths[c],
                     background)
    })

    list(arrays = padded, row_heights = row_heights,
         col_widths = col_widths)
}

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
#' @param crop Logical. If \code{TRUE}, transparent borders are
#'   cropped individually and images are padded to per-row height
#'   and per-column width for a tight layout with minimal white
#'   space. Default is \code{FALSE} (images must be same size).
#' @param colorbar_side For vertical colorbars, whether to place
#'   the bar on the \code{"right"} (default) or \code{"left"} of
#'   the brain images. Ignored for horizontal colorbars.
#' @return A list with \code{width}, \code{height}, \code{pixels}
#'   suitable for \code{write_png()} or \code{image_to_array()}.
#'
#' @export
compose_layout <- function(images, nrow = NULL, ncol = NULL,
                           colorbar = NULL,
                           colorbar_height = 80L,
                           colorbar_width = 80L,
                           background = c(0, 0, 0, 0),
                           colorbar_side = c("right", "left"),
                           crop = FALSE) {
    if (!is.list(images) || length(images) == 0L) {
        stop("images must be a non-empty list of renderer output images")
    }

    colorbar_side <- match.arg(colorbar_side)

    arrays <- lapply(images, image_to_array)

    if (length(arrays) == 1L && is.null(colorbar) && !isTRUE(crop)) {
        return(images[[1L]])
    }

    if (is.null(nrow) && is.null(ncol)) {
        ncol <- ceiling(sqrt(length(arrays)))
        nrow <- ceiling(length(arrays) / ncol)
    } else if (is.null(nrow)) {
        nrow <- ceiling(length(arrays) / ncol)
    } else if (is.null(ncol)) {
        ncol <- ceiling(length(arrays) / nrow)
    }

    if (isTRUE(crop)) {
        gc <- .grid_crop(arrays, nrow, ncol, background = background)
        arrays <- gc$arrays
        row_heights <- gc$row_heights
        col_widths  <- gc$col_widths
    } else {
        cell_h <- dim(arrays[[1L]])[1L]
        cell_w <- dim(arrays[[1L]])[2L]
        for (i in seq_along(arrays)) {
            if (dim(arrays[[i]])[1L] != cell_h ||
                dim(arrays[[i]])[2L] != cell_w) {
                stop("All images must have the same dimensions for composition. ",
                     "Image ", i, " has dimensions ",
                     paste(dim(arrays[[i]]), collapse = "x"),
                     " but expected ", cell_h, "x", cell_w)
            }
        }
        row_heights <- rep(cell_h, nrow)
        col_widths  <- rep(cell_w, ncol)
    }

    if (length(arrays) == 1L && is.null(colorbar)) {
        arr <- arrays[[1L]]
        out_h <- dim(arr)[1L]
        out_w <- dim(arr)[2L]
        pixels <- as.raw(round(aperm(arr, c(3L, 2L, 1L)) * 255))
        return(list(width = out_w, height = out_h, pixels = pixels))
    }

    bg_color <- as.numeric(background)
    bg_r <- bg_color[1]; bg_g <- bg_color[2]
    bg_b <- bg_color[3]; bg_a <- if (length(bg_color) > 3) bg_color[4] else 1

    full_w <- sum(col_widths)
    full_h <- sum(row_heights)

    composite <- array(c(bg_r, bg_g, bg_b, bg_a),
                       dim = c(full_h, full_w, 4L))
    for (r in seq_len(nrow)) {
        row_start <- if (r == 1L) 1L else cumsum(row_heights)[r - 1L] + 1L
        row_end   <- cumsum(row_heights)[r]
        for (c_idx in seq_len(ncol)) {
            i <- (r - 1L) * ncol + c_idx
            if (i > length(arrays)) break
            col_start <- if (c_idx == 1L) 1L
                         else cumsum(col_widths)[c_idx - 1L] + 1L
            col_end   <- cumsum(col_widths)[c_idx]
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
            if (colorbar_side == "left") {
                new_composite[, 1L:cbar_scaled_w, ] <- scaled_cbar
                new_composite[, (cbar_scaled_w + 1L):new_w, ] <- composite
            } else {
                new_composite[, 1L:full_w, ] <- composite
                new_composite[, (full_w + 1L):new_w, ] <- scaled_cbar
            }
            composite <- new_composite
        }
    }

    out_h <- dim(composite)[1L]
    out_w <- dim(composite)[2L]
    # composite is (H,W,4). C++ pixels are (4,W,H) flattened → row-major RGBA.
    pixels <- as.raw(round(aperm(composite, c(3L, 2L, 1L)) * 255))

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
                           background = c(0, 0, 0, 0),
                           colorbar_side = c("right", "left"),
                           crop = FALSE) {
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
                   background = background,
                   colorbar_side = colorbar_side,
                   crop = crop)
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
                              background = c(0, 0, 0, 0),
                              colorbar_side = c("right", "left"),
                              crop = FALSE) {
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
                   background = background,
                   colorbar_side = colorbar_side,
                   crop = crop)
}
