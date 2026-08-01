#' Apply a colormap to numerical data
#'
#' Maps numeric per-vertex (or per-element) data to RGBA colours using a
#' colormap. Handles multi-dataset data (e.g., two brain hemispheres),
#' NaN values, and optional outlier clipping via winsorizing.
#'
#' @param data A numeric vector, or a list of numeric vectors for
#'   multi-dataset mapping (e.g., \code{list(lh_data, rh_data)}).
#' @param colormap A colormap specification. Can be: (1) a function
#'   \code{f(n)} that returns \code{n} hex colour strings (e.g.,
#'   \code{viridis_colormap}), (2) a character vector of hex colour
#'   strings, or (3) an Nx3 or Nx4 numeric matrix of RGBA values in
#'   [0, 1]. Default is \code{viridis_colormap(256L)}.
#' @param limits Controls how the data value range is determined:
#'   \itemize{
#'     \item \code{NULL} (default): auto-detect from finite values
#'       (after winsorizing, if applicable).
#'     \item \code{c(min, max)}: explicit fixed range shared by all
#'       datasets.
#'     \item \code{"global"}: pool all finite values across all datasets
#'       to compute a single shared range.
#'     \item \code{"each"}: each dataset gets its own independent range
#'       (equivalent to \code{limits=NULL, global_range=FALSE}).
#'   }
#' @param nan_color Numeric vector of length 3 or 4 specifying the RGBA
#'   colour for \code{NaN} or \code{NA} values. Values are in [0, 1].
#'   Default is \code{c(0.5, 0.5, 0.5, 1.0)} (opaque mid-grey).
#' @param winsor_percentiles A numeric vector \code{c(lower, upper)}
#'   specifying percentile cutoffs for outlier clipping. Values below the
#'   lower percentile are clamped to the lower cutoff; values above the
#'   upper percentile are clamped to the upper cutoff. Applied before
#'   computing the effective range. \code{NULL} (default) disables
#'   winsorizing. Example: \code{c(0.02, 0.98)} clips the lowest 2% and
#'   highest 2% of values.
#'
#' @return If \code{data} is a single numeric vector, returns an Nx4
#'   numeric matrix of RGBA colours (one row per data value). If
#'   \code{data} is a list, returns a list of Nx4 matrices.
#'
#'   The returned object carries attributes with metadata:
#'   \itemize{
#'     \item For single-dataset results:
#'       \code{data_min}, \code{data_max}, \code{raw_min}, \code{raw_max},
#'       \code{winsor_lo}, \code{winsor_hi}, \code{nan_count}.
#'     \item For multi-dataset results:
#'       \code{pooled_data_min}, \code{pooled_data_max} (use for a shared
#'       colourbar), \code{data_ranges} (per-dataset min/max),
#'       \code{winsor_cutoffs}, \code{nan_counts}.
#'   }
#'
#' @examples
#' # Single dataset — no winsorizing
#' data <- c(1.2, 3.4, NA, 2.1, 5.0, 2.8)
#' colors <- apply_colormap(data)
#' # attr(colors, "data_min")  → effective min
#' # attr(colors, "data_max")  → effective max
#'
#' # Single dataset — with outlier clipping
#' noisy <- c(rnorm(95, mean=50, sd=10), 200, -50)  # two outliers
#' colors <- apply_colormap(noisy, winsor_percentiles = c(0.02, 0.98))
#'
#' # Two hemispheres — shared colour scale
#' lh_data <- c(2.3, 2.1, NA, 3.4)   # left hemisphere
#' rh_data <- c(2.5, 2.0, NA, 3.1)   # right hemisphere
#' colors <- apply_colormap(
#'     list(lh_data, rh_data),
#'     colormap = viridis_colormap(256L),
#'     limits = "global",
#'     winsor_percentiles = c(0.02, 0.98),
#'     nan_color = c(1, 1, 1, 1)  # white for missing (medial wall)
#' )
#' # colors[[1]] — RGBA for left hemisphere
#' # colors[[2]] — RGBA for right hemisphere
#' # attr(colors, "pooled_data_min/max") — use for colourbar
#'
#' # Use with render_mesh
#' \dontrun{
#' mesh_colors <- apply_colormap(thickness_data,
#'                               winsor_percentiles = c(0.02, 0.98))
#' render_mesh(vertices, triangles,
#'             colors = mesh_colors,
#'             camera = cam, options = opts)
#' }
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
