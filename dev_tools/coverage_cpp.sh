#!/usr/bin/env bash
#
# coverage_cpp.sh — LLVM source-based coverage for the scimesh C++ core.
#
# Builds the C++ unit tests with clang coverage instrumentation, runs them, and
# writes a report into coverage/:
#
#   coverage/index.html      per-file HTML with line/branch counts
#   coverage/lcov.info       lcov trace file (for editor gutters, e.g. the
#                            VS Code "Coverage Gutters" extension)
#   coverage/scimesh.profdata merged LLVM profile data
#
# The report covers src/core only: test code, the Catch2 amalgamation and all
# vendored third-party code (src/third_party, which also holds the header-only
# libraries included by the core) are excluded.  Coverage of the R layer is not
# measured here - use covr::package_coverage() for that.
#
# This is a local development tool: it uploads nothing, has no badge and no
# coverage threshold (numerous third-party headers would break any gate).
#
# Requirements (Linux): clang++, llvm-profdata and llvm-cov of the same major
# version, plus cmake.
#
# Usage:
#   dev_tools/coverage_cpp.sh                  # build, run all tests, report
#   dev_tools/coverage_cpp.sh "[fog]"          # only matching Catch2 tests
#   CXX=clang++-18 dev_tools/coverage_cpp.sh   # use a specific clang
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/cpp_tests/build-coverage"
OUT_DIR="$REPO_ROOT/coverage"
PROF_DIR="$OUT_DIR/prof"
TEST_BIN="$BUILD_DIR/scimesh_tests"

CXX="${CXX:-clang++}"
CC="${CC:-clang}"

# Paths to drop from the report: test code, the Catch2 amalgamation and all
# vendored third-party code (glm/stb/libfs/tinyobj/tinyply are header-only, so
# their instantiations show up in the library's translation units).
IGNORE_FILES='(/cpp_tests/|/src/third_party/|catch_amalgamated|rcpp_bindings)'

die() { echo "error: $*" >&2; exit 1; }

# ---- locate the LLVM tools that match the compiler --------------------------
command -v "$CXX" >/dev/null 2>&1 || die "'$CXX' not found. Install clang or set CXX."

clang_major="$("$CXX" --version | head -1 | grep -oE '[0-9]+' | head -1)"
[ -n "$clang_major" ] || die "could not determine the clang major version of '$CXX'."

find_llvm_tool() {
    local tool="$1" cand dir
    for cand in "${tool}-${clang_major}" "$tool"; do
        if command -v "$cand" >/dev/null 2>&1; then
            command -v "$cand"
            return 0
        fi
    done
    for dir in /usr/lib/llvm-*/bin /usr/lib/llvm/bin /usr/local/opt/llvm/bin /opt/homebrew/opt/llvm/bin; do
        if [ -x "$dir/$tool" ]; then
            echo "$dir/$tool"
            return 0
        fi
    done
    return 1
}

llvm_profdata="$(find_llvm_tool llvm-profdata)" || die \
    "llvm-profdata (version $clang_major) not found. On Debian/Ubuntu install 'llvm-$clang_major' (or 'llvm')."
llvm_cov="$(find_llvm_tool llvm-cov)" || die \
    "llvm-cov (version $clang_major) not found. On Debian/Ubuntu install 'llvm-$clang_major' (or 'llvm')."

echo "== scimesh C++ coverage =="
echo "  compiler     : $("$CXX" --version | head -1)"
echo "  llvm-profdata: $llvm_profdata"
echo "  llvm-cov     : $llvm_cov"
echo "  build dir    : ${BUILD_DIR#$REPO_ROOT/}"
echo "  report       : ${OUT_DIR#$REPO_ROOT/}"

# ---- configure & build ------------------------------------------------------
mkdir -p "$OUT_DIR"

if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "$BUILD_DIR/CMakeCache.txt" | head -1)"
    if [ -n "$cached_cxx" ] && [ "$(basename "$cached_cxx")" != "$(basename "$(command -v "$CXX")")" ]; then
        echo "note: build dir was configured with '$cached_cxx'; rerun with the same compiler or delete ${BUILD_DIR#$REPO_ROOT/}"
    fi
fi

echo ""
echo "[1/4] configuring (SCIMESH_COVERAGE=ON, Debug)"
cmake -S "$REPO_ROOT/cpp_tests" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSCIMESH_COVERAGE=ON \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    > "$OUT_DIR/cmake_configure.log" 2>&1 \
    || { tail -20 "$OUT_DIR/cmake_configure.log"; die "cmake configure failed (see ${OUT_DIR#$REPO_ROOT/}/cmake_configure.log)"; }

echo "[2/4] building"
cmake --build "$BUILD_DIR" -j "$(nproc 2>/dev/null || echo 4)" > "$OUT_DIR/cmake_build.log" 2>&1 \
    || { tail -30 "$OUT_DIR/cmake_build.log"; die "build failed (see ${OUT_DIR#$REPO_ROOT/}/cmake_build.log)"; }
[ -x "$TEST_BIN" ] || die "test binary not found at ${TEST_BIN#$REPO_ROOT/}"

# ---- run the tests with profiling ------------------------------------------
rm -rf "$PROF_DIR" "$OUT_DIR/html"
mkdir -p "$PROF_DIR"

echo "[3/4] running the test suite"
( cd "$BUILD_DIR" && LLVM_PROFILE_FILE="$PROF_DIR/%p.profraw" "$TEST_BIN" "$@" ) \
    | tail -5
prof_count="$(find "$PROF_DIR" -name '*.profraw' | wc -l)"
[ "$prof_count" -gt 0 ] || die "no profile data written to ${PROF_DIR#$REPO_ROOT/}"

"$llvm_profdata" merge -sparse "$PROF_DIR"/*.profraw -o "$OUT_DIR/scimesh.profdata"

# ---- report -----------------------------------------------------------------
echo "[4/4] writing report"
"$llvm_cov" report "$TEST_BIN" -instr-profile="$OUT_DIR/scimesh.profdata" \
    --ignore-filename-regex="$IGNORE_FILES" | tail -40

"$llvm_cov" show "$TEST_BIN" -instr-profile="$OUT_DIR/scimesh.profdata" \
    --ignore-filename-regex="$IGNORE_FILES" \
    --format=html --output-dir="$OUT_DIR/html" \
    --show-line-counts-or-regions --show-branches=count > /dev/null

"$llvm_cov" export "$TEST_BIN" -instr-profile="$OUT_DIR/scimesh.profdata" \
    --ignore-filename-regex="$IGNORE_FILES" --format=lcov > "$OUT_DIR/lcov.info"

echo ""
echo "Done. Open ${OUT_DIR#$REPO_ROOT/}/html/index.html, or use lcov.info in your editor."
