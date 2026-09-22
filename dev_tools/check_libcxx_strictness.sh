#!/usr/bin/env bash
#
# check_libcxx_strictness.sh — compile every translation unit of the R package
# with clang + libc++ 14, i.e. the toolchain of the CRAN macOS builders.
#
# Why this exists: our own builds are all lenient in the same way.  libstdc++
# (GCC, used by R on Linux and Windows) and libc++ >= 15 (Xcode 15+, i.e. the
# GitHub `macos-latest` runner) both pull a number of standard headers in
# transitively - `<array>` arrives via `<functional>` - so a translation unit
# that uses `std::array` without including `<array>` compiles there.  libc++ 14
# does not, and the package then fails to install on macOS with
#
#   error: implicit instantiation of undefined template 'std::array<...>'
#   note: template is declared here
#         .../c++/v1/__tuple:219:64
#
# because libc++ only *forward-declares* `std::array` in `<__tuple>` (it needs
# the declaration for `tuple_size`/`tuple_element`), which is why the message is
# about an "undefined template" rather than a missing name.
#
# That is exactly what broke the CRAN installation of scimesh 0.3.4 on
# r-release-macos-x86_64, r-oldrel-macos-x86_64 and r-oldrel-macos-arm64 (all
# three report `Apple clang version 14.0.3` + `MacOSX11.3.sdk`); the fix in
# 0.4.0 was to include `<array>` in src/core/primitives.cpp.
#
# The check is `-fsyntax-only`, so nothing is linked and libc++abi is not
# needed.  This is a local development tool; the `C++ stdlib strictness` CI
# workflow runs it on every push/PR.
#
# Requirements (Linux): clang, plus libc++ headers of version <= 14.  On
# Debian/Ubuntu: `sudo apt install clang-14 libc++-14-dev`.
#
# Usage:
#   dev_tools/check_libcxx_strictness.sh
#   CXX=clang++-14 dev_tools/check_libcxx_strictness.sh     # specific compiler
#   LIBCXX_INC=/usr/lib/llvm-14/include/c++/v1 dev_tools/check_libcxx_strictness.sh
#
# A libc++ >= 15 cannot reproduce the strict behaviour, so the script refuses to
# run with one; set LIBCXX_STRICT_ALLOW_NEWER=1 to check anyway (for the record
# only, it will not catch missing standard headers).
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$REPO_ROOT/src"

die() { printf 'error: %b\n' "$*" >&2; exit 1; }

# ---- compiler ---------------------------------------------------------------
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
    for cand in clang++-14 clang++-13 clang++-12 clang++; do
        if command -v "$cand" >/dev/null 2>&1; then CXX="$cand"; break; fi
    done
fi
[ -n "$CXX" ] || die "no clang++ found. Install clang-14 or set CXX."
command -v "$CXX" >/dev/null 2>&1 || die "'$CXX' not found."

# ---- libc++ headers ---------------------------------------------------------
if [ -z "${LIBCXX_INC:-}" ]; then
    for cand in \
        /usr/lib/llvm-14/include/c++/v1 \
        /usr/include/c++/v1 \
        /usr/local/include/c++/v1 \
        /Library/Developer/CommandLineTools/usr/include/c++/v1 \
        /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1
    do
        if [ -d "$cand" ]; then LIBCXX_INC="$cand"; break; fi
    done
fi
[ -n "${LIBCXX_INC:-}" ] && [ -d "$LIBCXX_INC" ] \
    || die "no libc++ headers found. On Debian/Ubuntu: sudo apt install libc++-14-dev (or set LIBCXX_INC)."

# libc++ 15 and later include <array> transitively, so they do not exercise the
# strict behaviour this script is about.
libcxx_ver="$(grep -A1 -E '^# *define +_LIBCPP_VERSION' "$LIBCXX_INC/__config" 2>/dev/null \
              | grep -oE '[0-9]+' | head -1 || true)"
if [ -n "$libcxx_ver" ] && [ "$libcxx_ver" -ge 15000 ] \
   && [ "${LIBCXX_STRICT_ALLOW_NEWER:-0}" != "1" ]; then
    die "libc++ $libcxx_ver is too new: it provides <array> transitively, so this check would\n       pass even for code that does not include <array>.  Use libc++ <= 14 (Debian/Ubuntu:\n       sudo apt install libc++-14-dev), or set LIBCXX_STRICT_ALLOW_NEWER=1 to check anyway."
fi

# ---- translation units ------------------------------------------------------
# The R package build (see src/Makevars) compiles src/*.cpp and src/core/*.cpp.
FILES=()
for f in "$SRC_DIR"/core/*.cpp; do FILES+=("$f"); done

# The two Rcpp bindings TUs are only checked when R and Rcpp are available; they
# are thin wrappers, all the code lives in src/core.
R_INC=""
RCPP_INC=""
if command -v R >/dev/null 2>&1; then
    R_INC="$(R RHOME 2>/dev/null)/include"
fi
if command -v Rscript >/dev/null 2>&1; then
    RCPP_INC="$(Rscript -e 'cat(system.file("include", package="Rcpp"))' 2>/dev/null)"
fi
if [ -d "$R_INC" ] && [ -d "$RCPP_INC" ]; then
    FILES+=("$SRC_DIR/rcpp_bindings.cpp" "$SRC_DIR/RcppExports.cpp")
    R_INCLUDES=(-I"$R_INC" -I"$RCPP_INC")
else
    R_INCLUDES=()
    echo "note: R or Rcpp not available, skipping src/rcpp_bindings.cpp and src/RcppExports.cpp." >&2
fi

INCLUDES=(-I"$SRC_DIR/core" -I"$SRC_DIR/third_party" -I"$SRC_DIR/third_party/glm" "${R_INCLUDES[@]}")
DEFINES=(-DNDEBUG -DSCIMESH_STB_WRITE_IMPL -DSCIMESH_STB_READ_IMPL \
         -DSCIMESH_STB_TRUETYPE_IMPL -DLIBFS_DBG_NONE)

echo "Checking ${#FILES[@]} translation units with $CXX (libc++ ${libcxx_ver:-unknown}, $LIBCXX_INC)"

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

failed=0
for f in "${FILES[@]}"; do
    rel="${f#"$REPO_ROOT"/}"
    if "$CXX" -std=gnu++17 -fsyntax-only \
       -nostdinc++ -isystem "$LIBCXX_INC" \
       "${DEFINES[@]}" "${INCLUDES[@]}" "$f" >"$LOG" 2>&1
    then
        printf '  ok    %s\n' "$rel"
    else
        printf '  FAIL  %s\n' "$rel"
        sed 's/^/        /' "$LOG"
        failed=$((failed + 1))
    fi
done

if [ "$failed" -ne 0 ]; then
    echo "FAILED: $failed of ${#FILES[@]} translation units do not compile with libc++ 14." >&2
    echo "        A missing standard header (e.g. <array>) is the usual reason." >&2
    exit 1
fi
echo "OK: all ${#FILES[@]} translation units compile with libc++ 14."
