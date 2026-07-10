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


* **Multi-Mesh Scene Support**
* *Rationale:* Users need to render multiple meshes in a single scene (e.g., both hemispheres, white matter + pial surface, brain + overlay). The renderer accepts a list of meshes, each with its own vertices, indices, and colors, and renders them together with proper depth sorting.


* **Configurable Background Color**
* *Rationale:* The framebuffer is clearable to any RGBA value before rendering begins. This allows transparent backgrounds (default) for compositing, or solid colors (white, black) for standalone figures.


* **Geometric Primitives for Annotation**
* *Rationale:* Scientific visualization often requires marking specific points in space (e.g., landmarks, activation peaks), drawing arrows to highlight regions, or connecting points on a mesh. The engine provides procedural mesh generators for primitives (spheres, cubes, cones, cylinders), allowing users to annotate visualizations without loading external model files.


---

## 2. Strict Boundary Matrix: What Is Out of Scope

To prevent scope creep and ensure the project can be completed in short time (weeks, not months), the following features are **explicitly excluded** from the C++ layer.

| Feature / Capability | Status | Handling / Alternative |
| --- | --- | --- |
| **Interactive Event Loop** | **OUT** | No mouse inputs, windows, or frame refresh loops. Input → Output image. |
| **Complex Materials / Texturing** | **OUT** | No PBR shaders, glass, reflectivity, or UV texture maps. Vertex colors only. |
| **In-Engine Text Rendering** | **OUT** | No TrueType font handling or text rasterization loops. Handled downstream via R. |
| **In-Engine Colorbars** | **OUT** | No vector layout generators inside C++. Managed via R with pure array operations. |
| **Far-Plane Clipping** | **OUT** | Unnecessary for single-object brain meshes. Set far plane to large value; z-buffer handles depth. |
| **Anti-Aliasing (MSAA/FXAA)** | **OPTIONAL** | Optional 2× supersampling: render at double resolution and downsample in R. Quadruples render time but remains within budget. |
| **Global Illumination / Shadows** | **OUT** | Avoids expensive raytracing or raymarching. Headlight shading is sufficient. |
| **Multithreading** | **DEFERRED** | Initial implementation is single-threaded for simplicity and correctness. Parallelization can be added later if needed. |

---

## 3. Architectural Separation of Concerns

The project utilizes a strict division of labor between high-performance C++ rendering operations and high-level R composition engines. The R interface follows a three-layer design to maintain domain-agnosticism at the core while providing neuro-specific convenience.

```
+--------------------------------------------------------+
|  LAYER 3: Publication Figure Assembly (Neuro-Specific) |
|  - vis.subject.morph.native(...)                       |
|  - Combines multiple views + colorbars + labels        |
|  - Full fsbrain-style workflow                         |
+---------------------------+----------------------------+
                            |
                            ▼
+--------------------------------------------------------+
|  LAYER 2: Neuro-Specific Convenience (Neuro-Specific)  |
|  - vis.brain.lateral(subject, hemi, data, ...)         |
|  - vis.brain.dorsal(...), vis.brain.medial(...)        |
|  - Knows RAS, hemispheres, Freesurfer conventions      |
|  - Provides colorbar + multi-view composition          |
+---------------------------+----------------------------+
                            |
                            ▼
+--------------------------------------------------------+
|  LAYER 1: Generic Renderer Interface (Domain-Agnostic) |
|  - render_mesh(mesh, camera, ...)                      |
|  - render_scene(mesh_list, camera, ...)                |
|  - Camera: LookAt vectors or generic theta/phi/zoom    |
|  - No knowledge of brain, hemisphere, RAS              |
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
|  - Export Image to PNG/TIFF via png package            |
|  - Tile multiple view matrices (Lateral, Medial, etc.) |
|  - Compose colorbars and text labels via pure R arrays |
+--------------------------------------------------------+
```

### Architectural Boundary: Domain-Agnostic Core vs. Neuro-Specific Convenience

**Layer 1 - Generic Renderer Interface (Domain-Agnostic):**
- `render_mesh(mesh, camera, ...)` - works for any mesh3d object
- `render_scene(mesh_list, camera, ...)` - multi-mesh scenes
- Camera specified as explicit LookAt vectors or generic theta/phi/zoom
- No knowledge of "brain", "hemisphere", "RAS", or anatomical views
- Can be used for any 3D mesh visualization task

**Layer 2 - Neuro-Specific Convenience Functions:**
- `vis.brain.lateral(subject, hemisphere, data, ...)` - wraps Layer 1
- `vis.brain.dorsal(...)`, `vis.brain.medial(...)`, etc.
- These know about RAS coordinates, hemispheres, Freesurfer conventions
- Provide colorbar + multi-view composition
- Use neuroimaging libraries like `freesurferformats` for file format knowledge

**Layer 3 - Publication Figure Assembly:**
- `vis.subject.morph.native(...)` - full fsbrain-style workflow
- Combines multiple views + colorbars + labels
- High-level functions for common neuroimaging publication needs

**C++ Core (Domain-Agnostic):**
- Receives generic mesh data (vertices, indices, colors)
- Receives explicit camera parameters (LookAt vectors: Eye, Center, Up)
- Has no knowledge of neuroimaging conventions, coordinate systems, or anatomical terminology
- Can be reused for any 3D mesh visualization task

This three-layer design ensures the core renderer interface remains reusable for non-neuroimaging applications while providing domain-specific convenience for neuroscientists through higher-level wrappers.

### Key Architectural Principles

1. **Renderer Independence:** The C++ core has no knowledge of R or Rcpp. It operates purely on native C++ data structures. The R package acts as a translation layer.

2. **In-Memory Image Abstraction:** The renderer returns an `Image` object (width, height, RGBA pixel buffer). File export is a separate utility, not part of the rendering algorithm.

3. **Stateless Rendering:** Each render call is independent, depending only on its inputs (mesh, camera, lighting, options). No persistent state between calls.

4. **Three-Layer R API Design:** The R interface follows a three-layer design:
   - **Layer 1 (Generic):** Domain-agnostic renderer interface (`render_mesh`, `render_scene`)
   - **Layer 2 (Neuro Convenience):** Neuro-specific view functions (`vis.brain.lateral`, etc.)
   - **Layer 3 (Publication):** Full publication figure assembly (`vis.subject.morph.native`)
   
   This ensures the core renderer interface remains reusable for non-neuroimaging applications while providing domain-specific convenience for neuroscientists.

### Multi-View Tiling, Colorbars, and Annotation Execution (Layer 3)

These are Layer 3 (publication figure assembly) features. Instead of complicating the C++ core with layout mechanics, the package delegates publication assembly to R using pure R array operations for pixel-perfect compositing:

1. **Multi-Angle Tiling:** The C++ core returns in-memory RGBA images. Layer 3 functions export them to PNG and compose multi-panel grids (e.g., 2×2 or 1×4 orientations) using pure R array concatenation (`rbind`, `cbind`, or the `abind` package).

2. **Colorbars:** Generated as deterministic raster images with fixed dimensions (e.g., 600×80px). Color strips are created directly from colormap functions, tick labels added via `grid::textGrob()` with the headless-safe `png()` device. No X11 or graphics device required.

3. **Annotations:** Text labels and layout composition handled entirely in R with pure array operations, providing predictable sizing and headless operation.

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
Viewport Clipping (clamp triangle bbox to framebuffer bounds)
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

### Depth Buffer Precision

The renderer uses a 32-bit floating-point depth buffer to prevent z-fighting on fine sulcal folds where triangles have very similar depths. This is critical for accurate depth sorting of cortical surface geometry.

### Colorspace Handling

The renderer works entirely in sRGB colorspace for simplicity. While physically correct rendering would require linear-space lighting calculations followed by sRGB conversion, the visual difference is negligible for brain visualization with vertex colors. This simplification avoids the complexity of colorspace conversions in the shading pipeline.

---

## 5. Development Layout & Testing Matrix

To guarantee maximum developer efficiency and minimize friction, the project will be maintained within a **single repository** using a dual-track isolated build system.

### Directory Mapping

* `R/`: Contains three layers of R functions:
  - **Layer 1 (Generic):** `render_mesh.R`, `render_scene.R` - domain-agnostic renderer interface
  - **Layer 2 (Neuro Convenience):** `vis_brain.R` - neuro-specific view functions (lateral, medial, dorsal, etc.)
  - **Layer 3 (Publication):** `vis_subject.R` - full publication figure assembly
  - **Utilities:** `image_export.R` (PNG/TIFF export), `cbar_layout.R` (colorbar generation), `camera.R` (auto-framing, LookAt math)
* `src/rcpp_bindings.cpp`: Acts strictly as the isolated translation boundary. Maps R data types to C++ containers. This file is the only file allowed to reference R hooks.
* `src/core/`: Dedicated, pure C++ source repository containing standard math utilities, camera matrices, rasterization algorithms, and the Image class. Completely agnostic to R. Uses GLM for graphics math.
* `tests/testthat/`: Holds high-level R interface unit tests confirming class validation, type checks, and user-facing error limits.
* `cpp_tests/`: Standalone, isolated C++ workspace containing a separate testing harness (e.g., `Catch2` or `doctest`) backed by a lightweight `CMakeLists.txt` profile. Includes `stb_image_write.h` for writing test output images to disk for visual debugging.

### Proposed Layout

```shell
scimesh/
├── DESCRIPTION
├── NAMESPACE
├── .Rbuildignore
│
├── R/
│   ├── render_mesh.R         # Layer 1: Generic mesh rendering (domain-agnostic)
│   ├── render_scene.R        # Layer 1: Multi-mesh scene rendering (domain-agnostic)
│   ├── camera.R              # Layer 1: Auto-framing, LookAt math (domain-agnostic)
│   ├── vis_brain.R           # Layer 2: Neuro-specific view functions (lateral, medial, etc.)
│   ├── vis_subject.R         # Layer 3: Publication figure assembly
│   ├── image_export.R        # Utility: PNG/TIFF export using png package
│   └── cbar_layout.R         # Utility: Colorbar generation and pure R composition
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
    ├── test_rasterizer.cpp   # Unit tests for the core/ algorithms
    └── stb_image_write.h     # For writing test images to disk
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

* **Layer 1 (Generic Renderer Interface):** Natively handles usability mapping. It exposes user-facing configuration functions that mimic the traditional `rgl::view3d(theta, phi, zoom)` interface. This layer performs the trigonometric translation of these spherical angles into explicit XYZ coordinates and injects them down into the clean C++ LookAt stack.

### 6.3 Proportional Safety via Explicit Viewports
While `rgl` dynamically responds to desktop window modifications—often introducing aspect-ratio distortion if a window is dragged non-uniformly—`scimesh` operates under absolute dimensional constraints.

The image dimensions (`width` and `height` in pixels) must be declared explicitly at execution time. The projection matrix calculates the aspect ratio strictly as width/height, guaranteeing that structural mesh features and anatomical proportions remain perfectly locked and invariant across all rendering scales and output file resolutions.

### 6.4 Camera Auto-Framing (Layer 1: Generic Feature)

Layer 1 (Generic Renderer Interface) automatically computes the mesh bounding box and positions the camera to fit the entire scene in view. This ensures users never get empty or clipped images. This is a domain-agnostic feature that works for any mesh, not just brain meshes.

The auto-framing algorithm:

1. Compute axis-aligned bounding box (AABB) of all meshes in the scene
2. Calculate the bounding sphere radius
3. Position camera at distance = radius / sin(fov/2) for perspective, or scale orthographic projection to fit
4. Set camera target to bounding box center

Users can override auto-framing by providing explicit camera parameters.

**Architectural Boundary:** The C++ renderer is domain-agnostic — it has no knowledge of neuroimaging conventions, coordinate systems, or anatomical orientations. It only receives final camera parameters (LookAt vectors). Auto-framing is a generic feature in Layer 1 that works for any mesh.

### 6.5 Standard Anatomical Views (Layer 2: Neuro-Specific Convenience)

**Critical Architectural Boundary:** The C++ renderer is completely domain-agnostic. It has no knowledge of neuroimaging conventions, coordinate systems, or anatomical terminology. It only receives explicit mathematical camera parameters (LookAt vectors: Eye, Center, Up). Standard anatomical views are provided as Layer 2 convenience functions, not part of the core renderer interface.

**RAS Coordinate System:** Neuroimaging meshes (e.g., Freesurfer surfaces) use the RAS coordinate system:
- **R**ight: positive X-axis
- **A**nterior: positive Y-axis
- **S**uperior: positive Z-axis

The Layer 2 convenience functions must know this orientation (provided by neuroimaging libraries like `freesurferformats`) to compute appropriate camera positions for anatomical views.

**Layer 2 Implementation:** The neuro-specific convenience layer provides pre-defined camera positions for common neuroimaging views:

- **Left lateral:** Camera at negative X, looking toward center (viewing left hemisphere outer surface)
- **Right lateral:** Camera at positive X, looking toward center (viewing right hemisphere outer surface)
- **Medial views:** Requires hemisphere-specific logic (camera direction depends on which hemisphere)
- **Dorsal:** Camera at positive Y, looking down (top view)
- **Ventral:** Camera at negative Y, looking up (bottom view)
- **Anterior:** Camera at positive Z, looking backward (front view)
- **Posterior:** Camera at negative Z, looking forward (back view)
- **3/4 views:** Oblique angles for publication figures (e.g., 45° lateral + 30° dorsal)

These are exposed as named presets (e.g., `view="left_lateral"`, `view="dorsal"`) that the Layer 2 functions map to specific LookAt parameters based on the mesh's RAS orientation and hemisphere information.

**Why Layer 2 Owns This:**
- Hemisphere identification (left vs right) comes from mesh metadata or filename
- RAS orientation knowledge comes from neuroimaging file formats (provided by `freesurferformats`)
- Anatomical terminology (lateral, medial, dorsal, etc.) is neuro-specific
- The core renderer interface (Layer 1) should remain reusable for non-neuroimaging meshes


## 7. Headless Colorbar Generation & Layout Composition Architecture

For scientific visualization, an accurate colorbar is as critical as the 3D geometry itself. `scimesh` decouples colorbar production from the 3D mesh engine and handles it entirely in R using pure R operations with the `png` package, requiring no system dependencies.

### 7.1 Image Stitching with Pure R

Multi-panel figures are assembled using standard R array operations:

```r
# Read rendered brain images as RGBA arrays
lateral <- png::readPNG("lateral.png")
medial <- png::readPNG("medial.png")

# Stitch side-by-side (assuming same height)
brain_views <- abind::abind(lateral, medial, along=2)  # or manual array concatenation

# Stack with colorbar below
final <- abind::abind(brain_views, colorbar, along=1)

# Write output
png::writePNG(final, "figure.png")
```

**Advantages:**
- Zero system dependencies (pure R)
- Works on any HPC system without ImageMagick
- Predictable sizing — dimensions are known from input arrays
- Fast array operations

### 7.2 Colorbar Generation Strategy

Colorbars are generated as deterministic raster images using base R and the `png` package:

1. **Color strip:** Create a raster matrix from the colormap function
2. **Tick labels:** Use `grid::textGrob()` with `png()` device (headless-safe)
3. **Fixed dimensions:** Colorbar images have predictable sizes (e.g., 600×80px horizontal)

This approach requires no X11, no graphics device, and produces consistent output across platforms.

### 7.3 Layout Composition

The complete pipeline:

1. C++ renderer returns in-memory RGBA buffers
2. R exports each buffer to PNG using `png::writePNG()`
3. R reads PNGs back as arrays using `png::readPNG()`
4. R concatenates arrays for multi-panel layout
5. R generates colorbar as array using `grid` + `png()` device
6. R concatenates colorbar array with brain view arrays
7. R writes final composite using `png::writePNG()`

All operations are headless-safe and require no system dependencies beyond the `png` package.


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

**Note:** For connecting points or drawing lines between locations, use thin cylinders via `generate_cylinder()`. This avoids the complexity of specialized line rasterization while providing consistent visual results.

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
```

### 9.3 Rendering Strategy

Primitives are rendered as regular triangle meshes, appended to the main scene before rendering:

1. **Mesh Generation:** Procedural functions create vertex positions, indices, and vertex colors
2. **Scene Assembly:** Primitive meshes are concatenated with the main mesh data
3. **Unified Rendering:** The standard rasterization pipeline processes all triangles together


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

# Connect two points on the cortical surface using a thin cylinder
add_cylinder(start=c(5, -20, 40), end=c(15, -25, 50), radius=0.5, color="yellow")
```


## 10. Rationale for Custom Engine Architecture vs. Existing Frameworks

A foundational question for any engineering project is why a custom renderer is necessary when mature open-source solutions exist. During the scoping phase, three prominent MIT-licensed candidates were thoroughly evaluated: `ssloy/tinyrenderer`, `elnormous/SoftwareRenderer`, and `trenki2/SoftwareRenderer`. While excellent in their respective domains, these engines carry heavy abstractions designed to emulate complex, stateful graphics APIs (like OpenGL or Vulkan) or real-time game loops, introducing unnecessary architectural bloat, complex dependency chains, and difficult memory management for data passing. Because `scimesh` strictly rejects real-time windowing, texturing, global illumination, and interactive event loops, the required functional footprint is small and manageable. Developing the engine from scratch ensures that data structures map perfectly and zero-copy to R matrices, guarantees absolute cross-platform compliance under CRAN's strict memory sanitizers (ASAN/UBSAN), and results in a radically maintainable codebase perfectly optimized for the highly specific niche of headless neuroimaging visualization.


## 11. Technology Choices

### C++ Math Library: GLM

Use GLM (OpenGL Mathematics) instead of reimplementing vector/matrix math. GLM is header-only, MIT-licensed, well-tested, and designed for graphics math (Vec3, Vec4, Mat4, transforms). It eliminates bugs in matrix operations and saves development time without adding runtime overhead.

### R Image I/O: png Package

Use the `png` package for reading and writing PNG images. It is pure R with no system dependencies, works headlessly on any platform including HPC clusters, and provides direct access to RGBA pixel arrays.

### R Colorbar Text: grid Package

Use `grid` graphics with the `png()` device for rendering colorbar text labels. The `png()` device is headless-safe and does not require X11 or any display server.

### Image Stitching: Pure R Arrays

Image composition (stitching multiple views, adding colorbars) is performed using pure R array operations (`rbind`, `cbind`, or the `abind` package). This eliminates the need for ImageMagick or other system-level image processing libraries.
