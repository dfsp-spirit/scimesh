#!/usr/bin/env bash
#
# run_all_R.sh — Run all scimesh R example scripts
#
# Usage:
#   ./examples/R/run_all_R.sh                    # run all
#   ./examples/R/run_all_R.sh spot_cow           # run only spot_cow
#
# Prints a summary at the end showing how many examples passed / failed.

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$REPO_ROOT"

FILTER="${1:-}"

PASSED=()
FAILED=()

run_one() {
    local script="$1"
    local label="$2"

    echo ""
    echo "============================================"
    echo "  Running: $label"
    echo "============================================"

    if Rscript "$script"; then
        echo "  PASS"
        PASSED+=("$label")
    else
        echo "  FAIL: exited with non-zero status"
        FAILED+=("$label")
    fi
}

if [[ -z "$FILTER" || "$FILTER" == "transparency" ]]; then
    run_one examples/R/transparency/run.R                transparency
fi
if [[ -z "$FILTER" || "$FILTER" == "primitives" ]]; then
    run_one examples/R/primitives/run.R                primitives
fi
if [[ -z "$FILTER" || "$FILTER" == "spot_cow" ]]; then
    run_one examples/R/spot_cow/run.R                    spot_cow
fi
if [[ -z "$FILTER" || "$FILTER" == "dragon" ]]; then
    run_one examples/R/dragon/run.R                      dragon
fi
if [[ -z "$FILTER" || "$FILTER" == "whole_brain_sulc" ]]; then
    run_one examples/R/whole_brain_sulc/run.R whole_brain_sulc
fi
if [[ -z "$FILTER" || "$FILTER" == "video_frames_orbit" ]]; then
    run_one examples/R/video_frames_orbit/run.R            video_frames_orbit
fi

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
    echo "  Available: transparency primitives spot_cow dragon whole_brain_sulc video_frames_orbit"
    exit 2
fi

if [[ ${#FAILED[@]} -gt 0 ]]; then
    exit 1
fi
exit 0
