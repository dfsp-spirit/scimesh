# Add an alpha channel to a 3-channel RGB array, producing a 4-channel
# RGBA array. Returns the input unchanged if it already has 4 channels.
ensure_rgba_array <- function(arr) {
    if (dim(arr)[3] >= 4L) {
        return(arr)
    }
    out <- array(1, dim = c(dim(arr)[1], dim(arr)[2], 4L))
    for (ch in seq_len(dim(arr)[3])) {
        out[, , ch] <- arr[, , ch]
    }
    out
}

# Internal: render colorbar strip with optional ticks to PNG, read back as array
.colorbar_render <- function(cols, n_colors, width, height, ticks,
                              tick_labels, data_range, label_cex,
                              background, horizontal, title) {
    tmp <- tempfile(fileext = ".png")
    grDevices::png(tmp, width = width, height = height,
                   bg = grDevices::rgb(background[1], background[2],
                                       background[3], background[4]))
    opar <- graphics::par(mar = c(0, 0, 0, 0), xaxs = "i", yaxs = "i")
    graphics::plot.new()
    graphics::plot.window(xlim = c(0, 1), ylim = c(0, 1))

    if (horizontal) {
        strip_bottom <- 0.20
        strip_top    <- 0.72
        strip_h <- strip_top - strip_bottom
        rect_left <- seq(0, 1 - 1 / n_colors, length.out = n_colors)
        rect_right <- seq(1 / n_colors, 1, length.out = n_colors)
        graphics::rect(rect_left, strip_bottom, rect_right, strip_top,
                       col = cols, border = NA)

        if (!is.null(ticks)) {
            tick_x <- (ticks - data_range[1]) / diff(data_range)
            tick_x <- pmax(0, pmin(1, tick_x))
            tick_len <- 0.06
            graphics::segments(tick_x, strip_bottom, tick_x,
                               strip_bottom - tick_len,
                               lwd = 1.2, col = "#333333")
            if (is.null(tick_labels)) {
                tick_labels <- as.character(signif(ticks, 3))
            }
            graphics::text(tick_x, strip_bottom - tick_len - 0.03,
                           labels = tick_labels,
                           cex = label_cex * 0.65,
                           adj = c(0.5, 1), col = "#333333")
        }

        if (!is.null(title) && nchar(title) > 0L) {
            graphics::text(0.5, strip_top + 0.12,
                           labels = title,
                           cex = label_cex * 0.9,
                           font = 1, adj = c(0.5, 0), col = "#333333")
        }
    } else {
        strip_left   <- 0.20
        strip_right  <- 0.72
        strip_w <- strip_right - strip_left
        y_bottom <- seq(0, 1 - 1 / n_colors, length.out = n_colors)
        y_top <- seq(1 / n_colors, 1, length.out = n_colors)
        x_left <- rep(strip_left, n_colors)
        x_right <- rep(strip_right, n_colors)
        graphics::rect(x_left, y_bottom, x_right, y_top,
                       col = cols, border = NA)

        if (!is.null(ticks)) {
            tick_y <- (ticks - data_range[1]) / diff(data_range)
            tick_y <- pmax(0, pmin(1, tick_y))
            tick_len <- 0.04
            graphics::segments(strip_right, tick_y,
                               strip_right + tick_len, tick_y,
                               lwd = 1.2, col = "#333333")
            if (is.null(tick_labels)) {
                tick_labels <- as.character(signif(ticks, 3))
            }
            graphics::text(strip_right + tick_len + 0.03, tick_y,
                           labels = tick_labels,
                           cex = label_cex * 0.65,
                           adj = c(0, 0.5), col = "#333333")
        }

        if (!is.null(title) && nchar(title) > 0L) {
            graphics::text(strip_right + 0.20, 0.5,
                           labels = title,
                           cex = label_cex * 0.9,
                           font = 1, srt = 90, adj = c(0.5, 0),
                           col = "#333333")
        }
    }

    graphics::par(opar)
    grDevices::dev.off()

    arr <- png::readPNG(tmp)
    unlink(tmp)
    ensure_rgba_array(arr)
}

#' Generate a horizontal colorbar image
#'
#' Creates a horizontal colorbar as a 4-channel RGBA array. The color
#' strip and optional tick labels are rendered to PNG using base R
#' graphics (headless-safe). Returns a 3D array suitable for
#' \code{image_to_array()} or direct composition.
#'
#' @param colormap A vector of colors or a function returning colors
#'   (e.g. \code{grDevices::hcl.colors}).
#' @param n_colors Number of discrete color segments in the gradient.
#' @param width Output width in pixels.
#' @param height Output height in pixels.
#' @param ticks Numeric vector of tick positions in data units
#'   (matching \code{data_range}). If \code{NULL}, ticks are computed
#'   automatically via \code{pretty()} restricted to the data range.
#' @param tick_labels Character vector of tick labels. If \code{NULL},
#'   defaults to formatted tick values.
#' @param data_range The data range that \code{ticks} are specified in.
#'   Defaults to \code{c(0, 1)}.
#' @param label_cex Label size multiplier.
#' @param title Optional title string drawn above the color strip
#'   (horizontal) or to the right (vertical).
#' @param background Background RGBA color (0-1 scale).
#' @return A 3D array of dimensions (height, width, 4) with values in
#'   \code{[0, 1]}.
#'
#' @export
colorbar_horizontal <- function(colormap, n_colors = 256L,
                                 width = 600L, height = 80L,
                                 ticks = NULL, tick_labels = NULL,
                                 data_range = c(0, 1),
                                 label_cex = 1.0,
                                 title = NULL,
                                 background = c(1, 1, 1, 1)) {
    if (is.function(colormap)) {
        cols <- colormap(n_colors)
    } else {
        cols <- grDevices::colorRampPalette(colormap)(n_colors)
    }
    if (!is.character(cols) || length(cols) != n_colors) {
        stop("colormap must produce ", n_colors, " colors")
    }
    if (is.null(ticks)) {
        tick_candidates <- pretty(data_range, n = 5)
        ticks <- tick_candidates[tick_candidates >= data_range[1] &
                                 tick_candidates <= data_range[2]]
    }
    .colorbar_render(cols, n_colors, width, height, ticks,
                     tick_labels, data_range, label_cex,
                     background, horizontal = TRUE, title = title)
}

#' Generate a vertical colorbar image
#'
#' Creates a vertical colorbar as a 4-channel RGBA array.
#'
#' @param colormap A vector of colors or a function returning colors.
#' @param n_colors Number of discrete color segments in the gradient.
#' @param width Output width in pixels.
#' @param height Output height in pixels.
#' @param ticks Numeric vector of tick positions in data units.
#'   If \code{NULL}, ticks are computed automatically via
#'   \code{pretty()} restricted to the data range.
#' @param tick_labels Character vector of tick labels.
#' @param data_range The data range that \code{ticks} are specified in.
#' @param label_cex Label size multiplier.
#' @param title Optional title string drawn to the right of the
#'   color strip.
#' @param background Background RGBA color (0-1 scale).
#' @return A 3D array of dimensions (height, width, 4) with values in
#'   \code{[0, 1]}.
#'
#' @export
colorbar_vertical <- function(colormap, n_colors = 256L,
                               width = 80L, height = 600L,
                               ticks = NULL, tick_labels = NULL,
                               data_range = c(0, 1),
                               label_cex = 1.0,
                               title = NULL,
                               background = c(1, 1, 1, 1)) {
    if (is.function(colormap)) {
        cols <- colormap(n_colors)
    } else {
        cols <- grDevices::colorRampPalette(colormap)(n_colors)
    }
    if (!is.character(cols) || length(cols) != n_colors) {
        stop("colormap must produce ", n_colors, " colors")
    }
    if (is.null(ticks)) {
        tick_candidates <- pretty(data_range, n = 5)
        ticks <- tick_candidates[tick_candidates >= data_range[1] &
                                 tick_candidates <= data_range[2]]
    }
    .colorbar_render(cols, n_colors, width, height, ticks,
                     tick_labels, data_range, label_cex,
                     background, horizontal = FALSE, title = title)
}

#' Viridis colormap
#'
#' Returns the viridis color palette. A convenience wrapper around
#' \code{grDevices::hcl.colors} that mimics the viridis color scheme
#' without requiring extra packages.
#'
#' @param n Number of colors.
#' @param alpha Alpha channel value (0-1).
#' @param direction Forward (1) or reversed (-1) direction.
#' @return A character vector of hex color strings.
#'
#' @export
viridis_colormap <- function(n, alpha = 1, direction = 1) {
    cols <- grDevices::hcl.colors(n, palette = "viridis", alpha = alpha,
                                   rev = direction < 0)
    cols
}

#' Custom diverging colormap for neuroimaging
#'
#' Returns a blue-white-red diverging colormap suitable for displaying
#' signed morphometry data (e.g. cortical thickness Z-scores).
#'
#' @param n Number of colors.
#' @return A character vector of hex color strings.
#'
#' @export
diverging_colormap <- function(n) {
    half <- ceiling(n / 2)
    low <- grDevices::colorRampPalette(c("#2166AC", "#F7F7F7"))(half)
    high <- grDevices::colorRampPalette(c("#F7F7F7", "#B2182B"))(
        n - half + 1)[-1]
    c(low, high)
}
