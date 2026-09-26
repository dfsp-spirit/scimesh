/// @file mesh.h
/// @brief The Mesh — the central data structure for 3D geometry in scimesh.
///
/// A `Mesh` is an **indexed face set**: vertices are stored once in an array,
/// and triangles reference them by index.  This is the standard representation
/// used in computer graphics, file formats like OBJ/STL/PLY, and GPU buffers.
///
/// @par Data layout (indexed face set)
/// @code{.unparsed}
///   vertices = [v0, v1, v2, v3, ...]          // 3D positions
///   triangles = [{v0=0, v1=1, v2=2}, ...]     // indices into vertices[]
///   colors = [c0, c1, c2, c3, ...]            // per-vertex colors (optional)
///   normals = [n0, n1, n2, n3, ...]           // per-vertex normals (optional)
///   uvs = [uv0, uv1, uv2, uv3, ...]           // texture coordinates (optional)
/// @endcode

#pragma once

#include <scimesh/types.h>
#include <scimesh/image.h>
#include <vector>
#include <cmath>

namespace scimesh {

/// @brief A 3D triangle mesh using an indexed face set representation.
///
/// ## Overview
///
/// The `Mesh` is the heart of scimesh.  It holds:
/// - **vertices** — 3D positions (required)
/// - **triangles** — which three vertices form each face (required)
/// - **colors** — per-vertex RGBA colors (optional, for vertex coloring)
/// - **face_colors** — per-face RGBA colors (optional, for flat face coloring)
/// - **normals** — per-vertex surface normals (optional, for lighting)
/// - **uvs** — texture coordinates (optional, for texture mapping)
/// - **texture** — an Image used as a texture map (optional)
///
/// The mesh uses an **indexed face set**.  This means vertices are stored
/// exactly once, and triangles store *indices* into the vertex array.
/// This saves memory when vertices are shared by multiple triangles.
///
/// ## Construction
///
/// There is no special constructor — create an empty mesh and populate it:
///
/// @code{.cpp}
/// Mesh m;
/// m.vertices = { {0,0,0}, {1,0,0}, {0,1,0} };   // three points
/// m.triangles = { {0, 1, 2} };                   // one triangle
/// m.colors = { {1,0,0}, {0,1,0}, {0,0,1} };     // red, green, blue vertices
/// @endcode
///
/// Or use one of the generator functions:
/// @code{.cpp}
/// Mesh sphere = scimesh::generate_sphere({0,0,0}, 1.0f, 32, {0.2f, 0.4f, 0.8f});
/// @endcode
///
/// Or load from a file:
/// @code{.cpp}
/// Mesh brain = scimesh::obj_io::read_obj("/data/brain.obj");
/// Mesh skull = scimesh::ply_io::read_ply("/data/skull.ply");
/// @endcode
///
/// ## Validation
///
/// Call `is_valid()` before rendering to detect common problems:
/// - missing vertices or triangles
/// - out-of-bounds triangle indices
/// - NaN or infinite vertex positions
/// - mismatched array sizes (e.g., colors but not one per vertex)
///
/// @see generate_sphere(), generate_cuboid(), generate_torus()
/// @see read_obj(), read_ply(), read_stl()
/// @see Renderer, Triangle, Color
struct Mesh {
    /// @name Required data
    /// @{

    /// @brief 3D vertex positions.
    ///
    /// Every mesh **must** have vertices.  Each element is a `Vec3` (x, y, z).
    /// Triangles reference these by index.
    ///
    /// @see Triangle, Vec3
    std::vector<Vec3> vertices;

    /// @brief Triangle index triplets.
    ///
    /// Each element `{v0, v1, v2}` points into `vertices[]`.
    /// A valid mesh must have at least one triangle.
    ///
    /// @see Triangle, vertices
    std::vector<Triangle> triangles;

    /// @}

    /// @name Optional per-vertex data
    /// @{

    /// @brief Per-vertex RGBA colors.
    ///
    /// If non-empty, must have exactly `vertices.size()` elements.
    /// Used for vertex-colored rendering (interpolated across triangles).
    ///
    /// @see Color, has_colors(), face_colors
    std::vector<Color> colors;

    /// @brief Per-face RGBA colors.
    ///
    /// If non-empty, must have exactly `triangles.size()` elements.
    /// Overrides per-vertex colors when present (flat shading by face).
    ///
    /// @see Color, has_face_colors(), colors
    std::vector<Color> face_colors;

    /// @brief Per-vertex surface normals (unit-length direction vectors).
    ///
    /// If non-empty, must have exactly `vertices.size()` elements.
    /// Normals are used for lighting calculations (Blinn-Phong shading).
    ///
    /// You can compute normals automatically with compute_vertex_normals().
    ///
    /// @see Vec3, has_normals(), compute_vertex_normals()
    std::vector<Vec3> normals;

    /// @brief Per-vertex texture coordinates (UVs).
    ///
    /// If non-empty, must have exactly `vertices.size()` elements.
    ///
    /// UVs are **image-space** coordinates, following the same rule as every
    /// other coordinate in scimesh: `u = 0` is the left edge of the texture
    /// image and `v = 0` its **top** edge, so `(0, 0)` addresses the top-left
    /// pixel of the texture and `(1, 1)` the bottom-right one.  That is exactly
    /// what Image::sample_bilinear() expects, so the renderer passes the UVs
    /// through unchanged.
    ///
    /// Note that OBJ and PLY files, OpenGL, rgl and tools like Blender and
    /// MeshLab store UVs with the opposite convention (v = 0 at the *bottom*),
    /// so those coordinates have to be converted once after loading — either by
    /// calling flip_uvs() or by subtracting from 1 while copying them (as
    /// `examples/cpp/spot_cow/` does).
    ///
    /// Only used when a `texture` image is also set; the renderer samples it
    /// with bilinear interpolation and multiplies it with the vertex color.
    ///
    /// @see Vec2, has_uvs(), texture, flip_uvs(), Image::sample_bilinear()
    std::vector<Vec2> uvs;

    /// @}

    /// @name Optional texture
    /// @{

    /// @brief An optional texture image mapped via UV coordinates.
    ///
    /// When set (width > 0), the rasterizer samples this image using the
    /// mesh's UV coordinates to color each pixel.
    ///
    /// @see Image, uvs, has_texture()
    Image texture;

    /// @}

    /// @name Default appearance
    /// @{

    /// @brief Fallback color when no per-vertex or per-face color is set.
    ///
    /// Defaults to `DEFAULT_COLOR` (neutral light gray).  You can change
    /// this to give a mesh a uniform color without populating `colors`.
    ///
    /// @see DEFAULT_COLOR
    Color default_color = DEFAULT_COLOR;

    /// @brief Whether the mesh contains any transparent fragments.
    ///
    /// You normally do **not** have to set this yourself: `is_transparent()`
    /// inspects `default_color`, `colors` and `face_colors` and is what the
    /// renderer uses, so a mesh whose colors have alpha < 1.0 renders
    /// translucent automatically (as do meshes loaded with such colors).
    /// Setting this to `true` forces the alpha-blending pass for a mesh that is
    /// not detected automatically (e.g. one whose transparency comes from an
    /// RGBA texture: the texture alpha is applied per fragment).
    ///
    /// @see is_transparent(), update_transparency(), colors, face_colors
    bool has_transparency = false;

    /// @}

    // ------------------------------------------------------------------
    //  Query methods
    // ------------------------------------------------------------------

    /// @brief Does the mesh have per-vertex colors?
    /// @return `true` if `colors` is not empty.
    /// @see colors
    bool has_colors() const { return !colors.empty(); }

    /// @brief Does the mesh have per-face colors?
    /// @return `true` if `face_colors` is not empty.
    /// @see face_colors
    bool has_face_colors() const { return !face_colors.empty(); }

    /// @brief Does the mesh have per-vertex normals?
    /// @return `true` if `normals` is not empty.
    /// @see normals, compute_vertex_normals()
    bool has_normals() const { return !normals.empty(); }

    /// @brief Does the mesh have texture coordinates?
    /// @return `true` if `uvs` is not empty.
    /// @see uvs, texture
    bool has_uvs() const { return !uvs.empty(); }

    /// @brief Does the mesh have a texture image?
    /// @return `true` if `texture.width > 0`.
    /// @see texture, uvs
    bool has_texture() const { return texture.width > 0; }

    /// @brief Do any of the mesh's colors have alpha < 1.0?
    ///
    /// Scans `default_color`, `colors` and `face_colors`.  This is a plain
    /// query: it does not look at `has_transparency`.
    ///
    /// @return `true` if at least one color is not fully opaque.
    /// @see is_transparent(), colors, face_colors, default_color
    bool has_alpha() const {
        const float threshold = 1.0f - 1e-6f;
        if (default_color.a < threshold) return true;
        for (const auto &c : colors) {
            if (c.a < threshold) return true;
        }
        for (const auto &c : face_colors) {
            if (c.a < threshold) return true;
        }
        return false;
    }

    /// @brief Does the mesh need the alpha-blending pass?
    ///
    /// True when `has_transparency` is set manually, or when any color (or
    /// `default_color`) has alpha < 1.0.  The renderer asks this instead of
    /// requiring you to keep `has_transparency` in sync, so per-vertex,
    /// per-face and uniform transparency all work out of the box.
    ///
    /// @return `true` if the mesh contains translucent fragments.
    /// @see has_alpha(), update_transparency(), has_transparency
    bool is_transparent() const { return has_transparency || has_alpha(); }

    /// @brief Recompute `has_transparency` from the mesh's colors.
    ///
    /// Sets the flag when `has_alpha()` reports translucent colors.  It never
    /// clears the flag, so a manually requested blend pass survives.
    ///
    /// @see is_transparent(), has_alpha()
    void update_transparency() {
        if (has_alpha()) has_transparency = true;
    }

    // ------------------------------------------------------------------
    //  Validation
    // ------------------------------------------------------------------

    /// @brief Check whether the mesh is in a valid, renderable state.
    ///
    /// This performs several checks:
    ///
    /// 1. **Geometry exists**: `vertices` and `triangles` are non-empty.
    /// 2. **Index bounds**: all triangle indices are within `[0, vertices.size())`.
    /// 3. **No degenerate data**: no NaN or infinite values in vertex positions.
    /// 4. **Array consistency**: if optional arrays (`colors`, `face_colors`,
    ///    `normals`, `uvs`) are present, they must have the expected size
    ///    (one element per vertex or per face, as appropriate).
    ///
    /// @par Why would I call this?
    /// After constructing a mesh manually, or after loading one from a file,
    /// call `is_valid()` to catch errors before rendering.  A mesh that fails
    /// validation will either produce garbage output or crash the renderer.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Mesh m = scimesh::obj_io::read_obj("model.obj");
    /// if (!m.is_valid()) {
    ///     std::cerr << "Mesh failed validation — check your file!\n";
    ///     return;
    /// }
    /// // safe to render now
    /// @endcode
    ///
    /// @return `true` if the mesh passes all checks.
    /// @see empty(), compute_bounding_box()
    bool is_valid() const {
        // Geometry must exist
        if (vertices.empty()) return false;
        if (triangles.empty()) return false;

        // Triangle indices must be in-bounds
        const auto nv = vertices.size();
        for (const auto &tri : triangles) {
            if (tri.v0 >= nv || tri.v1 >= nv || tri.v2 >= nv)
                return false;
        }

        // No degenerate geometry
        for (const auto &v : vertices) {
            if (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z))
                return false;
            if (std::isinf(v.x) || std::isinf(v.y) || std::isinf(v.z))
                return false;
        }

        // Optional arrays: if present, must match vertex or face count
        if (!colors.empty() && colors.size() != nv) return false;
        if (!face_colors.empty() && face_colors.size() != triangles.size())
            return false;
        if (!normals.empty() && normals.size() != nv) return false;
        if (!uvs.empty() && uvs.size() != nv) return false;

        return true;
    }

    /// @brief Is the mesh empty (no renderable geometry)?
    ///
    /// @return `true` if there are no vertices or no triangles.
    /// @see is_valid()
    bool empty() const { return vertices.empty() || triangles.empty(); }

    // ------------------------------------------------------------------
    //  Bounding box
    // ------------------------------------------------------------------

    /// @brief Compute the axis-aligned bounding box (AABB) of the mesh.
    ///
    /// The bounding box is the smallest box aligned with the X, Y, and Z
    /// axes that contains all mesh vertices.  Output is written to the
    /// two `Vec3` reference parameters.
    ///
    /// @param[out] min_bound  The corner with the smallest x, y, z values.
    /// @param[out] max_bound  The corner with the largest x, y, z values.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Vec3 bmin, bmax;
    /// mesh.compute_bounding_box(bmin, bmax);
    /// Vec3 center = (bmin + bmax) * 0.5f;   // geometric center
    /// Vec3 size   = bmax - bmin;             // dimensions
    /// @endcode
    ///
    /// @see Scene::compute_bounding_box()
    void compute_bounding_box(Vec3 &min_bound, Vec3 &max_bound) const {
        if (vertices.empty()) {
            min_bound = Vec3(0.0f);
            max_bound = Vec3(0.0f);
            return;
        }
        min_bound = vertices[0];
        max_bound = vertices[0];
        for (const auto &v : vertices) {
            min_bound = glm::min(min_bound, v);
            max_bound = glm::max(max_bound, v);
        }
    }
};

} // namespace scimesh
