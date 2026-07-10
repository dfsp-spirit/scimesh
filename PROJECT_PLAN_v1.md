# Project Specification and Scope Document: `scimesh`

This document defines the architecture, feature scope, and implementation boundaries for `scimesh`, a minimal, fast, headless 3D software renderer written in modern C++ with native R bindings.

The primary objective of this library is to serve as a zero-dependency, fallback rendering engine for neuroimaging meshes (e.g., Freesurfer cortical surfaces) when traditional windowing/GPU-backed frameworks (like `rgl` or X11/OpenGL configurations) fail, are unavailable, or are restricted by modern operating system windowing constraints (such as macOS headless limitations).

---

## 1. Core Feature List & Scientific Rationale

Every feature included in the core engine is strictly justified by the requirements of structural neuroimaging visualization:

* **Header-Only, Zero-Dependency C++ Core Architecture**
* *Rationale:* Guarantees seamless cross-platform compilation (macOS, Linux, Windows) on CRAN check servers without requiring system-level graphics contexts, window toolkits, or external binary linkers.


* **Direct-to-Memory Framebuffer (RGBA Canvas)**
* *Rationale:* Bypasses the GPU, windowing environment, and screen display servers entirely. Allows high-throughput image rendering natively on remote headless compute clusters and high-performance computing (HPC) nodes.


* **Alpha Channel / Transparent Background Support**
* *Rationale:* Neuroimaging publications, multi-panel papers, and presentations require brain meshes to be seamlessly composited onto white, custom-colored, or complex multi-figure layouts without solid bounding-box artifacts.


* **Barycentric Bounding-Box Rasterizer with Depth-Buffering (Z-Buffer)**
* *Rationale:* Handles complex, overlapping cortical geometry natively. Accurate depth sorting ensures foreground gyri and sulci cleanly occlude deep hidden surfaces. The bounding-box processing strategy ensures predictable, cache-friendly CPU loops.


* **On-the-Fly Normal Vector Computation (Vertex and Face)**
* *Rationale:* Standard surface file formats (e.g., Freesurfer `.white`, `.pial`, or raw `.obj`/`.ply` matrices) frequently omit surface normals. The engine must compute normals directly from the winding order of the face indices to evaluate directional lighting equations.


* **Backface Culling**
* *Rationale:* Cortical surface models represent closed, organic shapes. Approximately 50% of the triangles face away from any given camera viewpoint at any moment. Discarding these triangles instantly cuts downstream pixel rasterization overhead in half.


* **Flexible Camera Projection Stack (Orthographic & Perspective)**
* *Rationale:* Orthographic projection is essential in clinical and scientific visualization to preserve spatial relationships and metric scales across standard view axes. Perspective projection is utilized for natural anatomical renderings.


* **Camera-Aligned Directional Light ("Headlight Shading")**
* *Rationale:* Raw vertex colors on an unlit 3D model destroy all structural depth perception, making a highly folded brain mesh appear flat. A fixed light source bound to the camera casting onto calculated normals creates shadows in the sulci and highlights on the gyri, immediately resolving 3D topology.



---

## 2. Strict Boundary Matrix: What Is Out of Scope

To prevent scope creep and ensure the project can be completed inside a one-week development sprint, the following features are **explicitly excluded** from the C++ layer.

| Feature / Capability | Status | Handling / Alternative |
| --- | --- | --- |
| **Interactive Event Loop** | **OUT** | No mouse inputs, windows, or frame refresh loops. Input $\rightarrow$ Output image. |
| **Complex Materials / Texturing** | **OUT** | No PBR shaders, glass, reflectivity, or UV texture maps. Vertex colors only. |
| **In-Engine Text Rendering** | **OUT** | No TrueType font handling or text rasterization loops. Handled downstream via R. |
| **In-Engine Colorbars** | **OUT** | No vector layout generators inside C++. Managed via R graphics vectors. |
| **Anti-Aliasing (MSAA/FXAA)** | **OUT** | Super-sampling is achieved by rendering at $2\times$ scale and downsampling in R. |
| **Global Illumination / Shadows** | **OUT** | Avoids expensive raytracing or raymarching. Headlight shading is sufficient. |

---

## 3. Architectural Separation of Concerns

The project utilizes a strict division of labor between high-performance C++ rendering operations and high-level R composition engines.

```
+--------------------------------------------------------+
|                      R INTERFACE                       |
|  - Ingests mesh3d objects (vertices, indices, colors) |
|  - Validates array structures and 1-based indexing     |
+---------------------------+----------------------------+
                            |
                            | (Rcpp Zero-Copy Arrays)
                            ▼
+--------------------------------------------------------+
|                  PURE C++ RENDER ENGINE                |
|  - Compute Normals & Camera Transformations            |
|  - Backface Cull & Depth-Buffered Rasterization       |
|  - Write raw RGBA buffer to disk (stb_image_write)     |
+---------------------------+----------------------------+
                            |
                            | (Headless Alpha-Transparent PNG)
                            ▼
+--------------------------------------------------------+
|                    R LAYOUT ENGINE                     |
|  - Tile multiple view matrices (Lateral, Medial, etc.)  |
|  - Overlay vector text labels via grid/patchwork       |
|  - Append vector colorbar scales natively              |
+--------------------------------------------------------+

```

### Multi-View Tiling, Colorbars, and Annotation Execution

Instead of complicating the C++ core with layout mechanics, the package delegates publication assembly to R's native vector graphics engine:

1. **Multi-Angle Tiling:** The C++ core generates individual, transparent PNG views (e.g., Lateral, Medial). R ingests these images into native layout objects using `grid::rasterGrob()`. Multi-panel grids (e.g., $2\times2$ or $1\times4$ orientations) are composed natively using standard layout libraries like `gridExtra`, `cowplot`, or `patchwork`.
2. **Colorbars:** The R package leverages the existing structural palettes and numeric morphometry vectors to plot clean, resolution-independent vector colorbars natively using R base graphics or `ggplot2`, overlaying them adjacent to the image arrays.
3. **Annotations:** Spatial text labels (e.g., hemisphere titles, statistical thresholds) are written into the final layout using R's text engines (`grid::textGrob()`), guaranteeing crisp typography matching the document's design layout without requiring font file compilation inside the C++ engine.

---

## 4. Development Layout & Testing Matrix

To guarantee maximum developer efficiency and minimize friction, the project will be maintained within a **single repository** using a dual-track isolated build system.

### Directory Mapping

* `R/`: Contains user-facing wrapper functions, mesh extractions, and the `grid`/`patchwork` multi-panel composition pipelines.
* `src/rcpp_bindings.cpp`: Acts strictly as the isolated translation boundary. Maps R data types to simple, native C++ containers. This file is the only file allowed to reference R hooks.
* `src/core/`: Dedicated, pure C++ source repository containing standard math utilities, camera matrices, and rasterization algorithms. Completely agnostic to R.
* `tests/testthat/`: Holds high-level R interface unit tests confirming class validation, type checks, and user-facing error limits.
* `cpp_tests/`: Standalone, isolated C++ workspace containing a separate testing harness (e.g., `Catch2` or `doctest`) backed by a lightweight `CMakeLists.txt` profile.


Proposed layout:

```shell
scimesh/
├── DESCRIPTION
├── NAMESPACE
├── .Rbuildignore
│
├── R/
│   ├── render_mesh.R         # R wrapper exposing user-facing functions
│   └── cbar_layout.R         # Headless grid/patchwork composition logic
│
├── src/                      # CRAN compiled code folder
│   ├── Makevars              # Compiler flags (-O3)
│   ├── Makevars.win
│   ├── rcpp_bindings.cpp     # Rcpp translation boundary (ONLY file with R hooks)
│   │
│   └── core/                 # Pure C++ Engine (100% independent of R)
│       ├── renderer.h        # Main rasterization loops & Z-buffer
│       ├── camera.h          # LookAt matrix math
│       ├── math_utils.h      # Vector/Matrix structs & transformations
│       └── stb_image_write.h # Header-only PNG output writer
│
├── tests/                    # Standard R package tests
│   ├── testthat.R
│   └── testthat/
│       └── test-interface.R  # Tests for matrix validations & inputs
│
└── cpp_tests/                # Standalone C++ Testing Workspace
    ├── CMakeLists.txt        # Build profile for independent C++ compiling
    ├── main_test.cpp         # Test runner (Catch2 / doctest)
    └── test_rasterizer.cpp   # Unit tests for the core/ algorithms
```

### Dual-Track Development Workflow

* **Track A (C++ Engine Cycle):** Optimization of raster loops, vertex transformation corrections, or depth-buffer updates are developed and compiled entirely inside `cpp_tests/`. Compilation and execution require milliseconds, completely freeing the developer from the overhead of re-installing the R extension layer during foundational graphics development.
* **Track B (R Integration Cycle):** Once the C++ engine passes its isolated functional test constraints, the R package wrapper interfaces are configured and validated using standard package test suites.

## 5. Coordinate System Alignment & Camera Mapping Philosophy

To ensure a seamless transition for users moving from stateful interactive rendering to headless file production, `scimesh` aligns its geometrical foundations with the established conventions of the R graphics and neuroimaging ecosystems while strictly shielding the C++ core from legacy architectural quirks.

### 5.1 Coordinate System Mirroring
The C++ core implements a traditional **Right-Handed Coordinate System** to natively match `rgl` and standard neuroimaging spatial specifications (e.g., RAS: Right-Anterior-Superior):
* **X-Axis:** Positive values extend to the right.
* **Y-Axis:** Positive values extend upward.
* **Z-Axis:** Positive values extend outward (toward the camera viewpoint).

By keeping this mathematical parity, vertex arrays extracted from `mesh3d` objects can be fed directly into the C++ vertex processor without requiring on-the-fly matrix inversions or axis swapping at the R interface boundary.

### 5.2 Decoupling Camera Logic (API vs. Implementation)
Interactive toolkits like `rgl` manage camera positions using an intrinsic, stateful combination of spherical coordinates (theta/phi angles), pan, and bounding-box zoom states optimized for mouse interactions. To maintain code readability and engine reusability, `scimesh` splits this responsibility:

* **The C++ Engine Layer:** Remains entirely textbook and stateless. It accepts only a standard, explicit mathematical **LookAt Matrix** parameterized by three pure spatial vectors: `Eye` (camera coordinates), `Center` (target focal coordinates), and `Up` (the camera's orientation vector).
* **The R Wrapper Layer:** Natively handles usability mapping. It exposes user-facing configuration functions that mimic the traditional `rgl::view3d(theta, phi, zoom)` interface. The R layer performs the trigonometric translation of these spherical angles into explicit $XYZ$ coordinates and injects them down into the clean C++ LookAt stack.

### 5.3 Proportional Safety via Explicit Viewports
While `rgl` dynamically responds to desktop window modifications—often introducing aspect-ratio distortion if a window is dragged non-uniformly—`scimesh` operates under absolute dimensional constraints.

The image dimensions (`width` and `height` in pixels) must be declared explicitly at execution time. The projection matrix calculates the aspect ratio strictly as $\frac{\text{width}}{\text{height}}$, guaranteeing that structural mesh features and anatomical proportions remain perfectly locked and invariant across all rendering scales and output file resolutions.


## 6. Headless Colorbar Generation & Layout Composition Architecture

For scientific visualization, an accurate colorbar is as critical as the 3D geometry itself. `scimesh` decouples colorbar production from the 3D mesh engine and handles it entirely in R using `magick` for pixel-perfect compositing.

### 6.1 Colorbar Generation Strategy

Instead of relying on R's grid graphics (which require active display devices and produce unpredictable sizes), colorbars are generated as **deterministic raster images**:

1. **Color strip:** Generate directly as a raster matrix from the colormap function, write to PNG
2. **Tick labels:** Add using `magick::image_annotate()` with known font metrics
3. **Fixed dimensions:** Colorbar images have predictable sizes (e.g., 600×80px horizontal), eliminating layout guesswork

This approach requires no X11, no graphics device, and produces consistent output across platforms.

### 6.2 Layout Composition with Magick

Multi-panel figures are assembled using `magick::image_append()`:

```r
# Side-by-side brain views
brain_views <- image_append(c(lateral, medial), stack=FALSE)
# Stack with colorbar below
final <- image_append(c(brain_views, colorbar), stack=TRUE)
```

**Advantages:**
- Pixel-perfect control with no automatic layout calculations
- Works headlessly (no X11 needed)
- Predictable sizing — dimensions are known upfront
- Can cache colorbar images and reuse across plots



## 7. Rationale for Custom Engine Architecture vs. Existing Frameworks

A foundational question for any engineering project is why a custom renderer is necessary when mature open-source solutions exist. During the scoping phase, three prominent MIT-licensed candidates were thoroughly evaluated: `ssloy/tinyrenderer`, `elnormous/SoftwareRenderer`, and `trenki2/SoftwareRenderer`. While excellent in their respective domains, these engines carry heavy abstractions designed to emulate complex, stateful graphics APIs (like OpenGL or Vulkan) or real-time game loops, introducing unnecessary architectural bloat, complex dependency chains, and difficult memory management for data passing. Because `scimesh` strictly rejects real-time windowing, texturing, global illumination, and interactive event loops, the required functional footprint is exceptionally small (~300–400 lines of pure C++). Developing the engine from scratch ensures that data structures map perfectly and zero-copy to R matrices, guarantees absolute cross-platform compliance under CRAN's strict memory sanitizers (ASAN/UBSAN), and results in a radically maintainable codebase perfectly optimized for the highly specific niche of headless neuroimaging visualization.


## 8. Performance Budget Analysis

A typical publication-quality brain plot requires 2–4 mesh renders plus R-side composition (colorbars, labels, layout). The current `fsbrain` pipeline takes 8–10 seconds total, leaving approximately **1.5 seconds per rendering** as the performance target for the C++ engine.

### Feasibility Assessment

**Typical input sizes:**
- Freesurfer cortical surfaces: 150k–300k triangles per hemisphere
- Publication resolution: 800×600 to 1200×900 pixels

**Estimated costs per view (300k triangles at 1000×750):**
- Triangle setup (vertex transform, backface cull, bounding box): ~30–50 ms
- Pixel rasterization (barycentric test, z-buffer, shading): ~200–400 ms
- **Total per view: ~250–500 ms**

This provides comfortable margin within the 1.5-second budget.

### Key Performance Enablers

1. **Bounding-box rasterization** — processes only pixels within each triangle's screen-space bounds, not the entire framebuffer
2. **Cache-friendly memory layout** — row-major framebuffer access with contiguous z-buffer
3. **Compiler optimization** — `-O3` with modern GCC/Clang auto-vectorizes inner pixel loops
4. **Resolution discipline** — 1000×750 is sufficient for publications; avoid unnecessary 4K rendering

The 1.5-second per-view budget is achievable with a clean, well-optimized implementation.


## 9. Implementation Decisions

### 9.1 Far-Plane Clipping: Not Needed

Unlike near-plane clipping (which is critical to prevent artifacts when triangles intersect the camera), far-plane clipping is unnecessary for brain meshes. The camera is positioned to frame the entire mesh, so there are no distant objects to cull. Set the far plane to a large value in the projection matrix and let the z-buffer handle depth ordering naturally.

### 9.2 Anti-Aliasing: Optional 2× Supersampling

Start without anti-aliasing. For brain meshes with smooth shading and vertex colors, aliasing is less noticeable than with sharp edges. If needed, add 2× supersampling as an optional parameter: render at double resolution (e.g., 2000×1500) and downsample in R. This quadruples render time (~400ms → ~1600ms) but remains within budget for typical use cases. For multi-panel figures, each sub-image can be smaller, making supersampling more feasible.

### 9.3 C++ Math Library: Use GLM

Use GLM (OpenGL Mathematics) instead of reimplementing vector/matrix math. GLM is header-only, MIT-licensed, well-tested, and designed for graphics math (Vec3, Vec4, Mat4, transforms). It eliminates bugs in matrix operations and saves development time without adding runtime overhead.

### 9.4 Colorbar and Layout: Keep in R with Magick

Colorbar generation and figure composition belong in R, not C++. Adding text rendering to C++ would require font loading (FreeType dependency) and violate the zero-dependency goal. Use `magick` for pixel-perfect compositing with deterministic sizing, avoiding the unpredictability of grid graphics.




