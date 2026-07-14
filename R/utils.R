#' @keywords internal
#' @export
print.scimesh_image <- function(x, ...) {
    cat("Scimesh RGBA image ", x$width, "x", x$height, "\n", sep = "")
    invisible(x)
}

#' @keywords internal
#' @export
print.scimesh_camera <- function(x, ...) {
    cat(scimesh_print_camera(x), "\n")
    invisible(x)
}

#' @keywords internal
#' @export
print.scimesh_options <- function(x, ...) {
    cat(scimesh_print_options(x), "\n")
    invisible(x)
}

#' @keywords internal
#' @export
print.scimesh_mesh <- function(x, ...) {
    cat(scimesh_print_mesh(x), "\n")
    invisible(x)
}
