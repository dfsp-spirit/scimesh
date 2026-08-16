#' Create a scene descriptor
#'
#' Bundles a list of meshes together with their placement transforms, a
#' camera, and render options into a single scene object that can be passed
#' to \code{\link{render_scene}()} or \code{\link{write_gltf}()}.
#'
#' Each mesh is wrapped into a scene node \code{list(mesh = ..., transform = ...,
#' name = ...)}.  The optional \code{transform} is a 4x4 numeric matrix
#' (column-major, GLM style — the same convention accepted by
#' \code{\link{transform_mesh}()}) that places the mesh in world space at
#' render/export time without modifying the mesh itself.  The optional
#' \code{name} is used by exporters such as glTF for node names.
#'
#' @param meshes A list of mesh descriptors (scimesh or rgl format, see
#'   \code{\link{render_scene}()}), or a list of already-built scene nodes
#'   (\code{list(mesh = ..., transform = ..., name = ...)}).
#' @param camera A camera list from \code{\link{camera}()} or
#'   \code{\link{camera_auto}()}.  Optional here; when \code{NULL} it must be
#'   supplied when calling \code{\link{render_scene}()}.
#' @param options A render options list from \code{\link{render_options}()}.
#'   Optional here; when \code{NULL}, \code{\link{render_scene}()} uses its
#'   default options.
#' @param transforms Optional list of 4x4 numeric matrices, one per mesh,
#'   overriding any embedded \code{transform} in the nodes.  May contain
#'   \code{NULL} entries to mean identity.
#' @param names Optional character vector, one per mesh, overriding any
#'   embedded \code{name}.
#' @return A scene descriptor list with S3 class \code{"scimesh_scene"},
#'   with components \code{meshes} (list of scene nodes), \code{camera},
#'   and \code{options}.
#'
#' @examples
#' cube1 <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(1, 0, 0, 1))
#' cube2 <- generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5), c(0, 0, 1, 1))
#' # place the second cube 2 units along +X
#' tr <- diag(1, 4); tr[1, 4] <- 2
#' sc <- scene(list(cube1, list(mesh = cube2, transform = tr, name = "blue")),
#'             camera = camera_auto(list(cube1, cube2), direction = c(1, 1, 1)))
#' img <- render_scene(sc)
#'
#' @export
scene <- function(meshes, camera = NULL, options = NULL,
                  transforms = NULL, names = NULL) {
    if (!is.list(meshes)) {
        stop("meshes must be a list of mesh descriptors or scene nodes")
    }
    nodes <- lapply(meshes, as_scimesh_scene_node)

    if (!is.null(transforms)) {
        if (length(transforms) != length(nodes)) {
            stop("transforms must have one entry per mesh")
        }
        for (i in seq_along(nodes)) {
            nodes[[i]]$transform <- transforms[[i]]
        }
    }
    if (!is.null(names)) {
        if (length(names) != length(nodes)) {
            stop("names must have one entry per mesh")
        }
        for (i in seq_along(nodes)) {
            nodes[[i]]$name <- names[[i]]
        }
    }

    structure(list(meshes = nodes, camera = camera, options = options),
              class = "scimesh_scene")
}

#' Normalize one scene entry into a scene node
#'
#' Accepts either a bare mesh descriptor (scimesh or rgl format) or an
#' already-built scene node (\code{list(mesh = ..., transform = ...,
#' name = ...)}) and returns a node list.
#'
#' @param x A mesh descriptor list or a scene node list.
#' @return A scene node list with components \code{mesh}, \code{transform}
#'   (may be \code{NULL}), and \code{name} (may be \code{NULL}).
#' @noRd
as_scimesh_scene_node <- function(x) {
    if (!is.list(x)) {
        stop("Expected a mesh descriptor or scene node list, got ",
             class(x)[1])
    }
    if (!is.null(x$mesh)) {
        # Already a node: normalize the mesh inside it
        x$mesh <- as_scimesh_mesh(x$mesh)
        return(x)
    }
    list(mesh = as_scimesh_mesh(x), transform = NULL, name = NULL)
}

#' @export
print.scimesh_scene <- function(x, ...) {
    cat("scimesh scene with ", length(x$meshes), " mesh(es)\n", sep = "")
    for (i in seq_along(x$meshes)) {
        node <- x$meshes[[i]]
        m <- node$mesh
        label <- if (!is.null(node$name) && nzchar(node$name)) {
            node$name
        } else {
            sprintf("#%d", i)
        }
        nv <- if (is.matrix(m$vertices)) nrow(m$vertices) else 0
        nt <- if (is.matrix(m$triangles)) nrow(m$triangles) else 0
        cat(sprintf("  %s: %d vertices, %d triangles", label, nv, nt))
        if (!is.null(node$transform)) {
            cat(" [transform]")
        }
        cat("\n")
    }
    if (!is.null(x$camera)) {
        cat("  camera: set\n")
    }
    if (!is.null(x$options)) {
        cat("  options: set\n")
    }
    invisible(x)
}
