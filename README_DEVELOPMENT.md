## scimesh Development Information


### Required system dev dependencies

To build and check the R package, you will need these:

```shell
sudo apt install build-essential qpdf tidy pandoc libuv1-dev libharfbuzz-dev libfribidi-dev
```

You must also install all R packages which are `suggested` by scimesh, see the [package DESCRIPTION](./DESCRIPTION). Also make sure you have the basics, in R:

```R
install.packages(c("devtools", "knitr", "remotes"))
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
