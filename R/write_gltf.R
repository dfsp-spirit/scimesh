#' Write a scene or mesh list to a glTF file
#'
#' Exports a scene (or a list of meshes) to the glTF 2.0 format, either as a
#' JSON document with an external binary buffer (\code{.gltf} + \code{.bin})
#' or as a single self-contained binary file (\code{.glb}).  The resulting
#' file can be viewed in any glTF-capable viewer (e.g. a browser using
#' three.js) and includes per-mesh placement transforms, vertex colors, and
#' optionally a camera.
#'
#' Renderer-specific settings (shading mode, fog, SSAO, ...) are not part of
#' the glTF standard and are not exported.  Per-face colors are exported by
#' splitting vertices (each triangle gets its own vertices), which increases
#' geometry roughly 3x.
#'
#' @param meshes Either a \code{scimesh_scene} object (see \code{\link{scene}()}),
#'   a list of mesh descriptors, or a list of scene nodes — the same inputs
#'   accepted by \code{\link{render_scene}()}.
#' @param path Output file path.  For \code{format = "gltf"} the binary buffer
#'   is written next to it as \code{<stem>.bin}.
#' @param camera Optional camera list from \code{\link{camera}()} or
#'   \code{\link{camera_auto}()}.  When provided, a glTF perspective camera
#'   node is included.
#' @param format Output format: \code{"gltf"} (default, JSON + \code{.bin}) or
#'   \code{"glb"} (single binary file).
#' @return Invisibly \code{NULL}.
#'
#' @examples
#' cube <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
#' tr <- diag(1, 4); tr[1, 4] <- 2
#' sc <- scene(list(cube, list(mesh = cube, transform = tr, name = "second")))
#' out <- tempfile(fileext = ".glb")
#' write_gltf(sc, out, format = "glb")
#'
#' @export
write_gltf <- function(meshes, path, camera = NULL,
                       format = c("gltf", "glb")) {
    if (inherits(meshes, "scimesh_scene")) {
        sc <- meshes
        meshes <- sc$meshes
        if (is.null(camera)) {
            camera <- sc$camera
        }
    }
    if (!is.list(meshes)) {
        stop("meshes must be a list of mesh descriptors, scene nodes, ",
             "or a scimesh_scene")
    }
    format <- match.arg(format)
    scene_data <- lapply(meshes, as_scimesh_scene_node)
    scimesh_write_gltf(scene_data, path, camera, format)
    invisible(NULL)
}
