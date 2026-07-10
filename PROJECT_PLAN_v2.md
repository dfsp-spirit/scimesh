# Project Specification and Scope Document: `scimesh` (v2)

This document defines the architecture, feature scope, and implementation boundaries for `scimesh`, a minimal, fast, headless 3D software renderer written in modern C++ with native R bindings.

The primary objective of this library is to serve as a zero-dependency, fallback rendering engine for neuroimaging meshes (e.g., Freesurfer cortical surfaces) when traditional windowing/GPU-backed frameworks (like `rgl` or X11/OpenGL configurations) fail, are unavailable, or are restricted by modern operating system windowing constraints (such as macOS headless limitations).

---

## 1. Core Feature List & Scientific Rationale

Every feature included in the core engine is strictly justified by the requirements of structural neuroimaging visualization:

* **Zero-Dependency, Multi-File C++ Core Architecture**
* *Rationale:* Guarantees seamless cross-platform compilation (macOS, Linux, Windows) on CRAN check servers without requiring system-level graphics contexts, window toolkits, or external binary linkers. A conventional multi-file structure (not header-only) improves maintainability, compile times, and long-term extensibility.


* **In-Memory RGBA Image Output**
* *Rationale:* The renderer produces an in-memory `Image` object (width, height, RGBA pixel buffer). File export (PNG, etc.) is handled by separate utility functions, maintaining clean separation of concerns. This allows high-throughput rendering on remote headless compute clusters and HPC nodes without filesystem I/O during rendering.


* **Alpha Channel / Transparent Background Support**
* *Rationale:* Neuroimaging publications, multi-panel papers, and presentations require brain meshes to be seamlessly composited onto white, custom-colored, or complex multi-figure layouts without solid bounding-box artifacts.


* **Barycentric Bounding-Box Rasterizer with Depth-Buffering (Z-Buffer)**
* *Rationale:* Handles complex, overlapping cortical geometry natively. Accurate depth sorting ensures foreground gyri and sulci cleanly occlude deep hidden surfaces. The bounding-box processing strategy ensures predictable, cache-friendly CPU loops.


* **On-the-Fly Normal Vector Computation with Caching**
* *Rationale:* Standard surface file formats (e.g., Freesurfer `.white`, `.pial` or raw `.obj`/`.ply` matrices) frequently omit surface normals. The engine computes normals from winding order and caches them for reuse across multiple renderings of the same mesh, avoiding redundant computation.


* **Smooth (Vertex) and Flat (Face) Shading**
* *Rationale:* Smooth shading interpolates vertex normals for publication-quality visualization with continuous lighting. Flat shading uses face normals for mesh inspection and quality control, revealing triangle boundaries.


* **Configurable Backface Culling**
* *Rationale:* Cortical surface models represent closed, organic shapes. Approximately 50% of triangles face away from any given camera viewpoint. Backface culling is enabled by default for closed meshes but can be disabled for open surfaces or when viewing both sides.


* **Near-Plane Clipping**
* *Rationale:* Prevents rendering artifacts and numerical instability when triangles intersect the camera's near plane. Critical for stable rendering at all camera distances.


* **Optional Wireframe Rendering**
* *Rationale:* Useful for debugging, topology inspection, and quality-control visualizations. Can be rendered as an overlay or standalone mode.


* **Flexible Camera Projection Stack (Orthographic & Perspective)**
* *Rationale:* Orthographic projection is essential in clinical and scientific visualization to preserve spatial relationships and metric scales across standard view axes. Perspective projection is utilized for natural anatomical renderings.


* **Camera-Aligned Directional Light ("Headlight Shading")**
* *Rationale:* Raw vertex colors on an unlit 3D model destroy all structural depth perception, making a highly folded brain mesh appear flat. A fixed light source bound to the camera casting onto calculated normals creates shadows in the sulci and highlights on the gyri, immediately resolving 3D topology.


* **Stateless, Deterministic Rendering**
* *Rationale:* Each render depends only on the mesh, camera, lighting, and render options. No hidden state, making the renderer predictable, testable, and suitable for batch processing.


* **Geometric Primitives for Annotation**
* *Rationale:* Scientific visualization often requires marking specific points in space (e.g., landmarks, activation peaks), drawing arrows to highlight regions, or connecting points on a mesh. The engine provides procedural mesh generators for primitives (spheres, cubes, cones, cylinders) and line rendering, allowing users to annotate visualizations without loading external model files.


---

## 2. Strict Boundary Matrix: What Is Out of Scope

To prevent scope creep and ensure the project can be completed in short time (weeks, not months), the following features are **explicitly excluded** from the C++ layer.

| Feature / Capability | Status | Handling / Alternative |
| --- | --- | --- |
| **Interactive Event Loop** | **OUT** | No mouse inputs, windows, or frame refresh loops. Input → Output image. |
| **Complex Materials / Texturing** | **OUT** | No PBR shaders, glass, reflectivity, or UV texture maps. Vertex colors only. |
| **In-Engine Text Rendering** | **OUT** | No TrueType font handling or text rasterization loops. Handled downstream via R. |
| **In-Engine Colorbars** | **OUT** | No vector layout generators inside C++. Managed via R with `magick`. |
| **Far-Plane Clipping** | **OUT** | Unnecessary for single-object brain meshes. Set far plane to large value; z-buffer handles depth. |
| **Anti-Aliasing (MSAA/FXAA)** | **OPTIONAL** | Optional 2× supersampling: render at double resolution and downsample in R. Quadruples render time but remains within budget. |
| **Global Illumination / Shadows** | **OUT** | Avoids expensive raytracing or raymarching. Headlight shading is sufficient. |
| **Multithreading** | **DEFERRED** | Initial implementation is single-threaded for simplicity and correctness. Parallelization can be added later if needed. |

---

## 3. Architectural Separation of Concerns

The project utilizes a strict division of labor between high-performance C++ rendering operations and high-level R composition engines.

```
+--------------------------------------------------------+
|                      R INTERFACE                       |
|  - Ingests mesh3d objects (vertices, indices, colors) |
|  - Validates array structures and 1-based indexing     |
|  - Caches computed normals for reuse                   |
+---------------------------+----------------------------+
                            |
                            | (Rcpp Zero-Copy Arrays)
                            ▼
+--------------------------------------------------------+
|                  PURE C++ RENDER ENGINE                |
|  - Compute Camera Transformations                      |
|  - Near-Plane Clipping & Backface Culling              |
|  - Depth-Buffered Rasterization with Shading           |
|  - Return in-memory RGBA Image object                  |
+---------------------------+----------------------------+
                            |
                            | (In-Memory RGBA Buffer)
                            ▼
+--------------------------------------------------------+
|                 R IMAGE EXPORT & LAYOUT                |
|  - Export Image to PNG/TIFF via magick or png package  |
|  - Tile multiple view matrices (Lateral, Medial, etc.) |
|  - Compose colorbars and text labels via magick        |
+--------------------------------------------------------+
```

### Key Architectural Principles

1. **Renderer Independence:** The C++ core has no knowledge of R or Rcpp. It operates purely on native C++ data structures. The R package acts as a translation layer.

2. **In-Memory Image Abstraction:** The renderer returns an `Image` object (width, height, RGBA pixel buffer). File export is a separate utility, not part of the rendering algorithm.

3. **Stateless Rendering:** Each render call is independent, depending only on its inputs (mesh, camera, lighting, options). No persistent state between calls.

### Multi-View Tiling, Colorbars, and Annotation Execution

Instead of complicating the C++ core with layout mechanics, the package delegates publication assembly to R using `magick` for pixel-perfect compositing:

1. **Multi-Angle Tiling:** The C++ core returns in-memory RGBA images. R exports them to PNG and composes multi-panel grids (e.g., 2×2 or 1×4 orientations) using `magick::image_append()`.

2. **Colorbars:** Generated as deterministic raster images with fixed dimensions (e.g., 600×80px). Color strips are created directly from colormap functions, tick labels added via `magick::image_annotate()`. No X11 or graphics device required.

3. **Annotations:** Text labels and layout composition handled entirely in R with `magick`, providing predictable sizing and headless operation.

---

## 4. Rendering Pipeline

The rendering pipeline is explicit and follows standard graphics conventions:

```
Input Mesh (vertices, indices, colors)
    ↓
Model Transform (optional scaling/rotation)
    ↓
View Transform (LookAt matrix: Eye, Center, Up)
    ↓
Projection Transform (Orthographic or Perspective)
    ↓
Near-Plane Clipping (remove triangles intersecting camera)
    ↓
Backface Culling (discard rear-facing triangles, if enabled)
    ↓
Rasterization (bounding-box, barycentric interpolation)
    ↓
Z-Buffer Test (depth sorting)
    ↓
Shading (headlight directional light, smooth or flat)
    ↓
RGBA Image Output (in-memory buffer)
```

---

## 5. Development Layout & Testing Matrix

To guarantee maximum developer efficiency and minimize friction, the project will be maintained within a **single repository** using a dual-track isolated build system.

### Directory Mapping

* `R/`: Contains user-facing wrapper functions, mesh extractions, normal caching, and `magick`-based composition pipelines.
* `src/rcpp_bindings.cpp`: Acts strictly as the isolated translation boundary. Maps R data types to C++ containers. This file is the only file allowed to reference R hooks.
* `src/core/`: Dedicated, pure C++ source repository containing standard math utilities, camera matrices, rasterization algorithms, and the Image class. Completely agnostic to R. Uses GLM for graphics math.
* `tests/testthat/`: Holds high-level R interface unit tests confirming class validation, type checks, and user-facing error limits.
* `cpp_tests/`: Standalone, isolated C++ workspace containing a separate testing harness (e.g., `Catch2` or `doctest`) backed by a lightweight `CMakeLists.txt` profile.

### Proposed Layout

```shell
scimesh/
├── DESCRIPTION
├── NAMESPACE
├── .Rbuildignore
│
├── R/
│   ├── render_mesh.R         # R wrapper exposing user-facing functions
│   ├── image_export.R        # PNG/TIFF export utilities using magick
│   └── cbar_layout.R         # Colorbar generation and magick composition
│
├── src/                      # CRAN compiled code folder
│   ├── Makevars              # Compiler flags (-O3)
│   ├── Makevars.win
│   ├── rcpp_bindings.cpp     # Rcpp translation boundary (ONLY file with R hooks)
│   │
│   └── core/                 # Pure C++ Engine (100% independent of R)
│       ├── renderer.h        # Main rasterization loops & Z-buffer
│       ├── renderer.cpp      # Implementation
│       ├── camera.h          # LookAt matrix math
│       ├── camera.cpp        # Implementation
│       ├── image.h           # RGBA Image abstraction
│       ├── image.cpp         # Implementation
│       ├── math_utils.h      # Vector/Matrix utilities (uses GLM)
│       └── glm/              # GLM header-only library (vendored)
│           ├── vec3.hpp
│           ├── vec4.hpp
│           ├── mat4x4.hpp
│           └── gtc/matrix_transform.hpp
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

### Testing Strategy

1. **C++ Unit Tests:** Validate individual algorithms (normal computation, clipping, rasterization) in `cpp_tests/` using Catch2 or doctest.

2. **R Interface Tests:** Validate input validation, type checking, and error handling in `tests/testthat/`.

3. **Image Regression Tests:** Compare rendered images against known reference outputs to catch subtle rendering bugs. Store reference images in `tests/testthat/reference_images/`.

---

## 6. Coordinate System Alignment & Camera Mapping Philosophy

To ensure a seamless transition for users moving from stateful interactive rendering to headless file production, `scimesh` aligns its geometrical foundations with the established conventions of the R graphics and neuroimaging ecosystems while strictly shielding the C++ core from legacy architectural quirks.

### 6.1 Coordinate System Mirroring
The C++ core implements a traditional **Right-Handed Coordinate System** to natively match `rgl` and standard neuroimaging spatial specifications (e.g., RAS: Right-Anterior-Superior):
* **X-Axis:** Positive values extend to the right.
* **Y-Axis:** Positive values extend upward.
* **Z-Axis:** Positive values extend outward (toward the camera viewpoint).

By keeping this mathematical parity, vertex arrays extracted from `mesh3d` objects can be fed directly into the C++ vertex processor without requiring on-the-fly matrix inversions or axis swapping at the R interface boundary.

### 6.2 Decoupling Camera Logic (API vs. Implementation)
Interactive toolkits like `rgl` manage camera positions using an intrinsic, stateful combination of spherical coordinates (theta/phi angles), pan, and bounding-box zoom states optimized for mouse interactions. To maintain code readability and engine reusability, `scimesh` splits this responsibility:

* **The C++ Engine Layer:** Remains entirely textbook and stateless. It accepts only a standard, explicit mathematical **LookAt Matrix** parameterized by three pure spatial vectors: `Eye` (camera coordinates), `Center` (target focal coordinates), and `Up` (the camera's orientation vector).

* **The R Wrapper Layer:** Natively handles usability mapping. It exposes user-facing configuration functions that mimic the traditional `rgl::view3d(theta, phi, zoom)` interface. The R layer performs the trigonometric translation of these spherical angles into explicit XYZ coordinates and injects them down into the clean C++ LookAt stack.

### 6.3 Proportional Safety via Explicit Viewports
While `rgl` dynamically responds to desktop window modifications—often introducing aspect-ratio distortion if a window is dragged non-uniformly—`scimesh` operates under absolute dimensional constraints.

The image dimensions (`width` and `height` in pixels) must be declared explicitly at execution time. The projection matrix calculates the aspect ratio strictly as width/height, guaranteeing that structural mesh features and anatomical proportions remain perfectly locked and invariant across all rendering scales and output file resolutions.


## 7. Headless Colorbar Generation & Layout Composition Architecture

For scientific visualization, an accurate colorbar is as critical as the 3D geometry itself. `scimesh` decouples colorbar production from the 3D mesh engine and handles it entirely in R using `magick` for pixel-perfect compositing.

### 7.1 Colorbar Generation Strategy

Instead of relying on R's grid graphics (which require active display devices and produce unpredictable sizes), colorbars are generated as **deterministic raster images**:

1. **Color strip:** Generate directly as a raster matrix from the colormap function, write to PNG
2. **Tick labels:** Add using `magick::image_annotate()` with known font metrics
3. **Fixed dimensions:** Colorbar images have predictable sizes (e.g., 600×80px horizontal), eliminating layout guesswork

This approach requires no X11, no graphics device, and produces consistent output across platforms.

### 7.2 Layout Composition with Magick

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
5. **Normal caching** — compute normals once, reuse across multiple views

The 1.5-second per-view budget is achievable with a clean, well-optimized implementation.


## 9. Geometric Primitives for Annotation

Scientific visualization often requires annotating specific locations or relationships in 3D space. `scimesh` provides procedural mesh generators for common primitives, allowing users to mark points, draw arrows, or connect regions without loading external model files.

### 9.1 Supported Primitives

| Primitive | Use Case | Implementation |
|-----------|----------|----------------|
| **Sphere** | Mark landmarks, activation peaks | Icosphere subdivision (configurable detail level) |
| **Cube** | Mark points with flat-sided geometry | 8 vertices, 12 triangles |
| **Cone** | Arrow heads, directional indicators | Circular base with apex, configurable segments |
| **Cylinder** | Connect two points, shaft of arrows | Two circular caps connected by tube segments |
| **Line Segment** | Connect points on mesh, draw edges | Thin quad (billboarded or fixed screen-space width) |

### 9.2 Implementation Approach

**C++ Core:** Provides low-level mesh generation functions that return vertex/index arrays:
```cpp
Mesh generate_sphere(Vec3 center, float radius, int subdivisions);
Mesh generate_cube(Vec3 center, float size);
Mesh generate_cone(Vec3 base_center, Vec3 apex, float radius, int segments);
Mesh generate_cylinder(Vec3 start, Vec3 end, float radius, int segments);
```

**R Wrapper:** Exposes high-level, user-friendly functions:
```r
add_sphere(position, radius=1.0, color="red", subdivisions=2)
add_cube(position, size=1.0, color="blue")
add_arrow(start, end, color="green", shaft_radius=0.1, head_radius=0.3)
add_line(start, end, color="yellow", width=2.0)
```

### 9.3 Rendering Strategy

Primitives are rendered as regular triangle meshes, appended to the main scene before rendering:

1. **Mesh Generation:** Procedural functions create vertex positions, indices, and vertex colors
2. **Scene Assembly:** Primitive meshes are concatenated with the main mesh data
3. **Unified Rendering:** The standard rasterization pipeline processes all triangles together

**Line Rendering:** Lines are special cases — they can be rendered as:
- **Thin quads** (two triangles per line segment) with fixed screen-space width
- **Billboarded quads** that always face the camera for consistent visual thickness

### 9.4 Performance Considerations

- Small primitives (spheres with 2-3 subdivisions, cubes) add negligible overhead (~100-1000 triangles)
- Procedural generation is fast (~1ms for a sphere with 3 subdivisions)
- Primitives can be cached if reused across multiple renderings

### 9.5 Example Use Cases

```r
# Mark an activation peak on the brain
vis.subject.morph.native(subjects_dir, 'subject1', 'thickness', 'lh')
add_sphere(position=c(10, -30, 45), radius=2.0, color="red")

# Draw an arrow pointing to a region
add_arrow(start=c(0, 0, 0), end=c(10, -30, 45), color="blue")

# Connect two points on the cortical surface
add_line(start=c(5, -20, 40), end=c(15, -25, 50), color="yellow", width=2.0)
```


## 10. Rationale for Custom Engine Architecture vs. Existing Frameworks

A foundational question for any engineering project is why a custom renderer is necessary when mature open-source solutions exist. During the scoping phase, three prominent MIT-licensed candidates were thoroughly evaluated: `ssloy/tinyrenderer`, `elnormous/SoftwareRenderer`, and `trenki2/SoftwareRenderer`. While excellent in their respective domains, these engines carry heavy abstractions designed to emulate complex, stateful graphics APIs (like OpenGL or Vulkan) or real-time game loops, introducing unnecessary architectural bloat, complex dependency chains, and difficult memory management for data passing. Because `scimesh` strictly rejects real-time windowing, texturing, global illumination, and interactive event loops, the required functional footprint is exceptionally small (~300–400 lines of pure C++). Developing the engine from scratch ensures that data structures map perfectly and zero-copy to R matrices, guarantees absolute cross-platform compliance under CRAN's strict memory sanitizers (ASAN/UBSAN), and results in a radically maintainable codebase perfectly optimized for the highly specific niche of headless neuroimaging visualization.


## 11. Technology Choices

### C++ Math Library: GLM

Use GLM (OpenGL Mathematics) instead of reimplementing vector/matrix math. GLM is header-only, MIT-licensed, well-tested, and designed for graphics math (Vec3, Vec4, Mat4, transforms). It eliminates bugs in matrix operations and saves development time without adding runtime overhead.

### Image Compositing: Magick

Use the `magick` R package for image composition and colorbar generation. It provides pixel-perfect control, works headlessly, and produces deterministic output with known dimensions.
