#!/usr/bin/env bash
#
# run_all_cpp.sh — Build and run all scimesh C++ example apps
#
# Usage:
#   ./examples/cpp/run_all_cpp.sh
#
# Prints a summary at the end showing how many examples passed / failed.

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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

run_one spot_cow                          spot_cow
run_one transparency                      transparency_demo
run_one protein_data_bank_pdb_file        protein_demo        ../1CRN.pdb
run_one whole_brain_annot_single_image    whole_brain_annot
run_one whole_brain_sulc_single_image     whole_brain_sulc
run_one whole_brain_sulc_single_image_fsaverage whole_brain_sulc_fsaverage

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

if [[ ${#FAILED[@]} -gt 0 ]]; then
    exit 1
fi
exit 0
