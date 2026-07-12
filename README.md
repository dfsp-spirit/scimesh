# scimesh
messing with meshes, again. ignore this.


## Vendored work by others

* cpp_tests/catch_amalgamated.h and .cpp: [catchorg/catch2](catch2 from https://github.com/catchorg/Catch2/tree/devel/extras). A modern, C++-native, test framework for unit-tests, TDD and BDD.
* src/core/glm/: [g-truc/glm](https://github.com/g-truc/glm/releases). A header only C++ mathematics library for graphics software based on the OpenGL Shading Language (GLSL) specifications.
* src/core/tinyply.h and .cpp: [ddiakopoulos/tinyply](https://github.com/ddiakopoulos/tinyply). A single-header, zero-dependency (except the C++ STL) public domain implementation of the PLY mesh file format.
* src/core/tiny_obj_loader.h: [tinyobjloader/tinyobjloader](https://github.com/tinyobjloader/tinyobjloader). Tiny but powerful Wavefront .obj/.mtl loader.
* src/core/libfs.h: [dfsp-spirit/libfs](https://github.com/dfsp-spirit/libfs). A portable, header-only, single file, no-dependency, mildly templated, C++11 library for accessing FreeSurfer neuroimaging file formats.

## Development

### Running the C++ unit tests

The C++ unit tests use [Catch2](https://github.com/catchorg/Catch2) (amalgamated single-header version) and are located in `cpp_tests/`. To build and run them:

```sh
cd cpp_tests
cmake -B build
cmake --build build
./build/scimesh_tests
```

### TODO

* Add to the demos apps that render some meshes from:
    - Keenan Crane's Model Repository https://www.cs.cmu.edu/~kmcrane/Projects/ModelRepository/ (for UV stuff, nice clean meshes)
    - Morgan McGuire's Computer Graphics Archive at https://casual-effects.com/data/ (clean, standard assets)
    - The Stanford 3D Scanning Repository at https://graphics.stanford.edu/data/3Dscanrep/ (for real-world, messy models)