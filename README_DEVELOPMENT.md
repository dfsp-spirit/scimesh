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


### Making a release


* bump version in DESCRIPTION
* make sure new additions have proper doc strings and tests, then build/refresh all docs. In R, run `devtools::document()` to re-generate docs.
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

