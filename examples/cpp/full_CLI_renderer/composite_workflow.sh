#!/bin/bash
# scimesh — Headless composite workflow demo
#
# Renders left + right brain hemispheres from three viewpoints,
# arranges them in a grid, and attaches a colorbar.
# Demonstrates --composite mode and multi-mesh rendering (--mesh repeatable).
#
# Usage:
#   cd examples/cpp/full_CLI_renderer/build
#   bash ../composite_workflow.sh
#
# Requires: scimesh_render built in build/ directory.
# Input meshes: test_data/ply/lh_mesh_sulc_viridis.ply
#               test_data/ply/rh_mesh_sulc_viridis.ply

set -euo pipefail

MESH_DIR="${1:-../../../../test_data/ply}"
LH="$MESH_DIR/lh_mesh_sulc_viridis.ply"
RH="$MESH_DIR/rh_mesh_sulc_viridis.ply"
RENDERER="./scimesh_render"

echo "=== scimesh composite workflow demo ==="
echo "Meshes: $LH, $RH"

# Step 1: Render both hemispheres from three viewpoints
echo ""
echo "--- Step 1: Rendering views ---"
"$RENDERER" --mesh "$LH" --view-dir  1,0,0 --output lat --rotate 90 --bg-color 1,1,1
"$RENDERER" --mesh "$RH" --view-dir -1,0,0 --output med --rotate 270 --bg-color 1,1,1
"$RENDERER" --mesh "$LH" --mesh "$RH" --view-dir  0,0,1 --output sup --bg-color 1,1,1
echo "Views rendered: lat.png, med.png, sup.png"

# Step 2: Arrange the three brain views in a row
echo ""
echo "--- Step 2: Arranging brain views (horizontal) ---"
"$RENDERER" --composite lat.png med.png sup.png --cols 3 --output brain_grid
echo "Brain grid: brain_grid.png"

# Step 3: Attach colorbar to the brain grid
if [ -f colorbar.png ]; then
    echo ""
    echo "--- Step 3: Attaching colorbar ---"
    "$RENDERER" --composite brain_grid.png colorbar.png --cols 2 --output final --fit-mode pad --bg-color 1,1,1
    echo "Final composite: final.png"
fi

echo ""
echo "=== Done ==="
echo "Output files:"
ls -la lat.png med.png sup.png brain_grid.png 2>/dev/null
[ -f colorbar.png ] && ls -la colorbar.png final.png
