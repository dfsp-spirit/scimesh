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

declare -a EXAMPLES=(
    all_primitives:all_primitives
    spot_cow:spot_cow
    transparency:transparency_demo
    protein_data_bank_pdb_file:protein_demo
    whole_brain_annot:whole_brain_annot
    whole_brain_sulc:whole_brain_sulc
    whole_brain_sulc_fsaverage:whole_brain_sulc_fsaverage
    bunny:bunny
    dragon:dragon
    brain_video:brain_video
)

PASSED=()
FAILED=()

run_one() {
    local dir="$1"
    local exe="$2"

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

    echo "[run] $exe"
    if "./$exe"; then
        echo "  PASS"
        PASSED+=("$dir")
    else
        echo "  FAIL: exited with non-zero status"
        FAILED+=("$dir (runtime)")
    fi
}

for entry in "${EXAMPLES[@]}"; do
    dir="${entry%%:*}"
    exe="${entry##*:}"
    [[ -z "$FILTER" || "$FILTER" == "$dir" ]] || continue
    run_one "$dir" "$exe"
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
    echo "  Available: ${EXAMPLES[*]%%:*}"
    exit 2
fi

if [[ ${#FAILED[@]} -gt 0 ]]; then
    exit 1
fi
exit 0
