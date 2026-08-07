#!/bin/bash
# scimesh — Headless TIGHT-layout composite workflow demo
#
# Same idea as composite_workflow.sh (render left+right hemispheres from
# several viewpoints, arrange them, attach a colorbar) but produces compact,
# publication-style figures: the whitespace around each rendered brain view
# is cropped away first, which is where most of the wasted space comes from
# (a default 800x600 render can shrink to ~430x290 once cropped).
#
# This mirrors the algorithm used by fsbrain's arrange.brainview.images() /
# vis.export.from.coloredmeshes():
#   1. trim  -> render each view with --crop (crop_to_content: removes the
#               uniform background border), then --grow N to add a small
#               border back so adjacent tiles do not touch each other.
#   2. match -> arrange the cropped tiles with --composite --fit-mode pad.
#               grid_arrange computes the max cell width/height over all
#               tiles and pads every tile to that size (centered), exactly
#               the "find max size over all tiles, extend others to same
#               size" step - no need to measure images yourself.
#   3. colorbar -> attach the colorbar in a second composite pass; --crop
#               trims the whitespace around the colorbar image as well.
#
# No external tools (ImageMagick 'identify' etc.) are required: image
# dimensions are read with the built-in --info mode.
#
# Usage:
#   cd examples/cpp/full_CLI_renderer/build
#   bash ../composite_tight.sh
#
# Requires: scimesh_render built in build/ directory.
# Input meshes: test_data/ply/lh_mesh_sulc_viridis.ply
#               test_data/ply/rh_mesh_sulc_viridis.ply
# Colorbar:     colorbar.png next to this script (copied into build/ by CMake)

set -euo pipefail

MESH_DIR="${1:-../../../../test_data/ply}"
LH="$MESH_DIR/lh_mesh_sulc_viridis.ply"
RH="$MESH_DIR/rh_mesh_sulc_viridis.ply"
RENDERER="./scimesh_render"
BORDER="${BORDER:-5}"               # px border added back after cropping (tile gap)
COLORBAR="${COLORBAR:-colorbar.png}"

echo "=== scimesh tight composite workflow demo ==="
echo "Meshes: $LH, $RH"
echo "Colorbar: $COLORBAR"

# Step 1: Render both hemispheres from three viewpoints.
#   --crop removes the uniform background border (the big whitespace win);
#   --grow adds a small border back so adjacent tiles do not touch.
echo ""
echo "--- Step 1: Rendering views (cropped) ---"
"$RENDERER" --mesh "$LH" --view-dir  1,0,0 --output lat --rotate 90  --bg-color 1,1,1 --crop --grow "$BORDER"
"$RENDERER" --mesh "$RH" --view-dir -1,0,0 --output med --rotate 270 --bg-color 1,1,1 --crop --grow "$BORDER"
"$RENDERER" --mesh "$LH" --mesh "$RH" --view-dir  0,0,1 --output sup --bg-color 1,1,1 --crop --grow "$BORDER"
echo "Tight views rendered:"
"$RENDERER" --info lat.png med.png sup.png

# Step 2: Arrange the three cropped views into a row.
#   --fit-mode pad pads every tile to the max cell size (centered), so no
#   manual "find max size / extend the others" step is needed.
echo ""
echo "--- Step 2: Arranging cropped views (horizontal) ---"
"$RENDERER" --composite lat.png med.png sup.png --cols 3 --fit-mode pad --bg-color 1,1,1 --output brain_grid
echo "Brain grid: $( "$RENDERER" --info brain_grid.png | cut -d' ' -f2 )"

# Step 3: Trim and attach the colorbar to the right of the brain grid.
#   --crop in composite mode trims the whitespace around the colorbar.
if [ -f "$COLORBAR" ]; then
    echo ""
    echo "--- Step 3: Trimming + attaching colorbar ---"
    "$RENDERER" --composite brain_grid.png "$COLORBAR" --cols 2 --fit-mode pad --bg-color 1,1,1 --crop --output final
    echo "Final composite: $( "$RENDERER" --info final.png | cut -d' ' -f2 )"
else
    echo ""
    echo "Note: colorbar '$COLORBAR' not found, skipping step 3."
fi

echo ""
echo "=== Done ==="
ls -la lat.png med.png sup.png brain_grid.png 2>/dev/null
[ -f "$COLORBAR" ] && ls -la "$COLORBAR" final.png
