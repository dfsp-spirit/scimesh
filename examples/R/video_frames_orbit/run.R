#!/usr/bin/env Rscript
#
# scimesh Example — Turntable video from orbit frames
# ---------------------------------------------------
# Demonstrates camera_orbit() by rendering 12 frames of a camera
# circling around a mesh.  For more complex paths, replace
# camera_orbit() with your own function — it only needs to set
# camera$eye and camera$up.
#
# Usage:
#   Rscript examples/R/video_frames_orbit/run.R
#
# Output: frame_0000.png ... frame_0011.png in the current directory.
#   ./frames_to_video.sh   assembles them into orbit.mp4  (requires ffmpeg)
#
# Dependencies: scimesh

library(scimesh)

mesh <- generate_torus(c(0, 0, 0), major_radius = 1.2, minor_radius = 0.4,
                       major_segments = 48, minor_segments = 24,
                       color = c(0.7, 0.5, 0.3, 1.0))

cam <- camera_auto(mesh, direction = c(1, 1, 1), fov = 45)

n_frames <- 12L
cat(sprintf("Rendering %d frames at 600x400...\n", n_frames))

for (i in seq_len(n_frames)) {
    cam_i <- camera_orbit(cam, angle_degrees = 360 / n_frames * (i - 1))
    img <- render_mesh(mesh$vertices, mesh$triangles,
                       colors = mesh$colors, camera = cam_i,
                       options = render_options(width = 600L, height = 400L,
                                                shading = "smooth"))
    fname <- sprintf("frame_%04d.png", i - 1L)
    write_png(img, fname)
    cat(sprintf("  [%d/%d] %s\n", i, n_frames, fname))
}

cat("Done. Assemble with:  ./frames_to_video.sh\n")
