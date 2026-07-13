## ----include = FALSE----------------------------------------------------------
knitr::opts_chunk$set(
  collapse = TRUE,
  comment = "#>"
)

## ----eval=FALSE---------------------------------------------------------------
# library(scimesh)
# 
# img <- vis.subject.morph.native(
#     subjects_dir = "~/data/study1",
#     subject_id  = "subject1",
#     measure     = "sulc",
#     views       = "t4",
#     cortex_only = TRUE,
#     draw_colorbar = TRUE,
#     colorbar_title = "Sulcal depth [mm]",
#     aa_samples  = 2L                       # 2×2 supersampling anti-aliasing
# )
# 
# write_png(img, "brain_sulc_t4.png")

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

