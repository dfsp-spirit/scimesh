# Internal helper for the glTF examples: turn a binary glTF (.glb) file into a
# self-contained, fully offline HTML viewer.
#
# The generated page embeds the .glb (base64) together with the vendored
# three.js / GLTFLoader / OrbitControls classic-script builds (see
# examples/web/) into a single HTML file that can be opened by double-clicking
# — no internet connection or local web server required.
#
# We deliberately do NOT use Google's <model-viewer> web component here: while
# it would make the viewer markup a single line, it loads as an ES module from
# a CDN, which requires internet and does not work from file:// URLs (the
# double-click case).

#' Create a self-contained offline HTML viewer for a .glb file
#'
#' @param glb_path Path to a binary glTF (.glb) file.
#' @param html Path for the generated HTML file.
#' @param template Path to the viewer template. Defaults to
#'   \code{viewer_template.html} in the same directory as \code{glb_path}
#'   (i.e. examples/web/).
#' @return Invisibly the path to the generated HTML file.
#' @keywords internal
gltf_to_html <- function(glb_path, html, template = NULL) {
    if (is.null(template)) {
        template <- file.path(dirname(glb_path), "viewer_template.html")
    }
    if (!file.exists(glb_path)) {
        stop("glb file not found: ", glb_path)
    }
    if (!file.exists(template)) {
        stop("viewer template not found: ", template)
    }
    web_dir <- dirname(template)

    # --- base64-encode the .glb (no external R package needed) -------------
    glb <- readBin(glb_path, "raw", n = file.info(glb_path)$size)
    b64 <- base64_encode_raw(glb)

    # --- inline the vendored classic-script JS ------------------------------
    # Escape any "</script>" sequences so the JS cannot break out of the
    # <script> element.
    inline_js <- function(fname) {
        p <- file.path(web_dir, fname)
        if (!file.exists(p)) {
            stop("vendored script not found: ", p)
        }
        js <- paste(readLines(p, warn = FALSE), collapse = "\n")
        gsub("</script>", "<\\/script>", js, fixed = TRUE)
    }
    three_js    <- inline_js("three.min.js")
    loader_js   <- inline_js("GLTFLoader.js")
    controls_js <- inline_js("OrbitControls.js")

    # --- substitute into the template (literal, no regex surprises) ---------
    tpl <- paste(readLines(template, warn = FALSE), collapse = "\n")
    out <- gsub("__THREE_MIN_JS__", three_js, tpl, fixed = TRUE)
    out <- gsub("__GLTF_LOADER_JS__", loader_js, out, fixed = TRUE)
    out <- gsub("__ORBIT_CONTROLS_JS__", controls_js, out, fixed = TRUE)
    out <- gsub("__GLB_BASE64__", b64, out, fixed = TRUE)

    writeLines(out, html)
    invisible(html)
}

#' Base64-encode a raw vector (standard RFC 4648, with padding)
#'
#' Vectorized, so it is fast enough even for multi-megabyte .glb files.
#'
#' @param x A raw vector of bytes.
#' @return A character string with the base64 encoding.
#' @keywords internal
base64_encode_raw <- function(x) {
    chars <- strsplit(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
        "")[[1]]
    n <- length(x)
    pad <- (3L - n %% 3L) %% 3L
    if (pad > 0L) {
        x <- c(x, as.raw(rep(0L, pad)))
    }
    n3 <- length(x)
    idx <- seq.int(1L, n3, by = 3L)
    b1 <- as.integer(x[idx])
    b2 <- as.integer(x[idx + 1L])
    b3 <- as.integer(x[idx + 2L])
    v <- b1 * 65536L + b2 * 256L + b3
    c1 <- chars[v %/% 262144L %% 64L + 1L]
    c2 <- chars[v %/% 4096L %% 64L + 1L]
    c3 <- chars[v %/% 64L %% 64L + 1L]
    c4 <- chars[v %% 64L + 1L]
    out <- paste0(c1, c2, c3, c4)
    res <- paste(out, collapse = "")
    if (pad > 0L) {
        substr(res, nchar(res) - pad + 1L, nchar(res)) <-
            paste(rep("=", pad), collapse = "")
    }
    res
}
