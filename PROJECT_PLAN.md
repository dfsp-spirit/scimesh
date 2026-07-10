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

### Dual-Track Development Workflow

* **Track A (C++ Engine Cycle):** Optimization of raster loops, vertex transformation corrections, or depth-buffer updates are developed and compiled entirely inside `cpp_tests/`. Compilation and execution require milliseconds, completely freeing the developer from the overhead of re-installing the R extension layer during foundational graphics development.
* **Track B (R Integration Cycle):** Once the C++ engine passes its isolated functional test constraints, the R package wrapper interfaces are configured and validated using standard package test suites.