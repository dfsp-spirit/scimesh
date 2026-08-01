#' Apply a colormap to numerical data
#'
#' Maps numeric per-vertex (or per-element) data to RGBA colours using a
#' colormap. Handles multi-dataset data (e.g., two brain hemispheres),
#' NaN values, and optional outlier clipping via winsorizing.
#'
#' @param data A numeric vector, or a list of numeric vectors for
#'   multi-dataset mapping (e.g., list(lh_data, rh_data)).
#' @param colormap A colormap specification: a function f(n) returning
#'   n hex colour strings (e.g., viridis_colormap), a character vector
#'   of hex colours, or an Nx3/Nx4 matrix of RGBA values in [0,1].
#'   Default is viridis_colormap(256L).
#' @param limits How the data value range is determined. NULL (default)
#'   auto-detects from finite values after winsorizing. c(min, max)
#'   sets an explicit fixed range. "global" pools all datasets for a
#'   shared range. "each" uses independent per-dataset ranges.
#' @param nan_color RGBA colour for NaN/NA values as a length-3 (RGB)
#'   or length-4 (RGBA) numeric vector in [0,1]. Default mid-grey.
#' @param winsor_percentiles Optional c(lower, upper) percentiles for
#'   outlier clipping, e.g. c(0.02, 0.98). NULL disables winsorizing.
#'
#' @return If data is a single vector: an Nx4 numeric matrix of RGBA
#'   colours. If data is a list: a list of Nx4 matrices.
#'
#'   Attributes on the result provide metadata. Single-dataset:
#'   data_min, data_max, raw_min, raw_max, winsor_lo, winsor_hi,
#'   nan_count. Multi-dataset: pooled_data_min, pooled_data_max (use
#'   for a colourbar), data_ranges, winsor_cutoffs, nan_counts.
#'
#' @examples
#' data <- c(1.2, 3.4, NA, 2.1, 5.0, 2.8)
#' colors <- apply_colormap(data)
#'
#' noisy <- c(rnorm(95, mean = 50, sd = 10), 200, -50)
#' colors <- apply_colormap(noisy, winsor_percentiles = c(0.02, 0.98))
#'
#' lh <- c(2.3, 2.1, NA, 3.4)
#' rh <- c(2.5, 2.0, NA, 3.1)
#' 
#' colors <- apply_colormap(list(lh, rh),
#'     colormap = viridis_colormap(256L),
#'     limits = "global",
#'     winsor_percentiles = c(0.02, 0.98),
#'     nan_color = c(1, 1, 1, 1))
#'
#' @export
apply_colormap <- function(
    data,
    colormap = viridis_colormap(256L),
    limits = NULL,
    nan_color = c(0.5, 0.5, 0.5, 1.0),
    winsor_percentiles = NULL
) {
    # --- Resolve colormap to an Nx3 or Nx4 numeric matrix ---
    if (is.function(colormap)) {
        cmap_mat <- .colormap_to_matrix(colormap(256L))
    } else if (is.character(colormap)) {
        cmap_mat <- .colormap_to_matrix(colormap)
    } else if (is.matrix(colormap)) {
        cmap_mat <- colormap
    } else {
        stop("'colormap' must be a function, character vector of hex colours, ",
             "or numeric matrix")
    }
    if (ncol(cmap_mat) < 3L || ncol(cmap_mat) > 4L) {
        stop("Colormap matrix must have 3 or 4 columns (RGB or RGBA)")
    }

    # --- Resolve nan_color to length 4 ---
    if (length(nan_color) == 3L) {
        nan_color <- c(nan_color, 1.0)
    } else if (length(nan_color) != 4L) {
        stop("'nan_color' must have length 3 (RGB) or 4 (RGBA)")
    }

    # --- Resolve limits ---
    use_global <- FALSE
    explicit_limits <- NULL
    if (!is.null(limits)) {
        if (is.character(limits)) {
            if (identical(limits, "global")) {
                use_global <- TRUE
            } else if (!identical(limits, "each")) {
                stop("'limits' string must be \"global\" or \"each\"")
            }
        } else if (is.numeric(limits) && length(limits) >= 2L) {
            explicit_limits <- as.numeric(limits[1:2])
        } else {
            stop("'limits' must be NULL, c(min,max), \"global\", or \"each\"")
        }
    }

    # --- Resolve winsor_percentiles ---
    wp <- NULL
    if (!is.null(winsor_percentiles)) {
        if (!is.numeric(winsor_percentiles) || length(winsor_percentiles) < 2L) {
            stop("'winsor_percentiles' must be a numeric vector c(lower, upper)")
        }
        wp <- as.numeric(winsor_percentiles[1:2])
    }

    # --- Call single or multi C++ backend ---
    if (is.list(data) && !is.data.frame(data) && !is.matrix(data)) {
        # Multi-dataset
        result <- scimesh_apply_colormap_multi(
            data_list    = data,
            colormap_lut = cmap_mat,
            global_range = use_global,
            limits       = explicit_limits,
            nan_color    = nan_color,
            winsor_pct   = wp
        )
        # Attach class for S3 dispatch but keep as list of matrices
        class(result) <- c("scimesh_colormap_result", class(result))
        return(result)
    } else {
        # Single dataset
        data_vec <- as.numeric(data)
        result <- scimesh_apply_colormap_single(
            data         = data_vec,
            colormap_lut = cmap_mat,
            limits       = explicit_limits,
            nan_color    = nan_color,
            winsor_pct   = wp
        )
        # Extract the matrix and attach attributes
        colors <- result[["colors"]]
        attr(colors, "data_min")  <- result[["data_min"]]
        attr(colors, "data_max")  <- result[["data_max"]]
        attr(colors, "raw_min")   <- result[["raw_min"]]
        attr(colors, "raw_max")   <- result[["raw_max"]]
        attr(colors, "winsor_lo") <- result[["winsor_lo"]]
        attr(colors, "winsor_hi") <- result[["winsor_hi"]]
        attr(colors, "nan_count") <- result[["nan_count"]]
        return(colors)
    }
}

# Internal: convert hex colour strings to Nx3 matrix of [0,1] RGB
.colormap_to_matrix <- function(hex_cols) {
    if (is.character(hex_cols)) {
        # Strip leading '#' if present
        hex_clean <- gsub("^#", "", hex_cols)
        if (any(nchar(hex_clean) != 6L)) {
            stop("Hex colours must be 6-digit strings (e.g., '#FF0000' or 'FF0000')")
        }
        rgb_mat <- t(grDevices::col2rgb(paste0("#", hex_clean), alpha = FALSE)) / 255
        return(rgb_mat)
    }
    stop("Invalid colormap format")
}
