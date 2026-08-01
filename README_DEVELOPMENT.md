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

