#!/usr/bin/env bash
#
# run_all_cpp.sh — Build and run all scimesh C++ example apps
#
# Usage:
#   ./examples/cpp/run_all_cpp.sh                # run all
#   ./examples/cpp/run_all_cpp.sh spot_cow       # run only spot_cow
#
# Prints a summary at the end showing how many examples passed / failed.

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FILTER="${1:-}"

PASSED=()
FAILED=()

run_one() {
    local dir="$1"
    local exe="$2"
    shift 2
    local args=("$@")

    echo ""
    echo "============================================"
    echo "  Building & running: $dir"
    echo "============================================"

    cd "$SCRIPT_DIR/$dir"
    mkdir -p build
    cd build

    echo "[cmake] $dir"
    if ! cmake ..; then
        echo "  FAIL: cmake configuration failed"
        FAILED+=("$dir (cmake)")
        return 1
    fi

    echo "[make] $dir"
    if ! make; then
        echo "  FAIL: build failed"
        FAILED+=("$dir (make)")
        return 1
    fi

    echo "[run] $exe ${args[*]}"
    if "./$exe" "${args[@]}"; then
        echo "  PASS"
        PASSED+=("$dir")
    else
        echo "  FAIL: exited with non-zero status"
        FAILED+=("$dir (runtime)")
    fi
}

if [[ -z "$FILTER" || "$FILTER" == "all_primitives" ]]; then
    run_one all_primitives                    all_primitives
fi
if [[ -z "$FILTER" || "$FILTER" == "spot_cow" ]]; then
    run_one spot_cow                          spot_cow
fi
if [[ -z "$FILTER" || "$FILTER" == "transparency" ]]; then
    run_one transparency                      transparency_demo
fi
if [[ -z "$FILTER" || "$FILTER" == "protein_data_bank_pdb_file" ]]; then
    run_one protein_data_bank_pdb_file        protein_demo
fi
if [[ -z "$FILTER" || "$FILTER" == "whole_brain_annot" ]]; then
    run_one whole_brain_annot    whole_brain_annot
fi
if [[ -z "$FILTER" || "$FILTER" == "whole_brain_sulc" ]]; then
    run_one whole_brain_sulc     whole_brain_sulc
fi
if [[ -z "$FILTER" || "$FILTER" == "whole_brain_sulc_fsaverage" ]]; then
    run_one whole_brain_sulc_fsaverage whole_brain_sulc_fsaverage
fi
if [[ -z "$FILTER" || "$FILTER" == "bunny" ]]; then
    run_one bunny                             bunny
fi
if [[ -z "$FILTER" || "$FILTER" == "dragon" ]]; then
    run_one dragon                            dragon
fi
if [[ -z "$FILTER" || "$FILTER" == "brain_video" ]]; then
    run_one brain_video                       brain_video
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
    echo "  Available: all_primitives spot_cow transparency protein_data_bank_pdb_file whole_brain_annot whole_brain_sulc whole_brain_sulc_fsaverage bunny dragon brain_video"
    exit 2
fi

if [[ ${#FAILED[@]} -gt 0 ]]; then
    exit 1
fi
exit 0
