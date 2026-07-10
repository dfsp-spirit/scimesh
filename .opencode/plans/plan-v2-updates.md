# Plan: Update PROJECT_PLAN_v2.md

This document outlines all changes needed to update the PROJECT_PLAN_v2.md file based on review feedback.

---

## Summary of Changes

### Critical Additions

1. **Multi-Mesh Scene Support** - Add to Core Features (Section 1)
   - Renderer accepts a list of meshes, not just one
   - Each mesh has vertices, indices, colors
   - Proper depth sorting across all meshes

2. **Camera Auto-Framing** - Add to R Layer responsibilities (Section 3)
   - Compute mesh bounding box
   - Auto-position camera to fit entire mesh in view
   - Critical for usability - without it, users get empty/clipped images
   - **R layer responsibility** - requires neuro-specific knowledge

3. **Standard Anatomical Views** - Add to R Layer (Section 3 or new section)
   - Pre-defined camera positions: left lateral, right lateral, dorsal, ventral, anterior, posterior
   - Classic 3/4 views for publication figures
   - R layer provides these as named presets
   - **R layer responsibility** - requires RAS coordinate knowledge from neuroimaging libraries (e.g., `freesurferformats`)
   - **Architectural boundary**: C++ renderer is domain-agnostic, R layer owns all neuro-specific logic

4. **Viewport/Frustum Clipping** - Add to Rendering Pipeline (Section 4)
   - Clamp triangle bounding boxes to framebuffer bounds
   - Implicit with bbox rasterization but should be explicit to avoid bugs

### Important Additions

5. **Depth Buffer Precision** - Add to Core Features or Pipeline
   - Specify 32-bit float depth buffer (not 16-bit integer)
   - Prevents z-fighting on fine sulcal folds with similar depths

6. **Background/Clear Color** - Add render option
   - Framebuffer clearable to any RGBA value
   - Default to transparent, but allow solid colors

7. **stb_image_write for C++ Tests** - Add to Development Layout (Section 5)
   - C++ test harness needs to visualize outputs
   - Utility to write test images to disk for debugging

### Minor Additions

8. **sRGB Handling** - Add decision note
   - Option A: Render in linear space, convert to sRGB for output (physically correct)
   - Option B: Work entirely in sRGB (simpler, acceptable for brain viz)
   - Recommend Option B for simplicity

9. **Scene Clear Color** - Related to background color
   - Framebuffer initialization with configurable clear color

### Removals/Changes

10. **Remove "400 lines" claim** (Section 10)
    - Outdated estimate
    - With primitives, clipping, shading modes, etc., codebase will be 600-1000 lines
    - Just remove the specific number

11. **Replace Line Rendering with Thin Cylinders** (Section 9)
    - Remove "line segment" from primitives table
    - Replace with note: "For connecting points, use thin cylinders via `generate_cylinder()`"
    - Simpler than implementing specialized line rasterizer

12. **Remove Magick, Replace with Pure R** (Section 7 and throughout)
    - Use `png` package for I/O (pure R, no system deps)
    - Use pure R array manipulation for image stitching (`rbind`/`cbind` on RGBA arrays)
    - Use `grid` + `png()` device for colorbars with text (headless-safe)
    - Eliminates ImageMagick system dependency entirely - better for HPC

13. **Update Technology Choices** (Section 11)
    - Remove Magick
    - Add `png` package
    - Note that `grid` is used for colorbar text (headless-safe with `png()` device)

---

## Detailed Edit Plan

### Edit 1: Add Multi-Mesh Scene Support to Section 1

**Location:** After "Stateless, Deterministic Rendering" bullet

**Add:**
```markdown
* **Multi-Mesh Scene Support**
* *Rationale:* Users need to render multiple meshes in a single scene (e.g., both hemispheres, white matter + pial surface, brain + overlay). The renderer accepts a list of meshes, each with its own vertices, indices, and colors, and renders them together with proper depth sorting.
```

### Edit 2: Update Architecture Diagram (Section 3)

**Location:** Update the R Interface box

**Change:**
```
|  - Ingests mesh3d objects (vertices, indices, colors) |
```
**To:**
```
|  - Ingests list of mesh3d objects (multi-mesh scenes) |
|  - Auto-frame camera from mesh bounding box           |
|  - Provide standard anatomical view presets           |
|  - Handle neuro-specific logic (RAS, hemisphere info) |
```

**Also add a new "Architectural Boundary" subsection after the diagram:**

```markdown
### Architectural Boundary: Domain-Agnostic Renderer vs. Neuro-Specific R Layer

**C++ Core (Domain-Agnostic):**
- Receives generic mesh data (vertices, indices, colors)
- Receives explicit camera parameters (LookAt vectors)
- Has no knowledge of neuroimaging conventions, coordinate systems, or anatomical terminology
- Can be reused for any 3D mesh visualization task

**R Layer (Neuro-Specific):**
- Knows the mesh is in RAS coordinates (Right-Anterior-Superior)
- Knows hemisphere information (left vs right) from mesh metadata or filename
- Computes anatomical view directions (lateral, medial, dorsal, etc.) from RAS orientation
- Provides neuro-specific functionality (standard views, coordinate transformations)
- Uses neuroimaging libraries like `freesurferformats` for file format knowledge

This separation ensures the C++ renderer remains reusable for non-neuroimaging applications while the R layer provides domain-specific convenience for neuroscientists.
```

### Edit 3: Add Camera Auto-Framing Section

**Location:** New subsection in Section 6 (Coordinate System)

**Add Section 6.4:**
```markdown
### 6.4 Camera Auto-Framing (R Layer Responsibility)

The R layer automatically computes the mesh bounding box and positions the camera to fit the entire scene in view. This ensures users never get empty or clipped images. The auto-framing algorithm:

1. Compute axis-aligned bounding box (AABB) of all meshes in the scene
2. Calculate the bounding sphere radius
3. Position camera at distance = radius / sin(fov/2) for perspective, or scale orthographic projection to fit
4. Set camera target to bounding box center

Users can override auto-framing by providing explicit camera parameters.

**Architectural Boundary:** The C++ renderer is domain-agnostic — it has no knowledge of neuroimaging conventions, coordinate systems, or anatomical orientations. It only receives final camera parameters (LookAt vectors). All neuro-specific logic, including auto-framing, is handled by the R layer.
```

### Edit 4: Add Standard Anatomical Views

**Location:** New subsection in Section 6 or Section 3

**Add Section 6.5:**
```markdown
### 6.5 Standard Anatomical Views (R Layer Responsibility)

**Critical Architectural Boundary:** The C++ renderer is completely domain-agnostic. It has no knowledge of neuroimaging conventions, coordinate systems, or anatomical terminology. It only receives explicit mathematical camera parameters (LookAt vectors: Eye, Center, Up). All neuro-specific logic is handled exclusively by the R layer.

**RAS Coordinate System:** Neuroimaging meshes (e.g., Freesurfer surfaces) use the RAS coordinate system:
- **R**ight: positive X-axis
- **A**nterior: positive Y-axis  
- **S**uperior: positive Z-axis

The R layer must know this orientation (provided by neuroimaging libraries like `freesurferformats`) to compute appropriate camera positions for anatomical views.

**R Layer Implementation:** The R layer provides pre-defined camera positions for common neuroimaging views:

- **Left lateral:** Camera at negative X, looking toward center (viewing left hemisphere outer surface)
- **Right lateral:** Camera at positive X, looking toward center (viewing right hemisphere outer surface)
- **Medial views:** Requires hemisphere-specific logic (camera direction depends on which hemisphere)
- **Dorsal:** Camera at positive Y, looking down (top view)
- **Ventral:** Camera at negative Y, looking up (bottom view)
- **Anterior:** Camera at positive Z, looking backward (front view)
- **Posterior:** Camera at negative Z, looking forward (back view)
- **3/4 views:** Oblique angles for publication figures (e.g., 45° lateral + 30° dorsal)

These are exposed as named presets (e.g., `view="left_lateral"`, `view="dorsal"`) that the R layer maps to specific LookAt parameters based on the mesh's RAS orientation and hemisphere information.

**Why R Layer Owns This:**
- Hemisphere identification (left vs right) comes from mesh metadata or filename
- RAS orientation knowledge comes from neuroimaging file formats (provided by `freesurferformats`)
- Anatomical terminology (lateral, medial, dorsal, etc.) is neuro-specific
- The C++ renderer should remain reusable for non-neuroimaging meshes
```

### Edit 5: Update Rendering Pipeline (Section 4)

**Location:** After "Near-Plane Clipping" step

**Add:**
```markdown
    ↓
Viewport Clipping (clamp triangle bbox to framebuffer bounds)
    ↓
```

### Edit 6: Add Depth Buffer Precision Note

**Location:** After Section 4 (Rendering Pipeline) or in Section 1

**Add:**
```markdown
### Depth Buffer Precision

The renderer uses a 32-bit floating-point depth buffer to prevent z-fighting on fine sulcal folds where triangles have very similar depths. This is critical for accurate depth sorting of cortical surface geometry.
```

### Edit 7: Update Primitives Section (Section 9)

**Location:** Section 9.1 - Supported Primitives table

**Remove:**
```
| **Line Segment** | Connect points on mesh, draw edges | Thin quad (billboarded or fixed screen-space width) |
```

**Add note after table:**
```markdown
**Note:** For connecting points or drawing lines between locations, use thin cylinders via `generate_cylinder()`. This avoids the complexity of specialized line rasterization while providing consistent visual results.
```

**Location:** Section 9.3 - Rendering Strategy

**Remove:**
```markdown
**Line Rendering:** Lines are special cases — they can be rendered as:
- **Thin quads** (two triangles per line segment) with fixed screen-space width
- **Billboarded quads** that always face the camera for consistent visual thickness
```

### Edit 8: Replace Magick with Pure R (Section 7)

**Location:** Entire Section 7 - replace with new content

**Replace Section 7 with:**

```markdown
## 7. Headless Colorbar Generation & Layout Composition Architecture

For scientific visualization, an accurate colorbar is as critical as the 3D geometry itself. `scimesh` decouples colorbar production from the 3D mesh engine and handles it entirely in R using pure R operations with the `png` package, requiring no system dependencies.

### 7.1 Image Stitching with Pure R

Multi-panel figures are assembled using standard R array operations:

```r
# Read rendered brain images as RGBA arrays
lateral <- png::readPNG("lateral.png")
medial <- png::readPNG("medial.png")

# Stitch side-by-side (assuming same height)
brain_views <- abind(lateral, medial, along=2)  # or manual array concatenation

# Stack with colorbar below
final <- abind(brain_views, colorbar, along=1)

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
```

### Edit 9: Remove "400 lines" Claim (Section 10)

**Location:** Section 10 - Rationale for Custom Engine

**Find:**
```
the required functional footprint is exceptionally small (~300–400 lines of pure C++).
```

**Replace with:**
```
the required functional footprint is small and manageable.
```

### Edit 10: Update Technology Choices (Section 11)

**Location:** Section 11 - Technology Choices

**Replace entire section with:**

```markdown
## 11. Technology Choices

### C++ Math Library: GLM

Use GLM (OpenGL Mathematics) instead of reimplementing vector/matrix math. GLM is header-only, MIT-licensed, well-tested, and designed for graphics math (Vec3, Vec4, Mat4, transforms). It eliminates bugs in matrix operations and saves development time without adding runtime overhead.

### R Image I/O: png Package

Use the `png` package for reading and writing PNG images. It is pure R with no system dependencies, works headlessly on any platform including HPC clusters, and provides direct access to RGBA pixel arrays.

### R Colorbar Text: grid Package

Use `grid` graphics with the `png()` device for rendering colorbar text labels. The `png()` device is headless-safe and does not require X11 or any display server.

### Image Stitching: Pure R Arrays

Image composition (stitching multiple views, adding colorbars) is performed using pure R array operations (`rbind`, `cbind`, or the `abind` package). This eliminates the need for ImageMagick or other system-level image processing libraries.
```

### Edit 11: Add Background Color to Core Features

**Location:** Section 1 - Core Features

**Add:**
```markdown
* **Configurable Background Color**
* *Rationale:* The framebuffer is clearable to any RGBA value before rendering begins. This allows transparent backgrounds (default) for compositing, or solid colors (white, black) for standalone figures.
```

### Edit 12: Add stb_image_write to C++ Tests

**Location:** Section 5 - Development Layout

**Add to Directory Mapping:**
```markdown
* `cpp_tests/`: Standalone, isolated C++ workspace containing a separate testing harness (e.g., `Catch2` or `doctest`) backed by a lightweight `CMakeLists.txt` profile. Includes `stb_image_write.h` for writing test output images to disk for visual debugging.
```

**Add to Proposed Layout:**
```shell
└── cpp_tests/                # Standalone C++ Testing Workspace
    ├── CMakeLists.txt        # Build profile for independent C++ compiling
    ├── main_test.cpp         # Test runner (Catch2 / doctest)
    ├── test_rasterizer.cpp   # Unit tests for the core/ algorithms
    └── stb_image_write.h     # For writing test images to disk
```

### Edit 13: Add sRGB Handling Note

**Location:** New subsection in Section 1 or Section 4

**Add:**
```markdown
### Colorspace Handling

The renderer works entirely in sRGB colorspace for simplicity. While physically correct rendering would require linear-space lighting calculations followed by sRGB conversion, the visual difference is negligible for brain visualization with vertex colors. This simplification avoids the complexity of colorspace conversions in the shading pipeline.
```

---

## Execution Order

1. Edit Section 1: Add Multi-Mesh Scene Support, Background Color
2. Edit Section 3: Update architecture diagram with auto-framing and anatomical views
3. Edit Section 4: Add viewport clipping step, add depth buffer precision note
4. Edit Section 5: Add stb_image_write to cpp_tests
5. Edit Section 6: Add subsections 6.4 (auto-framing) and 6.5 (anatomical views)
6. Edit Section 7: Replace entire section with pure R approach
7. Edit Section 9: Remove line segment, add cylinder note
8. Edit Section 10: Remove "400 lines" claim
9. Edit Section 11: Replace with updated technology choices
10. Add sRGB note (where appropriate)

---

## Verification Checklist

After all edits, verify:
- [ ] Multi-mesh support mentioned in core features
- [ ] Camera auto-framing documented as R layer responsibility
- [ ] Standard anatomical views listed with R layer ownership
- [ ] RAS coordinate system explicitly mentioned
- [ ] freesurferformats mentioned as source of RAS info
- [ ] Architectural boundary clearly stated: C++ is domain-agnostic, R is neuro-specific
- [ ] Viewport clipping in pipeline
- [ ] Depth buffer precision specified
- [ ] Background color option mentioned
- [ ] stb_image_write in cpp_tests
- [ ] sRGB handling decision documented
- [ ] Line rendering removed or replaced with cylinders
- [ ] Magick completely removed
- [ ] Pure R approach for image stitching documented
- [ ] "400 lines" claim removed
- [ ] Technology choices updated (png, grid, pure R arrays)
