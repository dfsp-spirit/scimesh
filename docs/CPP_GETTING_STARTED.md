\page cpp_getting_started Getting Started with scimesh C++

A gentle introduction to using the scimesh C++ software renderer.  No
OpenGL, GPU, or display server required — just a C++17 compiler.

## What is scimesh C++?

scimesh is a **headless software renderer** that takes 3D triangle meshes
and produces RGBA images entirely on the CPU.  It is designed for
scientific visualization where GPU access is unavailable or impractical:
HPC clusters, CI pipelines, headless servers, and containers.

The renderer supports multi-light Blinn-Phong shading, anti-aliasing,
semi-transparent overlays, wireframe mode, SSAO, fog, clip planes, and
procedural geometry.

scimesh is **not** a game engine, raytracer, or interactive viewer.  It
produces static images.

## Prerequisites

- A C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10 or newer

## Project Layout

The C++ renderer core lives in `src/core/`:

| File | Purpose |
|------|---------|
| `scimesh/renderer.h` | Main `Renderer` class |
| `scimesh/mesh.h` | `Mesh` struct (vertices, triangles, colors, UVs, normals) |
| `scimesh/scene.h` | `Scene` struct (collection of meshes) |
| `scimesh/camera.h` | `Camera` struct and auto-framing helpers |
| `scimesh/render_options.h` | `RenderOptions` struct (resolution, shading, lights, AA, etc.) |
| `scimesh/image.h` | `Image` struct (RGBA pixel buffer, PPM/BMP output) |
| `scimesh/primitives.h` | Procedural geometry generators |
| `scimesh/transforms.h` | Mesh translation, scaling, rotation |
| `scimesh/normals.h` | Vertex normal computation |
| `scimesh/clipping.h` | Triangle clipping against planes |
| `scimesh/rasterizer.h` | Scanline rasterizer with z-buffer |
| `scimesh/math_utils.h` | Inline math helpers |
| `third_party/glm/` | Vendored GLM math library (headers only) |

To use scimesh in your own project, compile the `.cpp` files from
`src/core/` alongside your code.  Headers are under `src/core/scimesh/`
and included as `#include <scimesh/header.h>`.

## Minimal Example

The simplest program builds a colored cube and renders it:

```cpp
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/primitives.h>
#include <scimesh/image.h>

using namespace scimesh;

int main() {
    // 1. Create a mesh — a red cuboid
    Mesh cube = generate_cuboid(
        Vec3(0, 0, 0),              // center
        Vec3(1, 1, 1),              // half-extents
        Color(0.9f, 0.2f, 0.2f, 1)  // RGBA color
    );

    // 2. Set up camera — auto-frame to fit the mesh
    Vec3 view_dir = glm::normalize(Vec3(1.0f, 1.0f, 1.0f));
    Camera cam = camera_fit_mesh(cube, view_dir,
        Vec3(0, 1, 0),  // up
        45.0f,           // FOV
        1.1f             // margin
    );

    // 3. Configure rendering
    RenderOptions opts;
    opts.width  = 800;
    opts.height = 600;
    opts.background_color = Color(1, 1, 1, 1);  // white background
    opts.shading = ShadingMode::SMOOTH;

    // 4. Render
    Renderer renderer;
    Image img = renderer.render_mesh(cube, cam, opts);

    // 5. Save
    img.write_ppm("cube.ppm");
    img.write_bmp("cube.bmp");

    return 0;
}
```

### Loading a PLY Mesh

Instead of generating geometry, you can load a mesh from a PLY file:

```cpp
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/ply_io.h>
#include <scimesh/image.h>

using namespace scimesh;

int main() {
    Mesh mesh = ply_io::read("bunny.ply");

    Vec3 view_dir = glm::normalize(Vec3(1.0f, 1.0f, 1.0f));
    Camera cam = camera_fit_mesh(mesh, view_dir, Vec3(0, 1, 0), 45.0f, 1.1f);

    RenderOptions opts;
    opts.width  = 800;
    opts.height = 600;
    opts.background_color = Color(0.98f, 0.98f, 0.98f, 1);
    opts.shading = ShadingMode::SMOOTH;

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    img.write_ppm("bunny.ppm");

    return 0;
}
```

Compile with `ply_io.cpp` added to the source list (see Building below).

### Building

#### CMake with FetchContent (Recommended)

The easiest way to use scimesh in your own CMake project is via
`FetchContent`.  Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    scimesh
    GIT_REPOSITORY https://github.com/dfsp-spirit/scimesh.git
    GIT_TAG        main          # or a release tag, e.g. v0.2.8
)
FetchContent_MakeAvailable(scimesh)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scimesh)
```

That's it — CMake will clone scimesh, build it as a static library, and
make all headers and compiled code available to your target.  No manual
source listing, include paths, or preprocessor defines needed.

For the in-repo examples, see any
[`examples/cpp/*/CMakeLists.txt`](../examples/cpp/all_primitives/CMakeLists.txt)
which use `add_subdirectory()` for the same effect.

#### Manually (without CMake)

Copy scimesh `src/` so it sits next to your project.  Assuming your code is in
`my_project/`, the layout should look like this:

```
parent_directory/
├── my_project/           ← your application
│   ├── main.cpp
│   └── build/
└── src/                  ← scimesh repo src/ copied here
    ├── core/
    └── third_party/
```

From inside `build/`, scimesh sources are reachable at `../../src/`.
Compile the `.cpp` files from `../../src/core/` alongside `../main.cpp`
with `-std=c++17`.

> **Note:** If your code calls `write_png()`, add `-DSCIMESH_STB_WRITE_IMPL`.

A runnable script that manually compiles one of our demos using `g++`:
[`examples/cpp/all_primitives/build_manually.sh`](../examples/cpp/all_primitives/build_manually.sh).

## Core Concepts

### Mesh

A `Mesh` holds the geometry and appearance data:

```cpp
struct Mesh {
    std::vector<Vec3>     vertices;     // vertex positions
    std::vector<Triangle> triangles;    // triangle index triplets
    std::vector<Color>    colors;       // per-vertex RGBA (optional)
    std::vector<Color>    face_colors;  // per-face RGBA (optional)
    std::vector<Vec3>     normals;      // per-vertex normals (optional)
    std::vector<Vec2>     uvs;          // texture coordinates (optional)
    Image                 texture;      // texture image (optional)
    Color                 default_color;
    bool                  has_transparency;
};
```

A `Triangle` is three indices into the vertex array:

```cpp
struct Triangle { uint32_t v0, v1, v2; };
```

Colors are RGBA floats in [0, 1]:

```cpp
struct Color { float r, g, b, a; };
```

### Scene

A `Scene` is a collection of meshes rendered together:

```cpp
Scene scene;
scene.meshes.push_back(mesh_a);
scene.meshes.push_back(mesh_b);
```

When meshes have semi-transparent colors (`has_transparency = true` or
alpha < 1), the renderer sorts them back-to-front and blends
appropriately.

### Camera

A `Camera` defines the viewpoint:

```cpp
Camera cam;
cam.eye    = Vec3(0, 0, 5);    // where the camera is
cam.center = Vec3(0, 0, 0);    // what it looks at
cam.up     = Vec3(0, 1, 0);    // up direction
cam.projection = ProjectionType::PERSPECTIVE;
cam.fov_degrees = 45.0f;
```

Use the auto-framing helpers to avoid manual positioning:

```cpp
// Fit a single mesh
Camera cam = camera_fit_mesh(mesh, view_dir, up, 45.0f, 1.1f);

// Fit an entire scene
Camera cam = camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);
```

### RenderOptions

Controls image size, shading, lighting, and post-processing:

```cpp
RenderOptions opts;
opts.width  = 1200;
opts.height = 900;
opts.shading = ShadingMode::SMOOTH;        // or FLAT
opts.backface_culling = true;
opts.background_color = Color(0, 0, 0, 0); // transparent

// Lighting
opts.ambient = 0.2f;
opts.contrast = 1.1f;
opts.lights = {key_light, fill_light, rim_light};
opts.specular_color = Color(0.4f, 0.4f, 0.4f);
opts.shininess = 64.0f;

// Anti-aliasing
opts.aa_samples = 2;  // 2x2 SSAA

// SSAO
opts.ssao_enabled = true;
opts.ssao_radius = 12.0f;
opts.ssao_intensity = 0.5f;

// Projection
opts.projection = ProjectionType::ORTHOGRAPHIC;
```

### Renderer

The `Renderer` is the main entry point:

```cpp
Renderer renderer;

// Single mesh
Image img = renderer.render_mesh(mesh, cam, opts);

// Multi-mesh scene
Image img = renderer.render_scene(scene, cam, opts);

// Raw triangles (no index buffer)
Image img = renderer.render_triangles_raw(positions, colors, cam, opts);

// Points
Image img = renderer.render_points_raw(positions, colors, radius, cam, opts);
```

### Image

`Image` stores RGBA pixels and can write to several formats:

```cpp
Image img(width, height);

img.write_ppm("output.ppm");  // ASCII PPM
img.write_bmp("output.bmp");  // Windows BMP

// Direct pixel access
img.set_pixel(x, y, r, g, b, a);
img.get_pixel(x, y, r, g, b, a);
img.clear(r, g, b, a);

// Bilinear texture sampling
Color c = img.sample_bilinear(u, v);

// Post-processing
img.apply_contrast(1.1f);  // S-curve contrast stretch

// Downsample (e.g., for anti-aliasing)
Image small = img.downsample_box(2);
```

## Mesh I/O

scimesh can read and write several mesh formats:

### STL

```cpp
#include <scimesh/stl_io.h>

Mesh mesh = stl_io::read("model.stl");
stl_io::write(mesh, "output.stl");         // binary
stl_io::write(mesh, "output.stl", true);   // ASCII
```

### Wavefront OBJ

```cpp
#include <scimesh/obj_io.h>

Mesh mesh = obj_io::read("model.obj");
```

The OBJ reader supports vertices, faces, UV coordinates, and normals.
For textured meshes, the texture image must be loaded separately.

### Stanford PLY

```cpp
#include <scimesh/ply_io.h>

Mesh mesh = ply_io::read("model.ply");
```

The PLY reader supports ASCII and binary formats, with optional
per-vertex colors.

## Procedural Geometry

Generate primitives without external files:

```cpp
#include <scimesh/primitives.h>

Mesh sphere = generate_sphere(
    Vec3(0, 0, 0),     // center
    1.0f,               // radius
    32,                 // segments
    Color(0, 0.8f, 1, 1)
);

Mesh cylinder = generate_cylinder(
    Vec3(0, 0, 0),     // start
    Vec3(0, 3, 0),     // end
    0.5f,               // radius
    16,                 // segments
    Color(1, 0.5f, 0, 1)
);

Mesh cuboid = generate_cuboid(
    Vec3(0, 0, 0),     // center
    Vec3(1, 2, 0.5f),  // half-extents
    Color(0.2f, 0.6f, 1, 1)
);

Mesh torus = generate_torus(
    Vec3(0, 0, 0),     // center
    1.5f,               // major radius
    0.4f,               // minor radius
    32, 16,             // major/minor segments
    Color(1, 0.5f, 0, 1)
);

Mesh cone = generate_cone(
    Vec3(0, 0, 0),     // base
    Vec3(0, 2, 0),     // tip
    0.8f,               // radius
    16,                 // segments
    Color(0.5f, 0, 0.5f, 1)
);

Mesh plane = generate_plane(
    Vec3(0, 0, 0),     // center
    Vec3(0, 1, 0),     // normal
    2.0f, 2.0f,         // half-sizes
    Color(0.8f, 0.8f, 0.8f, 1)
);
```

### Merged Primitives

For rendering many instances at once (e.g., molecular atoms), use the
merged variants that combine multiple primitives into a single mesh:

```cpp
#include <scimesh/primitives.h>

std::vector<Vec3> centers = {Vec3(0,0,0), Vec3(2,0,0), Vec3(1,2,0)};
std::vector<float> radii = {0.5f, 0.3f, 0.4f};
std::vector<Color> colors = {
    Color(1,0,0,1), Color(0,1,0,1), Color(0,0,1,1)
};

Mesh atoms = generate_multi_spheres(centers, radii, colors, 16);

// Cylinders between point pairs
Mesh bonds = generate_multi_cylinders(starts, ends, radii, colors, 12);
```

## Mesh Transforms

Transform vertices without modifying colors or normals:

```cpp
#include <scimesh/transforms.h>

translate_mesh(mesh, Vec3(5, 0, 0));
scale_mesh(mesh, 2.0f);
rotate_mesh(mesh, 3.14159f / 4, Vec3(0, 0, 1));  // 45 degrees around Z

// Arbitrary 4x4 matrix
glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
transform_mesh(mesh, M);
```

## Lighting

Scimesh works in display-referred (sRGB) colour space for simplicity.
Lights are defined as structs with position, color, and intensity:

```cpp
Light key_light;
key_light.position  = Vec3(0.5f, 1.0f, 1.0f);
key_light.color     = Color(1.0f, 0.97f, 0.90f);
key_light.intensity = 1.5f;

Light fill_light;
fill_light.position  = Vec3(-1.0f, 0.2f, 0.5f);
fill_light.color     = Color(0.4f, 0.5f, 0.8f);
fill_light.intensity = 0.5f;

RenderOptions opts;
opts.lights = {key_light, fill_light};
opts.ambient = 0.2f;
opts.gamma = 2.2f;
opts.specular_color = Color(0.4f, 0.4f, 0.4f);
opts.shininess = 64.0f;
```

When no lights are specified, a single headlight at `(0, 0, 1)` is
used.

### Contrast

`opts.contrast` (default 1.0, no change) applies an S-curve contrast
stretch after shading via `(value - 0.5) * contrast + 0.5`.
Values > 1.0 push darks toward black and lights toward white.
Typical values: 1.1--1.3 for subtle contrast, up to 1.5 for dramatic.

### Ambient

`opts.ambient` (default 0.3) controls how much light reaches surfaces
facing away from the light source. Lower values produce deeper shadows.
Typical values are 0.1--0.3.

### Post-Processing

`Image::apply_contrast()` applies the same S-curve to an already
rendered image:

```cpp
Image img = renderer.render_mesh(mesh, cam, opts);
img.apply_contrast(1.1f);  // S-curve contrast stretch
img.write_png("output.png");
```

## Semi-Transparent Meshes

For transparency, set per-vertex colors with alpha < 1 and mark the mesh:

```cpp
Mesh glass_mesh = /* ... */;
for (auto &c : glass_mesh.colors) c.a = 0.3f;
glass_mesh.has_transparency = true;

Scene scene;
scene.meshes.push_back(opaque_mesh);
scene.meshes.push_back(glass_mesh);

// The renderer sorts transparent meshes back-to-front
Image img = renderer.render_scene(scene, cam, opts);
```

## Camera Helpers

### Auto-framing

```cpp
// Look from a direction, auto-distance to fit
Camera cam = camera_fit_mesh(mesh,
    Vec3(-1, 0, 0.2f),  // view direction
    Vec3(0, 0, 1),       // up
    45.0f,                // FOV
    1.05f                 // 5% margin
);

// Fit an entire scene
Camera cam = camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);
```

### Orthographic projection

```cpp
cam.projection = ProjectionType::ORTHOGRAPHIC;
cam.fov_degrees = 40.0f;  // orthographic extent
```

### Orbit paths

`camera_orbit()` rotates a camera's eye and up around its center:

```cpp
Camera cam = camera_fit_mesh(mesh, view_dir, up, 45.0f);
for (int i = 0; i < 48; i++) {
    Camera orbit_cam = camera_orbit(cam, Vec3(0, 0, 1), 360.0f / 48 * i);
    Image img = renderer.render_mesh(mesh, orbit_cam, opts);
    // write frame...
}
```

This is useful for turntable video sequences.  See `examples/cpp/brain_video/`
for a complete example.

## Anti-Aliasing

Set `aa_samples` to 2 or 4 for supersampled anti-aliasing:

```cpp
opts.aa_samples = 2;  // 2x2 SSAA — renders at 2x resolution, downsamples
```

Higher values produce smoother edges but use more memory and time.

## Examples

The `examples/cpp/` directory contains complete, runnable programs:

| Example | What it demonstrates |
|---------|---------------------|
| `spot_cow/` | Textured OBJ mesh with multi-light setup and SSAO |
| `transparency/` | Semi-transparent overlays with FreeSurfer surfaces |
| `protein_data_bank_pdb_file/` | Protein visualization from PDB files |
| `whole_brain_sulc/` | Whole-brain sulcal depth rendering |
| `whole_brain_sulc_fsaverage/` | Same on fsaverage template |
| `whole_brain_annot/` | Cortical parcellation coloring |
| `brain_video/` | Turntable animation — 48 orbit frames via camera_orbit() |

Each example has its own `CMakeLists.txt`.  To build and run:

```sh
cd examples/cpp/spot_cow
mkdir -p build && cd build
cmake ..
make
./spot_cow
```

Run all examples at once:

```sh
./examples/cpp/run_all_cpp.sh
```

## Common Pitfalls

**Black images / nothing visible:**
- Check that near/far planes contain your geometry.  The defaults work
  for unit-scale meshes; for very large or distant scenes, adjust
  `opts.near_plane` and `opts.far_plane`.
- Make sure your camera is looking at the mesh.  Use
  `camera_fit_mesh()` to auto-frame.

**Inverted faces / missing triangles:**
- Triangle winding order matters for backface culling.  If faces
  disappear when rotating, try `opts.backface_culling = false` or
  `opts.invert_normals = true`.

**Open surfaces:**
- For surface that are open (not watertight), set
  `opts.backface_culling = false` so both sides are visible.

**Transparency not working:**
- Make sure the mesh has `has_transparency = true` and vertex colors
  with alpha < 1.  Transparent meshes are rendered back-to-front.
- Note that you can also set the background color for the rendered image to transparent.

**Performance:**
- A 300k triangle mesh at 1200x900 with 2x AA renders in ~1-3 seconds.
  Higher AA samples and SSAO add cost.  Use `opts.threads = 0` for
  auto-parallelization (requires OpenMP).
