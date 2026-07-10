# scimesh Implementation Plan

This document outlines the step-by-step implementation strategy for scimesh, starting with the C++ renderer core.

---

## Phase 1: Project Infrastructure Setup

### 1.1 Create R Package Skeleton

**Goal:** Establish the R package structure with proper DESCRIPTION, NAMESPACE, and build configuration.

**Files to create:**
```
scimesh/
├── DESCRIPTION          # Package metadata, dependencies (Rcpp, png)
├── NAMESPACE            # Export declarations
├── .Rbuildignore        # Files to exclude from R package build
├── .gitignore           # Already exists, extend for R artifacts
├── R/                   # Empty for now, will add Layer 1-3 functions later
├── src/
│   ├── Makevars         # Compiler flags: PKG_CXXFLAGS = -O3 -std=c++17
│   └── Makevars.win     # Windows-specific flags
└── tests/
    ├── testthat.R       # Test runner
    └── testthat/        # Empty for now
```

**Key decisions:**
- Use Rcpp for C++/R interface
- Declare dependencies: Rcpp, png (for image I/O)
- Set C++17 standard explicitly

### 1.2 Set Up C++ Testing Infrastructure (cpp_tests/)

**Goal:** Create standalone C++ test harness for rapid development without R compilation overhead.

**Directory structure:**
```
cpp_tests/
├── CMakeLists.txt       # Build configuration
├── main.cpp             # Test runner entry point
├── test_math.cpp        # Vector/matrix tests
├── test_camera.cpp      # Camera transform tests
├── test_clipping.cpp    # Near-plane clipping tests
├── test_rasterizer.cpp  # Rasterization tests
└── test_integration.cpp # End-to-end render tests
```

**CMakeLists.txt requirements:**
- Include path to src/core/ for renderer headers
- Include path to src/core/glm/ for GLM headers
- Link stb_image_write for PNG output in tests
- Use a simple test framework (doctest or Catch2, single-header)
- Build type: Release with -O3 optimization

**Test workflow:**
1. Write C++ code in src/core/
2. Write tests in cpp_tests/
3. Build and run tests: `cd cpp_tests && cmake . && make && ./tests`
4. Iterate rapidly without R package installation

### 1.3 Create Core Header Files (src/core/)

**Goal:** Define the core abstractions as specified in the project plan.

**Files to create:**
```
src/core/
├── types.h              # Core type definitions (Vec3, Vec4, Mat4, Color)
├── mesh.h               # Mesh struct definition
├── scene.h              # Scene struct (list of meshes)
├── camera.h             # Camera struct and LookAt matrix computation
├── render_options.h     # RenderOptions struct
├── image.h              # Image struct (RGBA buffer)
├── renderer.h           # Main renderer interface
├── math_utils.h         # Vector/matrix utilities using GLM
├── clipping.h           # Near-plane clipping (Sutherland-Hodgman)
├── rasterizer.h         # Triangle rasterization with Z-buffer
└── normals.h            # Normal computation utilities
```

**Implementation files (.cpp):**
```
src/core/
├── camera.cpp           # LookAt matrix implementation
├── renderer.cpp         # Main rendering loop
├── clipping.cpp         # Clipping algorithms
├── rasterizer.cpp       # Rasterization implementation
├── normals.cpp          # Normal computation
└── image.cpp            # Image buffer management
```

---

## Phase 2: Core Data Structures

### 2.1 Implement Basic Types (types.h)

**Goal:** Define fundamental types using GLM.

**Implementation:**
```cpp
// types.h
#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <cstdint>

namespace scimesh {
    using Vec3 = glm::vec3;
    using Vec4 = glm::vec4;
    using Mat4 = glm::mat4;
    
    struct Color {
        float r, g, b, a;
    };
    
    // Vertex with position, color, and optional normal
    struct Vertex {
        Vec3 position;
        Color color;
        Vec3 normal;  // May be computed later
        bool has_normal;
    };
    
    // Triangle defined by three vertex indices
    struct Triangle {
        uint32_t v0, v1, v2;
    };
}
```

**Tests (test_math.cpp):**
- Verify Vec3/Vec4 operations
- Verify Color struct initialization
- Test Vertex construction with/without normals

### 2.2 Implement Mesh Structure (mesh.h)

**Goal:** Define the Mesh abstraction as specified in the project plan.

**Implementation:**
```cpp
// mesh.h
#pragma once
#include "types.h"

namespace scimesh {
    struct Mesh {
        std::vector<Vec3> vertices;           // Nx3 positions
        std::vector<Triangle> triangles;      // Mx3 indices
        std::vector<Color> colors;            // Nx4 RGBA (optional)
        std::vector<Vec3> normals;            // Nx3 normals (optional)
        Color default_color = {0.7f, 0.7f, 0.7f, 1.0f};  // Light gray
        
        bool has_colors() const { return !colors.empty(); }
        bool has_normals() const { return !normals.empty(); }
        
        // Validation
        bool is_valid() const;  // Check indices in range, no NaN, etc.
    };
}
```

**Tests (test_mesh.cpp):**
- Create simple meshes (cube, tetrahedron)
- Test validation logic
- Test default color fallback

### 2.3 Implement Scene Structure (scene.h)

**Goal:** Define Scene as a collection of meshes.

**Implementation:**
```cpp
// scene.h
#pragma once
#include "mesh.h"

namespace scimesh {
    struct Scene {
        std::vector<Mesh> meshes;
        
        // Compute bounding box of all meshes
        void compute_bounding_box(Vec3& min_bound, Vec3& max_bound) const;
    };
}
```

### 2.4 Implement Camera Structure (camera.h/cpp)

**Goal:** Implement camera with LookAt matrix computation.

**Implementation:**
```cpp
// camera.h
#pragma once
#include "types.h"

namespace scimesh {
    enum class ProjectionType {
        ORTHOGRAPHIC,
        PERSPECTIVE
    };
    
    struct Camera {
        Vec3 eye;
        Vec3 center;
        Vec3 up;
        ProjectionType projection;
        float fov_degrees;  // For perspective only
        
        // Compute view matrix (LookAt)
        Mat4 get_view_matrix() const;
        
        // Compute projection matrix
        Mat4 get_projection_matrix(float aspect_ratio, 
                                   float near_plane, 
                                   float far_plane) const;
    };
}
```

**Implementation details:**
- Use GLM's `glm::lookAt()` for view matrix
- Use GLM's `glm::perspective()` and `glm::ortho()` for projection
- Right-handed coordinate system (matches RAS)

**Tests (test_camera.cpp):**
- Verify LookAt matrix produces correct view transform
- Test perspective vs orthographic projection
- Verify aspect ratio handling

### 2.5 Implement RenderOptions (render_options.h)

**Goal:** Define rendering configuration.

**Implementation:**
```cpp
// render_options.h
#pragma once
#include "types.h"

namespace scimesh {
    enum class ShadingMode {
        SMOOTH,  // Vertex normal interpolation
        FLAT     // Face normals
    };
    
    struct RenderOptions {
        int width = 800;
        int height = 600;
        ShadingMode shading = ShadingMode::SMOOTH;
        bool backface_culling = true;
        Color background_color = {0.0f, 0.0f, 0.0f, 0.0f};  // Transparent
        Color default_color = {0.7f, 0.7f, 0.7f, 1.0f};     // Light gray
        bool invert_normals = false;
        
        // Derived: near/far planes (auto-computed from scene bounds)
        float near_plane = 0.1f;
        float far_plane = 1000.0f;
    };
}
```

### 2.6 Implement Image Structure (image.h/cpp)

**Goal:** Define RGBA image buffer.

**Implementation:**
```cpp
// image.h
#pragma once
#include <vector>
#include <cstdint>

namespace scimesh {
    struct Image {
        int width;
        int height;
        std::vector<uint8_t> pixels;  // RGBA, row-major, 4 bytes per pixel
        
        Image(int w, int h);
        
        // Pixel access
        void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void get_pixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) const;
        
        // Clear to background color
        void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        
        // Write to PNG (using stb_image_write)
        bool write_png(const std::string& filename) const;
    };
}
```

**Implementation details:**
- Use stb_image_write for PNG output
- Row-major layout for cache efficiency
- Pixels stored as uint8_t (0-255)

**Tests (test_image.cpp):**
- Create image, set/get pixels
- Write test PNG to disk
- Verify PNG can be opened

---

## Phase 3: Mathematical Utilities

### 3.1 Implement Math Utilities (math_utils.h)

**Goal:** Provide vector/matrix operations using GLM.

**Functions to implement:**
```cpp
// math_utils.h
#pragma once
#include "types.h"

namespace scimesh {
    // Compute triangle normal from vertices
    Vec3 compute_face_normal(const Vec3& v0, const Vec3& v1, const Vec3& v2);
    
    // Compute vertex normals by averaging adjacent face normals
    void compute_vertex_normals(const Mesh& mesh, std::vector<Vec3>& normals);
    
    // Transform point by matrix
    Vec3 transform_point(const Mat4& matrix, const Vec3& point);
    
    // Transform direction by matrix (no translation)
    Vec3 transform_direction(const Mat4& matrix, const Vec3& direction);
    
    // Perspective divide (convert from clip space to NDC)
    Vec3 perspective_divide(const Vec4& clip_coord);
    
    // Convert NDC to screen coordinates
    void ndc_to_screen(const Vec3& ndc, int width, int height, 
                       float& screen_x, float& screen_y, float& depth);
}
```

**Tests (test_math.cpp):**
- Verify face normal computation
- Verify vertex normal averaging
- Test coordinate transformations
- Test NDC to screen conversion

### 3.2 Implement Normal Computation (normals.h/cpp)

**Goal:** Compute normals on-the-fly if not provided.

**Implementation:**
```cpp
// normals.cpp
#include "normals.h"
#include "math_utils.h"

namespace scimesh {
    void compute_vertex_normals(const Mesh& mesh, std::vector<Vec3>& normals) {
        normals.resize(mesh.vertices.size(), Vec3(0.0f));
        
        // Accumulate face normals for each vertex
        for (const auto& tri : mesh.triangles) {
            Vec3 normal = compute_face_normal(
                mesh.vertices[tri.v0],
                mesh.vertices[tri.v1],
                mesh.vertices[tri.v2]
            );
            normals[tri.v0] += normal;
            normals[tri.v1] += normal;
            normals[tri.v2] += normal;
        }
        
        // Normalize
        for (auto& n : normals) {
            n = glm::normalize(n);
        }
    }
}
```

**Tests (test_normals.cpp):**
- Compute normals for a cube
- Verify normals point outward
- Test with inverted winding (should flip normals)

---

## Phase 4: Rendering Pipeline Components

### 4.1 Implement Near-Plane Clipping (clipping.h/cpp)

**Goal:** Split triangles that intersect the near plane.

**Algorithm:** Sutherland-Hodgman clipping against near plane.

**Implementation:**
```cpp
// clipping.h
#pragma once
#include "types.h"
#include <vector>

namespace scimesh {
    // Vertex in clip space (after projection, before perspective divide)
    struct ClipVertex {
        Vec4 position;  // Clip space coordinates
        Color color;
        Vec3 normal;
    };
    
    // Clip triangle against near plane
    // Returns 0, 1, or 2 triangles (as indices into output_vertices)
    int clip_triangle_near_plane(
        const ClipVertex& v0,
        const ClipVertex& v1,
        const ClipVertex& v2,
        float near_plane,
        std::vector<ClipVertex>& output_vertices,
        std::vector<Triangle>& output_triangles
    );
}
```

**Implementation details:**
- Transform vertices to clip space
- Check which vertices are behind near plane (z < -w * near_plane)
- If all behind: discard triangle
- If all in front: keep triangle
- If mixed: split into 1 or 2 triangles with interpolated attributes

**Tests (test_clipping.cpp):**
- Triangle fully in front: should pass through unchanged
- Triangle fully behind: should be discarded
- Triangle partially behind: should be split correctly
- Verify interpolated attributes (color, normal) are correct

### 4.2 Implement Rasterizer (rasterizer.h/cpp)

**Goal:** Rasterize triangles with Z-buffer.

**Algorithm:** Bounding-box rasterization with barycentric coordinates.

**Implementation:**
```cpp
// rasterizer.h
#pragma once
#include "types.h"
#include "image.h"
#include <vector>

namespace scimesh {
    struct Rasterizer {
        int width;
        int height;
        std::vector<float> z_buffer;  // 32-bit float depth buffer
        
        Rasterizer(int w, int h);
        
        void clear(float clear_depth);
        
        // Rasterize a single triangle
        void rasterize_triangle(
            const Vec3& screen_v0, const Color& color0, const Vec3& normal0,
            const Vec3& screen_v1, const Color& color1, const Vec3& normal1,
            const Vec3& screen_v2, const Color& color2, const Vec3& normal2,
            bool backface_culling,
            bool smooth_shading,
            const Vec3& light_direction,  // Camera-aligned light
            Image& output
        );
        
    private:
        // Compute barycentric coordinates
        void compute_barycentric(
            float x, float y,
            float x0, float y0,
            float x1, float y1,
            float x2, float y2,
            float& u, float& v, float& w
        ) const;
        
        // Shade a pixel
        Color shade_pixel(
            const Color& color,
            const Vec3& normal,
            const Vec3& light_direction
        ) const;
    };
}
```

**Implementation details:**
- Bounding-box rasterization: only iterate over pixels within triangle bounds
- Clamp bounding box to [0, width-1] × [0, height-1]
- Use barycentric coordinates for interpolation
- Z-buffer test: `if (depth < z_buffer[pixel_index])`
- Headlight shading: light direction = camera forward vector
- Smooth shading: interpolate normals, then compute lighting
- Flat shading: use face normal for lighting

**Tests (test_rasterizer.cpp):**
- Render single triangle, verify pixels are set
- Test Z-buffer occlusion (overlapping triangles)
- Test backface culling
- Test smooth vs flat shading
- Verify bounding box clamping

### 4.3 Implement Main Renderer (renderer.h/cpp)

**Goal:** Orchestrate the full rendering pipeline.

**Implementation:**
```cpp
// renderer.h
#pragma once
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"

namespace scimesh {
    class Renderer {
    public:
        // Render a single mesh
        Image render_mesh(const Mesh& mesh, 
                         const Camera& camera, 
                         const RenderOptions& options);
        
        // Render a scene (multiple meshes)
        Image render_scene(const Scene& scene, 
                          const Camera& camera, 
                          const RenderOptions& options);
        
    private:
        // Internal rendering pipeline
        void render_pipeline(
            const std::vector<Mesh>& meshes,
            const Camera& camera,
            const RenderOptions& options,
            Image& output
        );
    };
}
```

**Pipeline implementation:**
```cpp
void Renderer::render_pipeline(
    const std::vector<Mesh>& meshes,
    const Camera& camera,
    const RenderOptions& options,
    Image& output
) {
    // 1. Initialize output image with background color
    output.clear(options.background_color);
    
    // 2. Initialize rasterizer with Z-buffer
    Rasterizer rasterizer(options.width, options.height);
    rasterizer.clear(1.0f);  // Far plane depth
    
    // 3. Compute view and projection matrices
    Mat4 view = camera.get_view_matrix();
    float aspect = (float)options.width / options.height;
    Mat4 projection = camera.get_projection_matrix(
        aspect, options.near_plane, options.far_plane
    );
    Mat4 view_projection = projection * view;
    
    // 4. For each mesh
    for (const auto& mesh : meshes) {
        // 4a. Compute normals if not provided
        std::vector<Vec3> normals;
        if (!mesh.has_normals()) {
            compute_vertex_normals(mesh, normals);
        }
        const auto& mesh_normals = mesh.has_normals() ? mesh.normals : normals;
        
        // 4b. For each triangle
        for (const auto& tri : mesh.triangles) {
            // Get vertices
            Vec3 v0 = mesh.vertices[tri.v0];
            Vec3 v1 = mesh.vertices[tri.v1];
            Vec3 v2 = mesh.vertices[tri.v2];
            
            // Get colors (or use default)
            Color c0 = mesh.has_colors() ? mesh.colors[tri.v0] : options.default_color;
            Color c1 = mesh.has_colors() ? mesh.colors[tri.v1] : options.default_color;
            Color c2 = mesh.has_colors() ? mesh.colors[tri.v2] : options.default_color;
            
            // Get normals
            Vec3 n0 = mesh_normals[tri.v0];
            Vec3 n1 = mesh_normals[tri.v1];
            Vec3 n2 = mesh_normals[tri.v2];
            
            // Apply invert_normals option
            if (options.invert_normals) {
                n0 = -n0;
                n1 = -n1;
                n2 = -n2;
            }
            
            // Transform to clip space
            ClipVertex cv0, cv1, cv2;
            cv0.position = view_projection * Vec4(v0, 1.0f);
            cv0.color = c0;
            cv0.normal = n0;
            
            cv1.position = view_projection * Vec4(v1, 1.0f);
            cv1.color = c1;
            cv1.normal = n1;
            
            cv2.position = view_projection * Vec4(v2, 1.0f);
            cv2.color = c2;
            cv2.normal = n2;
            
            // Near-plane clipping
            std::vector<ClipVertex> clipped_vertices;
            std::vector<Triangle> clipped_triangles;
            int num_clipped = clip_triangle_near_plane(
                cv0, cv1, cv2, options.near_plane,
                clipped_vertices, clipped_triangles
            );
            
            // For each clipped triangle
            for (const auto& clipped_tri : clipped_triangles) {
                const auto& cv0 = clipped_vertices[clipped_tri.v0];
                const auto& cv1 = clipped_vertices[clipped_tri.v1];
                const auto& cv2 = clipped_vertices[clipped_tri.v2];
                
                // Perspective divide (clip space -> NDC)
                Vec3 ndc0 = perspective_divide(cv0.position);
                Vec3 ndc1 = perspective_divide(cv1.position);
                Vec3 ndc2 = perspective_divide(cv2.position);
                
                // NDC -> screen coordinates
                float sx0, sy0, sz0;
                float sx1, sy1, sz1;
                float sx2, sy2, sz2;
                ndc_to_screen(ndc0, options.width, options.height, sx0, sy0, sz0);
                ndc_to_screen(ndc1, options.width, options.height, sx1, sy1, sz1);
                ndc_to_screen(ndc2, options.width, options.height, sx2, sy2, sz2);
                
                // Rasterize
                Vec3 screen_v0(sx0, sy0, sz0);
                Vec3 screen_v1(sx1, sy1, sz1);
                Vec3 screen_v2(sx2, sy2, sz2);
                
                Vec3 light_direction(0.0f, 0.0f, -1.0f);  // Camera-aligned
                
                rasterizer.rasterize_triangle(
                    screen_v0, cv0.color, cv0.normal,
                    screen_v1, cv1.color, cv1.normal,
                    screen_v2, cv2.color, cv2.normal,
                    options.backface_culling,
                    options.shading == ShadingMode::SMOOTH,
                    light_direction,
                    output
                );
            }
        }
    }
}
```

**Tests (test_integration.cpp):**
- Render a cube with known camera position
- Verify output image matches expected result
- Test multi-mesh scene
- Test with/without vertex colors
- Test with/without normals

---

## Phase 5: C++ Testing and Validation

### 5.1 Create Test Meshes

**Goal:** Generate simple test meshes programmatically.

**Test meshes to create:**
- Unit cube (8 vertices, 12 triangles)
- Tetrahedron (4 vertices, 4 triangles)
- Simple plane (4 vertices, 2 triangles)
- Icosphere (for testing smooth shading)

### 5.2 Write Integration Tests

**Goal:** End-to-end rendering tests.

**Test scenarios:**
1. **Basic rendering:** Render a cube, verify pixels are set
2. **Depth testing:** Render two overlapping cubes, verify occlusion
3. **Backface culling:** Render cube with culling on/off
4. **Shading modes:** Compare smooth vs flat shading output
5. **Default colors:** Render mesh without vertex colors
6. **Normal inversion:** Render with invert_normals=true
7. **Near-plane clipping:** Position camera inside mesh

### 5.3 Visual Validation

**Goal:** Generate reference images for visual inspection.

**Process:**
1. Render test scenes to PNG
2. Manually inspect images for correctness
3. Store as reference images in `cpp_tests/reference_images/`
4. Add image comparison tests (pixel-by-pixel or perceptual)

---

## Phase 6: R Bindings

### 6.1 Create Rcpp Bindings (src/rcpp_bindings.cpp)

**Goal:** Expose C++ renderer to R.

**Functions to expose:**
```cpp
// rcpp_bindings.cpp
#include <Rcpp.h>
#include "core/renderer.h"

using namespace Rcpp;

// [[Rcpp::export]]
List render_mesh_cpp(
    NumericMatrix vertices,      // Nx3
    IntegerMatrix triangles,     // Mx3 (1-based indices from R)
    Nullable<NumericMatrix> colors,  // Nx4 or NULL
    Nullable<NumericMatrix> normals, // Nx3 or NULL
    NumericVector eye,           // length 3
    NumericVector center,        // length 3
    NumericVector up,            // length 3
    std::string projection,      // "perspective" or "orthographic"
    double fov,
    int width,
    int height,
    std::string shading,         // "smooth" or "flat"
    bool backface_culling,
    NumericVector background_color,  // length 4
    NumericVector default_color,     // length 4
    bool invert_normals
) {
    // Convert R data structures to C++ structures
    // Call renderer
    // Return Image as R list
}
```

**Key considerations:**
- R uses 1-based indices, C++ uses 0-based → subtract 1 when converting
- R matrices are column-major, C++ expects row-major → transpose or copy carefully
- Use Rcpp::Nullable for optional parameters
- Return image as list with width, height, pixels (raw vector)

### 6.2 Create Layer 1 R Functions

**Goal:** Provide domain-agnostic R interface.

**Files to create:**
```
R/
├── render_mesh.R      # render_mesh() wrapper
├── render_scene.R     # render_scene() wrapper
├── camera.R           # camera(), camera_auto() functions
└── image_export.R     # write_png(), write_tiff() functions
```

**Example implementation:**
```r
# R/render_mesh.R
render_mesh <- function(mesh, camera, options = render_options()) {
    # Validate inputs
    # Convert mesh3d to internal format
    # Call C++ renderer via Rcpp
    # Return Image object
}

# R/camera.R
camera <- function(eye, center, up = c(0, 1, 0), 
                   projection = "perspective", fov = 45) {
    list(
        eye = eye,
        center = center,
        up = up,
        projection = projection,
        fov = fov
    )
}

camera_auto <- function(mesh) {
    # Compute bounding box
    # Position camera to fit mesh in view
    # Return camera object
}
```

---

## Phase 7: Testing and Refinement

### 7.1 R Package Tests

**Goal:** Test R interface and error handling.

**Test scenarios:**
- Invalid mesh (NaN coordinates, out-of-range indices)
- Missing vertex colors (should use default)
- Empty mesh (should return background image)
- Invalid camera parameters
- Invalid render options

### 7.2 Performance Testing

**Goal:** Verify performance meets 1.5-second budget.

**Test with:**
- Freesurfer cortical surface (~150k triangles)
- Resolution: 1000×750
- Measure render time
- Profile to identify bottlenecks

### 7.3 Documentation

**Goal:** Document the R API.

**Create:**
- Function documentation (roxygen2)
- Vignette with examples
- README with quick start guide

---

## Phase 8: Layer 2 and Layer 3 (Future)

**Goal:** Add neuro-specific convenience functions.

**Layer 2 functions:**
```r
vis.brain.lateral(mesh, hemisphere, data, ...)
vis.brain.medial(mesh, hemisphere, data, ...)
vis.brain.dorsal(mesh, hemisphere, data, ...)
```

**Layer 3 functions:**
```r
vis.subject.morph.native(subjects_dir, subject, data_name, views, ...)
```

**Note:** These require knowledge of RAS coordinates and Freesurfer conventions, so they belong in Layer 2/3, not the core renderer.

---

## Implementation Order Summary

1. **Phase 1:** Project infrastructure (R package skeleton, cpp_tests/)
2. **Phase 2:** Core data structures (Mesh, Scene, Camera, Image)
3. **Phase 3:** Math utilities (transformations, normals)
4. **Phase 4:** Rendering pipeline (clipping, rasterizer, renderer)
5. **Phase 5:** C++ testing and validation
6. **Phase 6:** R bindings (Rcpp, Layer 1 functions)
7. **Phase 7:** Testing, performance, documentation
8. **Phase 8:** Layer 2/3 neuro-specific functions (future)

---

## Key Technical Decisions

1. **GLM for math:** Use vendored GLM for vector/matrix operations
2. **stb_image_write:** Use vendored stb_image_write for PNG output in tests
3. **32-bit float Z-buffer:** Prevent z-fighting on fine geometry
4. **Bounding-box rasterization:** Cache-friendly, predictable performance
5. **Sutherland-Hodgman clipping:** Robust near-plane handling
6. **Headlight shading:** Simple, effective for brain visualization
7. **sRGB colorspace:** Skip linear-space conversion for simplicity
8. **Stateless rendering:** No persistent state, deterministic output

---

## Estimated Timeline

- **Phase 1-2:** 1-2 days (infrastructure, data structures)
- **Phase 3-4:** 3-5 days (math, rendering pipeline)
- **Phase 5:** 2-3 days (testing, validation)
- **Phase 6:** 2-3 days (R bindings)
- **Phase 7:** 1-2 days (testing, docs)
- **Total:** ~2-3 weeks for core renderer

This is a realistic timeline for a focused implementation effort.
