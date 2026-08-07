#!/bin/bash
# scimesh — Headless composite workflow demo
#
# Renders a brain mesh from three viewpoints, arranges them in a grid,
# and attaches a colorbar.  Demonstrates the --composite mode.
#
# Usage:
#   cd examples/cpp/full_CLI_renderer/build
#   bash ../composite_workflow.sh
#
# Requires: scimesh_render built in build/ directory.
# The brain mesh (test_data/ply/lh_mesh_sulc_viridis.ply) must exist.

set -euo pipefail

MESH="${1:-../../../../test_data/ply/lh_mesh_sulc_viridis.ply}"
RENDERER="./scimesh_render"

echo "=== scimesh composite workflow demo ==="
echo "Mesh: $MESH"

# Step 1: Render three views of the brain
echo ""
echo "--- Step 1: Rendering views ---"
"$RENDERER" --mesh "$MESH" --view-dir  1,0,0 --output lat --bg-color 1,1,1
"$RENDERER" --mesh "$MESH" --view-dir -1,0,0 --output med --bg-color 1,1,1
"$RENDERER" --mesh "$MESH" --view-dir  0,0,1 --output sup --bg-color 1,1,1
echo "Views rendered: lat.png, med.png, sup.png"

# Step 2: Arrange the three brain views in a row
echo ""
echo "--- Step 2: Arranging brain views (horizontal) ---"
"$RENDERER" --composite lat.png med.png sup.png --cols 3 --output brain_grid
echo "Brain grid: brain_grid.png"

# Step 4: Attach colorbar to the brain grid
if [ -f colorbar.png ]; then
    echo ""
    echo "--- Step 4: Attaching colorbar ---"
    "$RENDERER" --composite brain_grid.png colorbar.png --cols 2 --output final --fit-mode pad --bg-color 1,1,1
    echo "Final composite: final.png"
fi

echo ""
echo "=== Done ==="
echo "Output files:"
ls -la lat.png med.png sup.png brain_grid.png 2>/dev/null
[ -f colorbar.png ] && ls -la colorbar.png final.png
