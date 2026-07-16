#!/usr/bin/env bash
#
# run_all_R.sh — Run all scimesh R example scripts
#
# Usage:
#   ./examples/R/run_all_R.sh                # run all
#   ./examples/R/run_all_R.sh spot_cow       # run only spot_cow
#
# Prints a summary at the end showing how many examples passed / failed.

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

export SCIMESH_TEST_DATA_DIR="$REPO_ROOT/test_data"

FILTER="${1:-}"

declare -a EXAMPLES=(
    transparency
    primitives
    spot_cow
    dragon
    whole_brain_sulc
    video_frames_orbit
    colormaps
)

PASSED=()
FAILED=()

run_one() {
    local dir="$1"

    echo ""
    echo "============================================"
    echo "  Running: $dir"
    echo "============================================"

    cd "$SCRIPT_DIR/$dir"

    if Rscript "$SCRIPT_DIR/$dir/run.R"; then
        echo "  PASS"
        PASSED+=("$dir")
    else
        echo "  FAIL: exited with non-zero status"
        FAILED+=("$dir")
    fi
}

for dir in "${EXAMPLES[@]}"; do
    [[ -z "$FILTER" || "$FILTER" == "$dir" ]] || continue
    run_one "$dir"
done

echo ""
echo "============================================"
echo "  SUMMARY"
echo "============================================"
echo "  Passed: ${#PASSED[@]}"
for p in "${PASSED[@]}"; do
    echo "    OK  $p"
done
echo "  Failed: ${#FAILED[@]}"
for f in "${FAILED[@]}"; do
    echo "    FAIL  $f"
done

if [[ -n "$FILTER" && ${#PASSED[@]} -eq 0 && ${#FAILED[@]} -eq 0 ]]; then
    echo "  WARNING: no example matched filter '$FILTER'"
    echo "  Available: ${EXAMPLES[*]}"
    exit 2
fi

if [[ ${#FAILED[@]} -gt 0 ]]; then
    exit 1
fi
exit 0
