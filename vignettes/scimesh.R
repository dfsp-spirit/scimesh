## ----include = FALSE----------------------------------------------------------
knitr::opts_chunk$set(
  collapse = TRUE,
  comment = "#>"
)

## ----eval=FALSE---------------------------------------------------------------
# library(scimesh)
# 
# sphere <- generate_sphere(c(0, 0, 0), radius = 1.2,
#                           segments = 32, color = c(0.9, 0.3, 0.2, 1.0))
# cam <- camera_auto(sphere, direction = c(1.2, 0.8, 1))
# 
# # Flat-shaded sphere
# img <- render_mesh(sphere$vertices, sphere$triangles,
#     colors = sphere$colors, camera = cam,
#     options = render_options(
#         lights = list(
#             list(position = c(0.5, 1.0, 0.8), intensity = 1.5),
#             list(position = c(-0.5, 0.2, 0.6), intensity = 0.5))))
# 
# write_png(img, "sphere.png")

## ----eval=FALSE---------------------------------------------------------------
# library(scimesh)
# 
# white <- freesurferformats::read.fs.surface("sub-01/surf/lh.white")
# pial  <- freesurferformats::read.fs.surface("sub-01/surf/lh.pial")
# 
# nv <- nrow(white$vertices)
# 
# white_mesh <- list(
#     vertices  = white$vertices,
#     triangles = white$faces,
#     colors    = matrix(c(0.7, 0.7, 0.7, 1.0), nv, 4, byrow = TRUE))
# 
# pial_mesh <- list(
#     vertices  = pial$vertices,
#     triangles = pial$faces,
#     colors    = matrix(c(0.9, 0.3, 0.2, 0.35), nv, 4, byrow = TRUE))
# 
# cam <- camera_auto(pial_mesh, direction = c(-1, 0, 0.2))
# img <- render_scene(list(white_mesh, pial_mesh), cam,
#     render_options(width = 1200, height = 900,
#         backface_culling = FALSE,
#         specular_color = c(0.4, 0.4, 0.4, 1),
#         shininess = 64))
# write_png(img, "transparency_lh.png")

