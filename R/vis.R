# remotes::namespacing of neuro vs domain-agnostic functions --------
#
# Domain-agnostic (render_*, camera_*, colorbar_*, compose_*, *_colormap):
#   Know nothing about brains, hemispheres, or FreeSurfer.  They operate
#   on generic triangular meshes and RGBA images.
#
# Neuro-specific (vis.subject.*, vis.view_*, vis.hemi_*):
#   Know about RAS coordinates, FreeSurfer directory layout, hemisphere
#   conventions, cortex masking, and anatomical view names.  These load
#   data via freesurferformats and delegate rendering to the agnostic
#   layer.
#
# The boundary is at the Mesh/Scene level: once per-vertex data has been
# mapped to RGBA colors and packed into a scimesh Mesh, everything
# downstream is domain-agnostic.

# ---- internal helpers ------------------------------------------------

# FreeSurfer file path in standard recon-all layout.
.fspath <- function(subjects_dir, subject, hemi, measure, surface) {
    file.path(subjects_dir, subject, "surf",
              paste0(hemi, ".", measure))
}

.fspath_surf <- function(subjects_dir, subject, hemi, surface) {
    file.path(subjects_dir, subject, "surf",
              paste0(hemi, ".", surface))
}

.fspath_label <- function(subjects_dir, subject, hemi, label) {
    file.path(subjects_dir, subject, "label",
              paste0(hemi, ".", label, ".label"))
}

# Load a single hemisphere: surface mesh, morphometry data, and optional
# cortex mask.  Returns a scimesh Mesh with vertex colors.
.load_fs_hemi <- function(subjects_dir, subject, hemi, measure,
                           surface, cortex_only) {
    if (!requireNamespace("freesurferformats", quietly = TRUE)) {
        stop("The 'freesurferformats' package is required to load ",
             "FreeSurfer data. Install it with: ",
             "remotes::install_github(\"dfsp-spirit/freesurferformats\")")
    }

    surf_file <- .fspath_surf(subjects_dir, subject, hemi, surface)
    if (!file.exists(surf_file)) {
        stop("Surface file not found: ", surf_file)
    }
    fs_surf <- freesurferformats::read.fs.surface(surf_file)

    morph_file <- .fspath(subjects_dir, subject, hemi, measure, surface)
    if (!file.exists(morph_file)) {
        # Try several extensions
        for (ext in c("", ".mgh", ".mgz")) {
            candidate <- paste0(morph_file, ext)
            if (file.exists(candidate)) {
                morph_file <- candidate
                break
            }
        }
    }
    if (!file.exists(morph_file)) {
        stop("Morphometry file not found: ", morph_file)
    }
    morph_data <- freesurferformats::read.fs.morph(morph_file)

    nv <- nrow(fs_surf$vertices)
    if (length(morph_data) != nv) {
        stop("Morphometry data length (", length(morph_data),
             ") does not match vertex count (", nv, ")")
    }

    if (isTRUE(cortex_only)) {
        label_file <- .fspath_label(subjects_dir, subject, hemi, "cortex")
        if (!file.exists(label_file)) {
            warning("Cortex label not found: ", label_file,
                    "  Skipping cortex masking.")
        } else {
            cortex_verts <- freesurferformats::read.fs.label(label_file)
            mask <- rep(FALSE, nv)
            mask[cortex_verts] <- TRUE
            morph_data[!mask] <- NA_real_
        }
    }

    list(
        vertices = fs_surf$vertices,
        faces    = fs_surf$faces,
        morph    = morph_data,
        hemi     = hemi
    )
}

# Resolve human-friendly view names to internal camera descriptions.
# Each camera is a list(direction=c(x,y,z), up=c(x,y,z), label=string).
.resolve_views <- function(views, hemi) {
    if (is.null(views) || length(views) == 0L) {
        stop("views must be a non-empty character vector or a recognised ",
             "shortcut like 't4'")
    }

    all_views <- c()

    for (v in views) {
        if (v == "t4") {
            all_views <- c(all_views, c("lateral_lh", "lateral_rh",
                                         "medial_lh", "medial_rh"))
        } else if (v == "t3") {
            # 3 views: lateral + medial lh, lateral rh
            all_views <- c(all_views, c("lateral_lh", "medial_lh",
                                         "lateral_rh"))
        } else if (v == "t9") {
            all_views <- c(all_views, c(
                "lateral_lh", "lateral_rh",
                "medial_lh", "medial_rh",
                "dorsal", "ventral",
                "anterior", "posterior",
                "rostral"))
        } else if (v == "si") {
            # single interactive-equivalent: default to lateral_lh for lh, lateral_rh for rh
            if (hemi == "lh") {
                all_views <- c(all_views, "lateral_lh")
            } else if (hemi == "rh") {
                all_views <- c(all_views, "lateral_rh")
            } else {
                all_views <- c(all_views, "lateral_lh")
            }
        } else {
            # canonicalise: allow sd_lateral_lh -> lateral_lh, etc.
            vc <- .canonical_view(v)
            all_views <- c(all_views, vc)
        }
    }

    unique(all_views)
}

# Map a canonical view name to a camera (direction, up, label).
.view_to_camera <- function(view_name, mesh) {
    # Normalise: strip prefixes like "sd_", "si_", "sr_"
    clean <- sub("^(sd_|si_|sr_)", "", view_name)

    lookup <- list(
        lateral_lh      = list(dir = c(-1,  0,  0), up = c(0, 0, 1),
                                label = "Left lateral"),
        medial_lh       = list(dir = c( 1,  0,  0), up = c(0, 0, 1),
                                label = "Left medial"),
        lateral_rh      = list(dir = c( 1,  0,  0), up = c(0, 0, 1),
                                label = "Right lateral"),
        medial_rh       = list(dir = c(-1,  0,  0), up = c(0, 0, 1),
                                label = "Right medial"),
        dorsal          = list(dir = c( 0,  0,  1), up = c(0, 1, 0),
                                label = "Dorsal"),
        ventral         = list(dir = c( 0,  0, -1), up = c(0, 1, 0),
                                label = "Ventral"),
        anterior        = list(dir = c( 0,  1,  0), up = c(0, 0, 1),
                                label = "Anterior"),
        posterior       = list(dir = c( 0, -1,  0), up = c(0, 0, 1),
                                label = "Posterior"),
        rostral         = list(dir = c( 0,  1,  0), up = c(0, 0, 1),
                                label = "Rostral"),
        caudal          = list(dir = c( 0, -1,  0), up = c(0, 0, 1),
                                label = "Caudal")
    )

    if (is.null(lookup[[clean]])) {
        stop("Unknown view: ", view_name, ". Valid views: ",
             paste(names(lookup), collapse = ", "))
    }

    cam_spec <- lookup[[clean]]

    # Pick the mesh to fit the camera to.  For hemi-specific views we fit
    # only that hemisphere so the camera is as close as possible.  For
    # global views (dorsal, ventral, etc.) we fit the whole scene.
    fit_mesh <- NULL
    if (is.list(mesh) && !is.null(mesh$vertices)) {
        fit_mesh <- mesh  # single mesh already
    } else if (is.list(mesh) && length(mesh) > 0L) {
        view_hemi <- NULL
        if (grepl("_lh$", clean)) {
            view_hemi <- "lh"
        } else if (grepl("_rh$", clean)) {
            view_hemi <- "rh"
        }
        if (!is.null(view_hemi) && !is.null(mesh[[view_hemi]])) {
            fit_mesh <- mesh[[view_hemi]]
        } else {
            # Global view or hemi key not present — use the first mesh
            fit_mesh <- mesh[[1L]]
        }
    }

    if (is.null(fit_mesh) || is.null(fit_mesh$vertices)) {
        stop("mesh must be a mesh descriptor list with 'vertices'")
    }

    cam <- camera_auto(fit_mesh, direction = cam_spec$dir,
                       up = cam_spec$up, margin = 1.02)

    attr(cam, "label") <- cam_spec$label
    cam
}

.canonical_view <- function(v) {
    # strip sd_ / si_ / sr_ prefix
    sub("^(sd_|si_|sr_)", "", v)
}

# Map a data range to colors using the user-supplied (or default) colormap.
.apply_colormap <- function(morph_data, makecmap_options) {
    col_fn <- makecmap_options$colFn
    if (is.null(col_fn)) {
        col_fn <- viridis_colormap
    }
    n <- makecmap_options$n
    if (is.null(n)) n <- 256L

    finite_vals <- morph_data[!is.na(morph_data)]
    if (length(finite_vals) == 0L) {
        # all NA: use a dummy color
        cols <- rep("#FFFFFF", length(morph_data))
        return(cols)
    }

    lo <- makecmap_options$range[1L]
    hi <- makecmap_options$range[2L]
    if (is.null(lo) || is.na(lo)) lo <- min(finite_vals)
    if (is.null(hi) || is.na(hi)) hi <- max(finite_vals)

    if (isTRUE(makecmap_options$symm)) {
        mx <- max(abs(lo), abs(hi))
        lo <- -mx
        hi <-  mx
    }

    rng <- hi - lo
    if (rng == 0) rng <- 1

    palette <- col_fn(n)

    na_color <- makecmap_options$col.na
    if (is.null(na_color)) na_color <- "#FFFFFF"

    colors <- character(length(morph_data))
    for (i in seq_along(morph_data)) {
        if (is.na(morph_data[i])) {
            colors[i] <- na_color
        } else {
            t_val <- (morph_data[i] - lo) / rng
            t_val <- max(0, min(1, t_val))
            idx <- as.integer(t_val * (n - 1L)) + 1L
            if (idx < 1L) idx <- 1L
            if (idx > n) idx <- n
            colors[i] <- palette[idx]
        }
    }
    colors
}

# Convert hex colors (#RRGGBB or #RRGGBBAA) to Nx4 float matrix (0-1).
.hex_to_rgba_matrix <- function(hex_colors) {
    n <- length(hex_colors)
    mat <- matrix(0, nrow = n, ncol = 4L)
    if (n == 0L) return(mat)

    clean <- sub("^#", "", hex_colors)
    # pad 6-char codes to 8 so the alpha extraction always succeeds
    short <- nchar(clean) == 6L
    if (any(short)) {
        clean[short] <- paste0(clean[short], "FF")
    }

    r <- as.integer(paste0("0x", substr(clean, 1L, 2L))) / 255
    g <- as.integer(paste0("0x", substr(clean, 3L, 4L))) / 255
    b <- as.integer(paste0("0x", substr(clean, 5L, 6L))) / 255
    a <- as.integer(paste0("0x", substr(clean, 7L, 8L))) / 255

    cbind(r, g, b, a)
}

# Build internal Mesh descriptor list for each hemi from loaded data.
.build_mesh <- function(hemi_data, makecmap_options) {
    colors <- .apply_colormap(hemi_data$morph, makecmap_options)
    rgba <- .hex_to_rgba_matrix(colors)

    list(
        vertices  = hemi_data$vertices,
        triangles = hemi_data$faces,
        colors    = rgba,
        hemi      = hemi_data$hemi
    )
}

# ---- public neuro-specific functions --------------------------------

#' Visualize native-space morphometry data on brain surfaces
#'
#' Load FreeSurfer surface and morphometry data for a subject, map the
#' per-vertex values to colors, and render the result from one or more
#' standard anatomical views.  Returns an in-memory image — no
#' interactive 3D window or separate export step needed.
#'
#' @param subjects_dir Path to the FreeSurfer SUBJECTS_DIR.
#' @param subject_id Subject identifier (folder name).
#' @param measure Morphometry measure name (e.g. \code{"thickness"},
#'   \code{"sulc"}, \code{"curv"}, \code{"area"}).  The corresponding
#'   file \code{surf/<hemi>.<measure>} is loaded.
#' @param hemi One of \code{"lh"}, \code{"rh"}, or \code{"both"}.
#' @param surface Surface name (e.g. \code{"white"}, \code{"pial"},
#'   \code{"inflated"}).  Default is \code{"white"}.
#' @param views Character vector of view names.  Supports fsbrain-style
#'   shortcuts like \code{"t4"} (2×2 tile), \code{"t9"}, \code{"si"},
#'   and single views like \code{"lateral_lh"}, \code{"medial_rh"},
#'   \code{"dorsal"}, \code{"ventral"}, \code{"anterior"},
#'   \code{"posterior"}.  Prefixed forms like \code{"sd_lateral_lh"}
#'   are also accepted.
#' @param cortex_only Logical.  If \code{TRUE}, non-cortex (medial wall)
#'   vertices are masked to \code{NA} using the subject's
#'   \code{label/<hemi>.cortex.label} files.
#' @param draw_colorbar Logical.  If \code{TRUE}, a horizontal colorbar
#'   is appended below the rendered views.
#' @param makecmap_options A named list of colormap options:
#'   \describe{
#'     \item{colFn}{Colormap function, e.g. \code{viridis::viridis}.
#'       Defaults to \code{scimesh::viridis_colormap}.}
#'     \item{n}{Number of color levels (default 256).}
#'     \item{range}{Numeric(2) fixing the data range for the colormap.
#'       Defaults to the finite min/max of the data.}
#'     \item{symm}{Logical.  If \code{TRUE}, the range is centred on
#'       zero (useful for signed data like Z-scores).}
#'     \item{col.na}{Hex color for \code{NA} vertices (default white).}
#'   }
#' @param width Width of each sub-image in pixels.
#' @param height Height of each sub-image in pixels.
#' @param background Background RGBA colour (0-1 scale).
#' @param colorbar_height Height of the colorbar strip in pixels.
#' @param ... Additional arguments passed to \code{render_options()}
#'   (e.g. \code{shading}, \code{invert_normals}, \code{wireframe}).
#'
#' @return A list with \code{width}, \code{height}, and \code{pixels}
#'   (raw RGBA vector).  Suitable for \code{write_png()} or
#'   \code{image_to_array()}.
#'
#' @section Domain boundary:
#' This is a **neuro-specific** function. It knows about FreeSurfer
#' directory layout, RAS coordinates, cortex labels, and anatomical view
#' names.  All rendering is delegated to the domain-agnostic
#' \code{render_mesh()} / \code{render_scene()} layer.
#'
#' @export
vis.subject.morph.native <- function(subjects_dir, subject_id, measure,
                                      hemi = c("both", "lh", "rh"),
                                      surface = "white",
                                      views = c("t4"),
                                      cortex_only = FALSE,
                                      draw_colorbar = FALSE,
                                      makecmap_options = list(),
                                      width = 800L, height = 600L,
                                      background = c(1, 1, 1, 1),
                                      colorbar_height = 80L,
                                      ...) {
    hemi <- match.arg(hemi)

    if (!requireNamespace("freesurferformats", quietly = TRUE)) {
        stop("The 'freesurferformats' package is required. ",
             "Install with: ",
             "remotes::install_github(\"dfsp-spirit/freesurferformats\")")
    }

    # ---- Load hemi data ----
    hemi_list <- list()
    if (hemi %in% c("lh", "both")) {
        hemi_list$lh <- .load_fs_hemi(subjects_dir, subject_id, "lh",
                                       measure, surface, cortex_only)
    }
    if (hemi %in% c("rh", "both")) {
        hemi_list$rh <- .load_fs_hemi(subjects_dir, subject_id, "rh",
                                       measure, surface, cortex_only)
    }

    # ---- Resolve views ----
    view_names <- .resolve_views(views, hemi)

    # ---- Build default cmap options ----
    cmap <- makecmap_options
    if (is.null(cmap$colFn))  cmap$colFn  <- viridis_colormap
    if (is.null(cmap$n))      cmap$n      <- 256L
    if (is.null(cmap$col.na)) cmap$col.na <- "#FFFFFF"

    # ---- Build meshes ----
    meshes <- lapply(hemi_list, .build_mesh, makecmap_options = cmap)

    # ---- Render options ----
    ropts <- render_options(width = width, height = height,
                            background_color = background, ...)
    if (is.null(list(...)$backface_culling)) {
        ropts$backface_culling <- FALSE
    }

    # ---- Render each view ----
    images <- list()
    for (vn in view_names) {
        cam <- .view_to_camera(vn, meshes)
        clean <- .canonical_view(vn)
        if (grepl("_lh$", clean) && !is.null(meshes[["lh"]])) {
            scene <- list(meshes[["lh"]])
        } else if (grepl("_rh$", clean) && !is.null(meshes[["rh"]])) {
            scene <- list(meshes[["rh"]])
        } else {
            scene <- meshes
        }
        img <- render_scene(scene, cam, ropts)
        images <- c(images, list(img))
    }

    # ---- Colorbar ----
    cbar <- NULL
    if (isTRUE(draw_colorbar)) {
        all_morph <- unlist(lapply(hemi_list, `[[`, "morph"))
        finite_vals <- all_morph[!is.na(all_morph)]
        lo <- cmap$range[1L]
        hi <- cmap$range[2L]
        if (is.null(lo) || is.na(lo)) lo <- min(finite_vals)
        if (is.null(hi) || is.na(hi)) hi <- max(finite_vals)
        if (isTRUE(cmap$symm)) {
            mx <- max(abs(lo), abs(hi))
            lo <- -mx; hi <- mx
        }

        cbar <- colorbar_horizontal(
            cmap$colFn,
            n_colors = cmap$n,
            width = 600L, height = colorbar_height,
            ticks = signif(c(lo, (lo + hi) / 2, hi), 3),
            data_range = c(lo, hi),
            background = background)
    }

    # ---- Compose layout ----
    compose_layout(images, colorbar = cbar,
                   colorbar_height = colorbar_height,
                   background = background,
                   crop = TRUE)
}
