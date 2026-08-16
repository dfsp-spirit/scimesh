#!/usr/bin/env Rscript
# Demo: export both brain hemisphere meshes to glTF 2.0 and to a fully
# offline HTML viewer.
#
# Loads the two example hemisphere meshes from test_data/ply (sulcal-depth
# viridis coloring), places them in a scene (with a camera), and
#   1. writes examples/web/brain.glb            — single binary glTF file
#   2. writes examples/web/brain.gltf + brain.bin — JSON glTF + external buffer
#   3. writes examples/web/brain_viewer.html    — self-contained, offline,
#      double-clickable HTML viewer (three.js + GLTFLoader + OrbitControls are
#      inlined and the .glb is embedded as base64; no internet or server).
#
# The C++ example examples/cpp/gltf_export does the same export in C++.
#
# Usage (from the scimesh repo root):
#   Rscript examples/R/export_gltf.R

library(scimesh)

# Locate the repo root: this script lives in examples/R/.
get_repo_root <- function() {
    args <- commandArgs(trailingOnly = FALSE)
    file_arg <- args[grep("^--file=", args)]
    if (length(file_arg)) {
        p <- normalizePath(sub("^--file=", "", file_arg))
        return(dirname(dirname(dirname(p))))
    }
    normalizePath("../..")  # fallback: assume run from the repo root
}

repo_root <- get_repo_root()
data_dir  <- file.path(repo_root, "test_data/ply")
out_dir   <- file.path(repo_root, "examples/web")

# --- 1. Load the two hemisphere meshes ---------------------------------------
lh_ply <- file.path(data_dir, "lh_mesh_sulc_viridis.ply")
rh_ply <- file.path(data_dir, "rh_mesh_sulc_viridis.ply")
for (f in c(lh_ply, rh_ply)) {
    if (!file.exists(f)) {
        stop("PLY file not found: ", f, " (run from the scimesh repo root)")
    }
}
lh <- read_ply(lh_ply)
rh <- read_ply(rh_ply)
cat(sprintf("Loaded %d vertices (lh), %d vertices (rh)\n",
            nrow(lh$vertices), nrow(rh$vertices)))

# --- 2. Put them into a scene ------------------------------------------------
cam <- camera_auto(list(lh, rh), direction = c(1, 1, 1))
sc  <- scene(list(lh, rh), camera = cam)

# --- 3. Export to glTF -------------------------------------------------------
write_gltf(sc, file.path(out_dir, "brain.glb"), format = "glb")
cat("Wrote ", file.path(out_dir, "brain.glb"), "\n", sep = "")

write_gltf(sc, file.path(out_dir, "brain.gltf"), camera = cam)
cat("Wrote ", file.path(out_dir, "brain.gltf"), " + brain.bin\n", sep = "")

# --- 4. Generate the self-contained offline HTML viewer ----------------------
source(file.path(repo_root, "examples/R/gltf_to_html.R"))
gltf_to_html(file.path(out_dir, "brain.glb"),
             html = file.path(out_dir, "brain_viewer.html"),
             template = file.path(out_dir, "viewer_template.html"))
cat("Wrote ", file.path(out_dir, "brain_viewer.html"),
    " (double-click to view)\n", sep = "")
