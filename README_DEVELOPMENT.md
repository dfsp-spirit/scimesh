## scimesh Development Information


### Required system dev dependencies

To build and check the R package, you will need these:

```shell
sudo apt install build-essential qpdf tidy pandoc libuv1-dev libharfbuzz-dev libfribidi-dev
```

You must also install all R packages which are `suggested` by scimesh, see the [package DESCRIPTION](./DESCRIPTION).

Also make sure you have the basics, in R:

```R
install.packages(c("devtools", "knitr", "remotes"))
```

And finally, install tinytex for vignette building:

```R
install.packages("tinytex")
tinytex::install_tinytex()
```

### Running the C++ unit tests

The C++ tests use [Catch2](https://github.com/catchorg/Catch2) (amalgamated)
and are in `cpp_tests/`:

```sh
cd cpp_tests
cmake -B build
cmake --build build
./build/scimesh_tests
```

### C++ code coverage

Coverage of the C++ core is measured with clang's source-based coverage and
reported with `llvm-cov`.  The helper script builds the test suite with
instrumentation, runs it and writes a report:

```sh
dev_tools/coverage_cpp.sh                 # build, run all tests, report
dev_tools/coverage_cpp.sh "[fog]"         # only matching Catch2 tests
CXX=clang++-18 dev_tools/coverage_cpp.sh  # use a specific clang
```

**Requirements (Linux):** `clang`, plus `llvm-profdata` and `llvm-cov` of the
same major version, and `cmake`.  On Debian/Ubuntu: `sudo apt install clang llvm`.

The report is written to `coverage/` (gitignored): `coverage/html/index.html`
for browsing, `coverage/lcov.info` if you want coverage gutters in your editor
(e.g. the VS Code "Coverage Gutters" extension).

Notes:

* Test code, the Catch2 amalgamation and all vendored third-party code
  (`src/third_party/`, whose header-only libraries are inlined into the core)
  are excluded from the report.
* Library files that no test references are not linked into the test binary at
  all, so they do not show up in the report - not even as 0 % (e.g.
  `obj_io.cpp` and `transforms.cpp`, which are exercised from R instead).  A file
  shown at 0 % is linked but never executed (e.g. `ply_io.cpp`).
* Coverage is a local development tool: nothing is uploaded, there is no badge
  and there are no coverage thresholds (vendored third-party headers would break
  any gate).  The `Coverage (C++)` CI workflow stores the HTML report as an
  artifact but never fails on low coverage.
* Coverage of the R layer is not measured here - use `covr::package_coverage()`
in R for that.  The C++ code inside the R package is a separate build (see
`src/Makevars`) and is not covered by this script.

### Checking compatibility with the macOS toolchain

A missing standard header is invisible to almost every build we have: libstdc++
(GCC, i.e. R on Linux and Windows) and libc++ 15+ (Xcode 15+, i.e. the
`macos-latest` CI runner) both pull headers such as `<array>` in transitively
via `<functional>`.  The CRAN macOS builders use the clang 14 / libc++ 14
toolchain, which does not - so such code fails there with a confusing
`implicit instantiation of undefined template 'std::array<...>'` (this is what
broke the CRAN installation of 0.3.4).  Compile every translation unit with
libc++ 14 to check:

```sh
dev_tools/check_libcxx_strictness.sh                    # needs libc++ <= 14
CXX=clang++-14 dev_tools/check_libcxx_strictness.sh     # use a specific clang
LIBCXX_INC=/usr/lib/llvm-14/include/c++/v1 dev_tools/check_libcxx_strictness.sh
```

**Requirements (Linux):** `clang` plus the libc++ headers of version <= 14.  On
Debian/Ubuntu: `sudo apt install clang-14 libc++-14-dev`.  The check is
`-fsyntax-only`, so nothing is linked and R is only needed for the two Rcpp
binding translation units (they are skipped when R or Rcpp is missing).  The
script refuses to run with libc++ >= 15, because that standard library cannot
reproduce the strict behaviour.  The `C++ stdlib strictness` CI workflow runs
this on every push/PR, on the pinned `ubuntu-24.04` image (newer Ubuntu releases
no longer ship libc++ 14).

### Examples

The example programs are part of what we ship, so they are built and run:

```sh
./examples/cpp/run_all_cpp.sh      # builds and runs every C++ example
./examples/R/run_all_R.sh          # runs every R example (needs scimesh installed)
```

Both can take a name to run a single example.  The `Examples` CI workflow does
this on every push/PR, because the C++ examples are not compiled by any test
target - breakage in them would otherwise go unnoticed.

### Running the R unit tests

```r
devtools::test()
```

Or via R CMD check:

```r
R CMD build . && R CMD check scimesh_*.tar.gz
```

### Generating C++ API documentation

The C++ API documentation is generated with [Doxygen](https://www.doxygen.nl/).
The configuration is in `Doxyfile` at the repository root.

**Prerequisites:** Install Doxygen (and optionally Graphviz for diagrams):

```shell
sudo apt install doxygen graphviz
```

**Generate locally:**

```shell
# From the repository root:
doxygen Doxyfile
```

This produces HTML output in `docs/cpp_api/html/`. Open `docs/cpp_api/html/index.html`
in a browser to view the docs.

The published API documentation is available at:
`https://dfsp-spirit.github.io/scimesh/`

It is built and deployed automatically via GitHub Actions (`.github/workflows/docs.yml`)
on every push to `main`.


### Making a release


* bump version in all of these files:
    * `DESCRIPTION` (R package version)
    * `CMakeLists.txt` — the `project(scimesh VERSION ...)` line (C++ version, single source of truth)
        * The C++ header `version.h` is **auto-generated** by CMake from this value — no need to edit it manually.
    * `Doxyfile` — the `PROJECT_NUMBER` field (version shown in published C++ API docs)
* make sure new additions have proper doc strings and tests, then build/refresh all docs. In R, run `devtools::document()` to re-generate docs.
* **REQUIRED:** Re-run Doxygen so the published C++ API docs pick up the new version in `PROJECT_NUMBER`:
    ```
    doxygen Doxyfile
    ```
    Do not skip this step — the online API documentation at `https://dfsp-spirit.github.io/scimesh/` will show a stale or empty version otherwise.
* run all tests and make sure they are green:

    C++ unit tests:
    ```
    cd cpp_tests && cmake -B build && cmake --build build && ./build/scimesh_tests
    ```

    R unit tests:
    ```
    devtools::test()
    ```

* make sure to run all examples:
    ```
    ./examples/cpp/run_all_cpp.sh
    ./examples/R/run_all_R.sh
    ```
* run `R CMD build .` to build new package version
* run `R CMD check scimesh_0.1.0.tar.gz`, or whatever version you are testing. This must pass without errors/warnings/notes.
* run the even stricter `R CMD check scimesh_0.1.0.tar.gz --as-cran` and see what you can do to get as little notes and warnings as possible. Stuff in downstream code is not our problem though, but you may have to discuss that with CRAN team on submit, hf.
* test package on [winbuilder](https://win-builder.r-project.org/upload.aspx)
* [submit to CRAN](https://cran.r-project.org/submit.html) when green
* when it's accepted at CRAN, tag it in git and publish release on github, with artefact attached

