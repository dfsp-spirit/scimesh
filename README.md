# scimesh

**scimesh** is a headless, GPU-free software renderer for 3D meshes. It
produces publication-quality images entirely on the CPU — no OpenGL, no
X11, no GPU drivers required.  Works anywhere a C++17 compiler **or** R
runs: HPC clusters, headless servers, macOS without XQuartz, containers,
and CI pipelines.

scimesh provides both a **standalone C++ library** and an **R package**
with native bindings, making it easy to render 3D surface meshes to PNG
images for scientific visualization.

## Visualization Examples

![Stanford Dragon rendered with scimesh](web/images/dragon.png)

*The [Stanford Dragon](https://graphics.stanford.edu/data/3Dscanrep/) (871k triangles), rendered with scimesh's
software rasterizer using smooth shading, three-point Blinn-Phong
lighting, and 4x anti-aliasing. Render time 5.2 seconds on the author's mini PC (AMD Ryzen 7 8745H CPU).
[Source code](examples/cpp/dragon/main.cpp).*


![FreeSurfer reconstruction of human brain rendered with scimesh](web/images/whole_brain_sulc.png)

*A FreeSurfer reconstruction of a human brain surface based on an MRI scan (300k triangles), rendered with scimesh's
software rasterizer using smooth shading, no anti-aliasing. Render time 1.8 seconds on the author's mini PC (AMD Ryzen 7 8745H CPU).
[Source code](examples/cpp/whole_brain_sulc_single_image/main.cpp).*

## What scimesh is

- A **headless software renderer** — renders 3D triangle meshes to
  images without any GPU or display server. This means you could use scimesh for non-interactive rendering
  when rgl/OpenGL is unavailable.
- Designed for **scientific mesh visualization** (neuroimaging, molecular structures, geometric primitives).
- Fast enough for publication figures: a ~300k triangle cortical surface at 1200x900 with 2x anti-aliasing renders in 0.2 seconds on a modern CPU.

## Features

### C++ layer

- Software rasterization with z-buffer, smooth and flat shading
- Multi-light Blinn-Phong illumination with specular highlights
- Anti-aliasing via ordered-grid supersampling (2x, 4x SSAA)
- Semi-transparent mesh overlays with depth-sorted alpha blending
- Wireframe rendering with adaptive edge thickness
- Orthographic and perspective projection
- Screen-space ambient occlusion (SSAO)
- Depth cueing (fog)
- Clip planes
- Procedural geometry: spheres, cylinders, cuboids, pyramids, tetrahedra, tori, planes
- Mesh I/O: STL (binary/ASCII), Wavefront OBJ, Stanford PLY
- Image I/O: PNG, PPM, BMP
- Automatic camera framing (`camera_fit_mesh`, `camera_fit_scene`)
- Per-vertex and per-face coloring (vertex colors, face colors)
- Texture mapping with bilinear sampling
- Mesh transform utilities: translate, scale, rotate, arbitrary 4x4 matrices

### R Layer

The R layer gives access to all C++ layer features, and adds on top:

- Image composition: grids, cropping, stacking
- Colorbars (and using them during image composition)
- Anatomical view helpers for neuroimaging (lateral, medial, dorsal, etc.)


## What scimesh is not

- Not a plotting framework — use ggplot2, lattice, or plotly for
  statistical plots.  scimesh renders 3D geometry.
- Not a hardware renderer — no OpenGL, Vulkan, or Metal.  For
  interactive 3D rotation, use rgl.
- Not for real-time or interactive use — it produces static images, and software rendering is not fast enough for real-time use.
- Not a system for physical based rendering — no PBR materials, no raytracing, etc.
- Not a mesh editing/manipulation framework — it reads, transforms,
  and renders meshes but does not provide interactive editing, mesh
  repair, remeshing, or boolean operations.
  For mesh repair and remeshing in R, see the
  [Rvcg](https://cran.r-project.org/package=Rvcg) package (wraps VCGLib).


## Installation

### R package

```r
# Install from GitHub
remotes::install_github("dfsp-spirit/scimesh")

# For FreeSurfer neuroimaging data
remotes::install_github("dfsp-spirit/freesurferformats")
```

### C++ (header + source)

The C++ core lives in `src/core/`.  To use scimesh in a C++ project,
add the source files and include paths to your build system.  See
[`docs/CPP_GETTING_STARTED.md`](docs/CPP_GETTING_STARTED.md) for
details.

## Quick Start

### R

```r
library(scimesh)

# Create a colored cube
cube <- generate_cuboid(c(0, 0, 0), c(1, 1, 1), c(0.2, 0.6, 1.0, 1.0))

# Render it
cam <- camera_auto(cube, direction = c(1, 1, 1))
img <- render_mesh(cube$vertices, cube$triangles,
                   colors = cube$colors, camera = cam)

# Save
write_png(img, "cube.png")
```

### C++

```cpp
#include "renderer.h"
#include "primitives.h"

// Build a mesh, set up camera and options, render, save.
// See docs/CPP_GETTING_STARTED.md for a full walkthrough.
```

## Documentation

- **R vignette**: `vignettes/scimesh.Rmd` — comprehensive guide to the
  R package with examples for all major features.
- **C++ getting started**: [`docs/CPP_GETTING_STARTED.md`](docs/CPP_GETTING_STARTED.md)
  — gentle introduction to the C++ renderer.
- **R examples**: [`examples/R/`](examples/R/) — runnable scripts
  demonstrating textured rendering, transparency, and neuroimaging
  visualization.
- **C++ examples**: [`examples/cpp/`](examples/cpp/) — standalone
  programs covering textured OBJ, transparency, protein visualization,
  and whole-brain rendering.

## Examples

| Example | Language | Description |
|---------|----------|-------------|
| [`examples/R/spot_cow/`](examples/R/spot_cow/) | R | Textured OBJ mesh (Spot cow) with multi-light setup |
| [`examples/R/transparency/`](examples/R/transparency/) | R | Semi-transparent pial overlay on white matter |
| [`examples/R/vis_subject_morph_native/`](examples/R/vis_subject_morph_native/) | R | FreeSurfer sulcal depth visualization with colorbar |
| [`examples/cpp/spot_cow/`](examples/cpp/spot_cow/) | C++ | Textured OBJ rendering with bilinear sampling |
| [`examples/cpp/bunny/`](examples/cpp/bunny/) | C++ | High-quality Stanford Bunny with SSAO, specular, and 4x AA |
| [`examples/cpp/dragon/`](examples/cpp/dragon/) | C++ | Stanford Dragon with SSAO, specular, and 4x AA |
| [`examples/cpp/transparency/`](examples/cpp/transparency/) | C++ | Multi-mesh alpha blending with FreeSurfer data |
| [`examples/cpp/protein_data_bank_pdb_file/`](examples/cpp/protein_data_bank_pdb_file/) | C++ | Protein ball-and-stick visualization from PDB files |
| [`examples/cpp/whole_brain_sulc_single_image/`](examples/cpp/whole_brain_sulc_single_image/) | C++ | Whole-brain sulcal depth rendering |
| [`examples/cpp/whole_brain_sulc_single_image_fsaverage/`](examples/cpp/whole_brain_sulc_single_image_fsaverage/) | C++ | Same on fsaverage template |
| [`examples/cpp/whole_brain_annot_single_image/`](examples/cpp/whole_brain_annot_single_image/) | C++ | Cortical parcellation visualization |

## Development


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

## Vendored Dependencies

Thanks heaps to the authors of these great software packages that scimesh is built upon -- and of course, to the authors of the dependencies of these packages.

- `cpp_tests/catch_amalgamated.{h,cpp}`: [catchorg/Catch2](https://github.com/catchorg/Catch2/tree/devel/extras) — C++ test framework.
- `src/core/glm/`: [g-truc/glm](https://github.com/g-truc/glm) — header-only C++ math library.
- `src/core/tinyply.{h,cpp}`: [ddiakopoulos/tinyply](https://github.com/ddiakopoulos/tinyply) — PLY file reader.
- `src/core/tiny_obj_loader.h`: [tinyobjloader/tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) — Wavefront OBJ loader.
- `src/core/libfs.h`: [dfsp-spirit/libfs](https://github.com/dfsp-spirit/libfs) — FreeSurfer file format reader.
- `cpp_tests/stb_image.h`, `cpp_tests/stb_image_write.h`: [nothings/stb](https://github.com/nothings/stb) — image loading/saving.

Note: This section is to give credit only. Users do not need to worry about installing these, they come packaged (vendored) with scimesh already.

## License and Author

License: [MIT](./LICENSE)

Author: [Tim Schäfer](https://ts.rcmd.org/)

