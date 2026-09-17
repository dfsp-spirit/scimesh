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
#' @param lines \code{NULL}, a line layer (see \code{\link{line_layer}}), or a
#'   list of them.  Line layers are drawn after the meshes with a width
#'   measured in pixels and without creating any geometry, which makes them the
#'   cheap way to draw many thin lines (wireframes, graph or connectome edges).
#'   They share the depth buffer with the meshes and contribute to the scene
#'   bounding box (and thus to the camera framing), unless the layer sets
#'   \code{affects_bounds = FALSE}, see \code{\link{line_layer}}.
#' @param texts \code{NULL}, a text layer (see \code{\link{text_layer}}), or a
#'   list of them.  Text layers are drawn after the meshes and the lines as
#'   billboards and create no geometry either, so they are the way to annotate a
#'   figure (region names, atom labels, panel tags).  Unlike line layers they are
#'   ignored by the scene bounding box, since their extent depends on the font
#'   and the output size.
#' @return A scene descriptor list with S3 class \code{"scimesh_scene"},
#'   with components \code{meshes} (list of scene nodes), \code{lines} (list
#'   of line layers, possibly empty), \code{texts} (list of text layers,
#'   possibly empty), \code{camera}, and \code{options}.
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
#' # add a line layer drawn on top of the meshes:
#' sc2 <- scene(list(cube1),
#'              lines = line_layer(matrix(c(0, 0, 1), ncol = 3),
#'                                 matrix(c(1, 1, 1), ncol = 3), width = 3))
#'
#' # add a label anchored above the cube:
#' sc3 <- scene(list(cube1),
#'              texts = text_layer(matrix(c(0, 1, 0), ncol = 3), "cube",
#'                                 size = 18, adj = c(0.5, 0)))
#'
#' @seealso \code{\link{line_layer}}, \code{\link{text_layer}}
#' @export
scene <- function(meshes, camera = NULL, options = NULL,
                  transforms = NULL, names = NULL, lines = NULL,
                  texts = NULL) {
    if (!is.list(meshes)) {
        stop("meshes must be a list of mesh descriptors or scene nodes")
    }
    lines <- normalize_line_layers(lines)
    texts <- normalize_text_layers(texts)
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

    structure(list(meshes = nodes, lines = lines, texts = texts,
                   camera = camera, options = options),
              class = "scimesh_scene")
}

#' Normalize the \code{lines} argument of scene()
#'
#' Accepts \code{NULL}, a single line layer, or a list of line layers and
#' returns a (possibly empty) list of line layers.  Layers may also be wrapped
#' into a scene node (\code{list(lines = <layer>, transform = ...)}).
#'
#' @param lines \code{NULL}, a line layer, or a list of line layers / nodes.
#' @return A list of line layers (or line layer nodes).
#' @keywords internal
normalize_line_layers <- function(lines) {
    if (is.null(lines)) {
        return(list())
    }
    if (inherits(lines, "scimesh_lines")) {
        return(list(lines))
    }
    if (!is.list(lines)) {
        stop("lines must be NULL, a line layer (see lines()), or a list of them")
    }
    for (i in seq_along(lines)) {
        entry <- lines[[i]]
        layer <- if (inherits(entry, "scimesh_lines")) {
            entry
        } else if (is.list(entry)) {
            entry$lines
        } else {
            NULL
        }
        if (!inherits(layer, "scimesh_lines")) {
            stop(sprintf("lines[[%d]] is not a line layer, see line_layer()", i))
        }
    }
    return(lines)
}

#' Normalize the \code{texts} argument of scene()
#'
#' Accepts \code{NULL}, a single text layer, or a list of text layers and
#' returns a (possibly empty) list of text layers.  Layers may also be wrapped
#' into a scene node (\code{list(text = <layer>, transform = ...)}), which
#' allows moving a group of world-space labels with one transform.
#'
#' @param texts \code{NULL}, a text layer, or a list of text layers / nodes.
#' @return A list of text layers (or text layer nodes).
#' @keywords internal
normalize_text_layers <- function(texts) {
    if (is.null(texts)) {
        return(list())
    }
    if (inherits(texts, "scimesh_text")) {
        return(list(texts))
    }
    if (!is.list(texts)) {
        stop("texts must be NULL, a text layer (see text_layer()), or a list of them")
    }
    for (i in seq_along(texts)) {
        entry <- texts[[i]]
        layer <- if (inherits(entry, "scimesh_text")) {
            entry
        } else if (is.list(entry)) {
            entry$text
        } else {
            NULL
        }
        if (!inherits(layer, "scimesh_text")) {
            stop(sprintf("texts[[%d]] is not a text layer, see text_layer()", i))
        }
    }
    return(texts)
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
    cat("scimesh scene with ", length(x$meshes), " mesh(es)",
        if (length(x$lines) > 0L) sprintf(" and %d line layer(s)",
                                          length(x$lines)) else "",
        if (length(x$texts) > 0L) sprintf(" and %d text layer(s)",
                                          length(x$texts)) else "",
        "\n", sep = "")
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


#' Change whether line layers of a scene contribute to its bounds
#'
#' Sets the \code{affects_bounds} flag of one or more line layers of a scene, see
#' \code{\link{line_layer}}.  A layer with the flag set contributes to the
#' bounding box of the scene, and thus to the extent that a camera fitted to the
#' scene (\code{\link{camera_auto}}) has to cover; a layer without it is ignored
#' when the bounds are computed, which is what you want for decorational lines.
#'
#' The layers can be selected by position (\code{index}) or by name
#' (\code{name}, for layers that were added as scene nodes with a
#' \code{name}).  Exactly one of the two has to be given.  A scene that contains
#' no mesh at all is framed by its line layers even when they all opted out,
#' since there would otherwise be no geometry to derive a camera from.
#'
#' @param scene A scene descriptor list, see \code{\link{scene}()}.
#' @param index Integer vector, the positions (1-based) of the layers to update,
#'   or \code{NULL} to select them by \code{name}.
#' @param name Character vector, the names of the layers to update, or
#'   \code{NULL} to select them by \code{index}.  Bare line layers (which are not
#'   wrapped into a scene node) have no name.
#' @param affects_bounds Whether the selected layers contribute to the scene
#'   bounds (default \code{TRUE}), see \code{\link{line_layer}}.
#'
#' @return The scene with the updated layers, invisibly.  Since a scene is a
#'   plain list, the update has to be assigned to take effect, e.g.
#'   \code{sc <- scene_set_line_affects_bounds(sc, name = "leader",
#'   affects_bounds = FALSE)}.
#'
#' @seealso \code{\link{line_layer}}, \code{\link{camera_auto}}
#' @examples
#' from <- matrix(c(-1, 0, 0, 5, 0, 0), ncol = 3, byrow = TRUE)
#' to   <- matrix(c(1, 0, 0, 6, 0, 0), ncol = 3, byrow = TRUE)
#' # The second segment is a decorational leader line pointing away from the data.
#' sc <- scene(list(generate_cuboid(c(0, 0, 0), c(0.5, 0.5, 0.5))),
#'             lines = list(list(lines = line_layer(from, to), name = "edges")))
#' sc <- scene_set_line_affects_bounds(sc, index = 1, affects_bounds = TRUE)
#' @export
scene_set_line_affects_bounds <- function(scene, index = NULL, name = NULL,
                                          affects_bounds = TRUE) {
    if (!inherits(scene, "scimesh_scene")) {
        stop("scene must be a scene descriptor, see scene()")
    }
    if (is.null(index) == is.null(name)) {
        stop("exactly one of index and name must be given")
    }
    if (!is.logical(affects_bounds) || length(affects_bounds) != 1L ||
        is.na(affects_bounds)) {
        stop("affects_bounds must be a single TRUE or FALSE")
    }
    if (length(scene$lines) == 0L) {
        stop("the scene contains no line layer")
    }

    if (!is.null(index)) {
        if (!is.numeric(index) || length(index) == 0L || anyNA(index) ||
            any(index < 1) || any(index > length(scene$lines))) {
            stop(sprintf("index must be in 1..%d", length(scene$lines)))
        }
        selected <- as.integer(index)
    } else {
        if (!is.character(name) || length(name) == 0L || anyNA(name)) {
            stop("name must be a character vector")
        }
        layer_names <- vapply(scene$lines, function(entry) {
            if (is.list(entry) && !inherits(entry, "scimesh_lines") &&
                !is.null(entry$name)) {
                return(as.character(entry$name))
            }
            return("")
        }, character(1L))
        selected <- which(layer_names %in% name)
        if (length(selected) == 0L) {
            stop(sprintf("no line layer with name in %s; the named layers are: %s",
                         paste(sprintf("'%s'", name), collapse = ", "),
                         if (any(nzchar(layer_names)))
                             paste(sprintf("'%s'", layer_names[nzchar(layer_names)]), collapse = ", ")
                         else "none"))
        }
    }

    for (i in selected) {
        entry <- scene$lines[[i]]
        if (inherits(entry, "scimesh_lines")) {
            entry$affects_bounds <- affects_bounds
        } else {
            entry$lines$affects_bounds <- affects_bounds
        }
        scene$lines[[i]] <- entry
    }

    return(invisible(scene))
}
