#!/usr/bin/env Rscript
#
# Compare primitive generation in rgl and scimesh.
#
# This requires rgl to be installed.
#

library(scimesh)
library(rgl)

# Create a cube in rgl and convert to scimesh format.
# rgl::cube3d() returns a quad mesh (ib), so we triangulate with as.tmesh3d().
rgl_cube_quad  <- cube3d()
rgl_cube_tmesh <- as.tmesh3d(rgl_cube_quad)
rgl_cube       <- mesh_from_rgl(rgl_cube_tmesh)

# Create a cube mesh in scimesh.
# generate_cuboid takes a center and half-extents.  The default rgl cube is
# axis-aligned, centered at the origin, with side length 2.
scimesh_cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1))

# ---- Compare properties ---------------------------------------------------
cat("===== Vertex count =====\n")
cat(sprintf("rgl:      %d vertices\n", nrow(rgl_cube$vertices)))
cat(sprintf("scimesh:  %d vertices\n", nrow(scimesh_cube$vertices)))

cat("\n===== Triangle count =====\n")
cat(sprintf("rgl:      %d triangles\n", nrow(rgl_cube$triangles)))
cat(sprintf("scimesh:  %d triangles\n", nrow(scimesh_cube$triangles)))

cat("\n===== Bounding box =====\n")
rgl_bbox <- apply(rgl_cube$vertices, 2, range)
scimesh_bbox <- apply(scimesh_cube$vertices, 2, range)
cat(sprintf("rgl:      x[%.2f, %.2f]  y[%.2f, %.2f]  z[%.2f, %.2f]\n",
            rgl_bbox[1, 1], rgl_bbox[2, 1],
            rgl_bbox[1, 2], rgl_bbox[2, 2],
            rgl_bbox[1, 3], rgl_bbox[2, 3]))
cat(sprintf("scimesh:  x[%.2f, %.2f]  y[%.2f, %.2f]  z[%.2f, %.2f]\n",
            scimesh_bbox[1, 1], scimesh_bbox[2, 1],
            scimesh_bbox[1, 2], scimesh_bbox[2, 2],
            scimesh_bbox[1, 3], scimesh_bbox[2, 3]))

cat("\n===== Colors =====\n")
cat(sprintf("rgl:      %s\n",
            if (!is.null(rgl_cube$colors)) sprintf("%d x %d matrix", nrow(rgl_cube$colors), ncol(rgl_cube$colors)) else "none"))
cat(sprintf("scimesh:  %s\n",
            if (!is.null(scimesh_cube$colors)) sprintf("%d x %d matrix", nrow(scimesh_cube$colors), ncol(scimesh_cube$colors)) else "none"))

cat("\n===== Normals =====\n")
cat(sprintf("rgl:      %s\n",
            if (!is.null(rgl_cube$normals)) sprintf("%d x %d matrix", nrow(rgl_cube$normals), ncol(rgl_cube$normals)) else "none"))
cat(sprintf("scimesh:  %s\n",
            if (!is.null(scimesh_cube$normals)) sprintf("%d x %d matrix", nrow(scimesh_cube$normals), ncol(scimesh_cube$normals)) else "none"))

cat("\n===== Texture coordinates (UV) =====\n")
cat(sprintf("rgl:      %s\n",
            if (!is.null(rgl_cube$texcoords)) "present" else "none"))
cat(sprintf("scimesh:  %s\n",
            if (!is.null(scimesh_cube$texcoords)) "present" else "none"))

cat("\n===== Vertex positions (first 3) =====\n")
cat("rgl:\n")
print(head(rgl_cube$vertices, 3))
cat("scimesh:\n")
print(head(scimesh_cube$vertices, 3))

# Note: the order of the vertices may differ between the two libraries,
# so a direct comparison of the vertex arrays is not meaningful.  Instead
# we compare aggregate properties like bounding box, counts, and the
# presence/absence of normals, colors, and texture coordinates.

cat("\nDone.\n")
