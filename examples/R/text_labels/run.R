#!/usr/bin/env Rscript
#
# scimesh Example — Text labels
# -----------------------------
# Shows how to annotate figures with text_layer():
#
#   1. text_labels_brain.png   world-space labels that follow the camera,
#                              plus screen-space panel tags and a caption
#   2. text_labels_atoms.png   element labels next to atoms, with a side-by-side
#                              comparison of depth_test = TRUE/FALSE
#   3. text_labels_screen.png  screen-space labels: title, panel tag, caption,
#                              right-aligned text, a rotated (vertical) label,
#                              and a label placed at the projected position of
#                              a 3D point
#
# Usage:
#   Rscript examples/R/text_labels/run.R
#
# The PNG files are written to the current directory.
#
# Labels are billboards: they always face the camera and keep their pixel size,
# so they stay readable from every viewpoint (unlike text turned into geometry).
# The font (Inter, SIL OFL 1.1) ships with the package; pass font_file = to a
# text_layer(), or set the SCIMESH_FONT environment variable, to replace it.

library(scimesh)

white_opts <- function(width, height) {
    render_options(width = width, height = height,
                   background_color = c(1, 1, 1, 1), aa_samples = 2)
}

# ---------------------------------------------------------------------------
#  Demo 1: world-space labels on a brain-like scene
# ---------------------------------------------------------------------------

demo_brain <- function() {
    message("Demo 1: world-space labels (hemisphere view)")

    left  <- generate_sphere(c(-1.8, 0, 0), radius = 2, segments = 48,
                             color = c(0.56, 0.62, 0.76, 1))
    right <- generate_sphere(c(1.8, 0, 0), radius = 2, segments = 48,
                             color = c(0.63, 0.69, 0.81, 1))

    opts <- white_opts(560, 440)
    cam <- camera_auto(list(left, right), direction = c(0, 0, 1), fov = 45,
                       margin = 1.18)

    # World-space labels: the positions are the 3D locations they annotate, so
    # the labels follow the camera.  `adj = c(0.5, 0)` puts the bottom centre of
    # the text box onto the anchor.
    labels <- text_layer(
        rbind(c(-1.8, 2.3, 0), c(1.8, 2.3, 0), c(4.2, 0, 0), c(-4.2, 0, 0)),
        c("left", "right", "anterior", "posterior"),
        size = 22, adj = c(0.5, 0),
        colors = matrix(c(0.12, 0.12, 0.16, 1,
                          0.12, 0.12, 0.16, 1,
                          0.72, 0.15, 0.15, 1,
                          0.15, 0.25, 0.72, 1), ncol = 4, byrow = TRUE),
        depth_test = FALSE)   # direction annotations must stay readable

    caption <- text_layer(c(14, 12), "world-space labels\n(follow the camera)",
                          space = "screen", adj = c(0, 1), size = 17,
                          colors = c(0.25, 0.25, 0.28, 1), line_spacing = 1.35)

    # Panel tags are plain screen-space labels, so they never move with the scene.
    tag_a <- text_layer(c(12, 8), "A", space = "screen", adj = c(0, 1),
                        size = 26, colors = c(0, 0, 0, 1))
    tag_b <- text_layer(c(548, 8), "B", space = "screen", adj = c(1, 1),
                        size = 26, colors = c(0, 0, 0, 1))

    view_a <- render_scene(scene(list(left, right), camera = cam, options = opts,
                                 texts = list(labels, caption, tag_a)))
    cam_b <- camera_orbit(cam, axis = c(0, 1, 0), angle_degrees = 55)
    view_b <- render_scene(scene(list(left, right), camera = cam_b, options = opts,
                                 texts = list(labels, caption, tag_b)))

    img <- stack_horizontal(view_a, view_b, background = c(1, 1, 1, 1))
    write_png(img, "text_labels_brain.png")
    invisible(NULL)
}

# ---------------------------------------------------------------------------
#  Demo 2: atom labels and occlusion
# ---------------------------------------------------------------------------

demo_atoms <- function() {
    message("Demo 2: element labels next to atoms (with occlusion)")

    # A small molecule; the sulfur atom sits *behind* the central carbon atom as
    # seen from the camera.
    centers <- rbind(c(0, 0, 0), c(1.5, 0.85, 0.3), c(-1.45, 0.7, -0.35),
                     c(0.25, -1.5, 0.25), c(0, 0, -1.2))
    atoms <- generate_multi_spheres(
        centers, radii = c(0.70, 0.55, 0.50, 0.70, 0.45),
        colors = matrix(c(0.36, 0.36, 0.40, 1,
                          0.25, 0.35, 0.85, 1,
                          0.85, 0.30, 0.25, 1,
                          0.36, 0.36, 0.40, 1,
                          0.85, 0.72, 0.20, 1), ncol = 4, byrow = TRUE),
        segments = 32L)
    bonds <- generate_multi_cylinders(
        from = matrix(rep(c(0, 0, 0), 4), ncol = 3, byrow = TRUE),
        to = centers[2:5, , drop = FALSE], radii = 0.10,
        colors = c(0.62, 0.62, 0.65, 1), caps = FALSE)

    opts <- white_opts(560, 440)
    cam <- camera_auto(list(atoms), direction = c(0.15, 0.10, 1), margin = 1.15)

    # Element symbols: anchored on the atom centres, nudged up and to the right
    # by a pixel offset, with a white halo so they stay readable on the atoms.
    element_labels <- function(depth_test) {
        text_layer(centers, c("C", "N", "O", "C", "S"), size = 20,
                   adj = c(0, 0.5), offset = c(9, -9),
                   colors = c(0.15, 0.15, 0.18, 1),
                   halo_color = c(1, 1, 1, 0.85), halo_width = 1.5,
                   depth_test = depth_test)
    }

    # Panel A: the default.  The sulfur label is hidden, because its anchor is
    # behind the carbon atom.  Panel B: depth_test = FALSE shows it anyway.
    cap_a <- text_layer(c(14, 12), "depth_test = TRUE\nthe S label is hidden",
                        space = "screen", adj = c(0, 1), size = 16,
                        colors = c(0.10, 0.35, 0.10, 1),
                        halo_color = c(1, 1, 1, 0.9), halo_width = 2)
    cap_b <- text_layer(c(14, 12), "depth_test = FALSE\nthe S label shines through",
                        space = "screen", adj = c(0, 1), size = 16,
                        colors = c(0.60, 0.15, 0.15, 1),
                        halo_color = c(1, 1, 1, 0.9), halo_width = 2)

    panel_a <- render_scene(scene(list(atoms, bonds), camera = cam,
                                  options = opts,
                                  texts = list(element_labels(TRUE), cap_a)))
    panel_b <- render_scene(scene(list(atoms, bonds), camera = cam,
                                  options = opts,
                                  texts = list(element_labels(FALSE), cap_b)))

    img <- stack_horizontal(panel_a, panel_b, background = c(1, 1, 1, 1))
    write_png(img, "text_labels_atoms.png")
    invisible(NULL)
}

# ---------------------------------------------------------------------------
#  Demo 3: screen-space labels (figure layout)
# ---------------------------------------------------------------------------

demo_screen <- function() {
    message("Demo 3: screen-space labels (titles, tags, captions)")

    opts <- render_options(width = 640, height = 440, aa_samples = 2,
                           background_color = c(0.13, 0.14, 0.17, 1))
    object <- generate_sphere(c(0, 0, 0), radius = 1.6, segments = 64,
                              color = c(0.78, 0.48, 0.26, 1))
    cam <- camera_auto(list(object), direction = c(0, 0, 1), margin = 1.15)

    # Screen space is measured in output pixels from the top left corner, so
    # these labels do not care about the camera at all.
    dark_halo <- c(0, 0, 0, 0.9)
    title <- text_layer(c(22, 20), "screen-space labels", space = "screen",
                        adj = c(0, 1), size = 26, colors = c(0.95, 0.95, 0.96, 1),
                        halo_color = dark_halo, halo_width = 1.5)
    tag <- text_layer(c(618, 16), "A", space = "screen", adj = c(1, 1), size = 30,
                      colors = c(0.95, 0.95, 0.96, 1), halo_color = dark_halo,
                      halo_width = 1.5)

    # text_extent() measures a label, e.g. to right-align a caption or to place
    # it so that it cannot collide with another one.
    caption_text <- paste("screen space is measured in output pixels",
                          "from the top-left corner.", sep = "\n")
    ext <- text_extent(caption_text, size = 15)
    message(sprintf("  caption is %.0f x %.0f px", ext$width, ext$height))
    caption <- text_layer(c(618, 422), caption_text, space = "screen",
                          adj = c(1, 0), size = 15,
                          colors = c(0.82, 0.83, 0.86, 1), line_spacing = 1.35)

    # world_to_screen() projects a 3D location into the image, which is how you
    # put a screen-space label (or a callout) next to something in the scene.
    anchor <- world_to_screen(matrix(c(0, 1.6, 0), ncol = 3), cam, 640, 440, opts)
    projected <- text_layer(c(anchor$x, anchor$y), "projected anchor",
                            space = "screen", adj = c(0.5, 0), size = 15,
                            offset = c(0, -4), colors = c(0.95, 0.95, 0.96, 1),
                            halo_color = dark_halo, halo_width = 1.5)

    # Rotation turns a label about its anchor: 90 degrees reads bottom to top,
    # which is the usual orientation of a y-axis label.
    vertical <- text_layer(c(26, 258), "rotated 90 degrees (reads bottom to top)",
                           space = "screen", adj = c(0, 0.5), size = 15,
                           colors = c(0.82, 0.83, 0.86, 1), rotation = 90)

    img <- render_scene(scene(list(object), camera = cam, options = opts,
                               texts = list(title, tag, caption, projected,
                                            vertical)))
    write_png(img, "text_labels_screen.png")

    # Screen-space labels need no mesh at all: render_text() draws them onto the
    # background described by the render options.
    only_text <- render_text(c(20, 20), "labels without meshes", space = "screen",
                             adj = c(0, 1), size = 24,
                             colors = c(0, 0, 0, 1),
                             options = render_options(width = 300, height = 70,
                                                      background_color = c(1, 1, 1, 1)))
    write_png(only_text, "text_labels_only.png")
    invisible(NULL)
}

main <- function() {
    font <- font_info()
    message(sprintf("scimesh text label example (font: %s, %s)",
                    font$family, font$path))

    errors <- 0
    for (demo in list(demo_brain, demo_atoms, demo_screen)) {
        tryCatch(demo(), error = function(e) {
            message("  ERROR: ", conditionMessage(e))
            errors <<- errors + 1
        })
    }

    outputs <- c("text_labels_brain.png", "text_labels_atoms.png",
                 "text_labels_screen.png", "text_labels_only.png")
    for (f in outputs) {
        if (file.exists(f)) {
            message(sprintf("  wrote %s (%s bytes)", f, file.size(f)))
        } else {
            message(sprintf("  MISSING %s", f))
            errors <- errors + 1
        }
    }

    if (errors > 0) {
        message(sprintf("%d error(s)", errors))
        quit(status = 1)
    }
    message("All demos finished.")
    invisible(NULL)
}

if (!interactive()) {
    main()
}
