/// @file types.h
/// @brief Fundamental types used throughout the scimesh rendering engine.
///
/// This header defines the basic building blocks: vectors, colors, triangles,
/// lights, and clip planes.  All scimesh types live in the `scimesh` namespace.
///
/// @note scimesh uses GLM (OpenGL Mathematics) for its vector and matrix types.
///       `Vec2`, `Vec3`, `Vec4`, and `Mat4` are all GLM types, so you get the
///       full GLM API for free — dot products, cross products, matrix
///       multiplication, etc.

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <cstdint>
#include <string>

namespace scimesh {

// ---------------------------------------------------------------------------
//  Vector & matrix type aliases (GLM)
// ---------------------------------------------------------------------------

/// @brief 2-component floating-point vector (xy).
///
/// Use `v.x`, `v.y` to access components.
/// @see Vec3, Vec4, Mat4
using Vec2 = glm::vec2;

/// @brief 3-component floating-point vector (xyz).
///
/// This is the workhorse type for positions, directions, and normals.
/// Access components with `.x`, `.y`, `.z`.
///
/// @par Example
/// @code{.cpp}
/// Vec3 pos(1.0f, 2.0f, 3.0f);   // a point in 3D space
/// Vec3 dir = glm::normalize(pos); // convert to unit-length direction
/// float dot = glm::dot(pos, dir); // dot product
/// @endcode
/// @see Vec2, Vec4, Mat4
using Vec3 = glm::vec3;

/// @brief 4-component floating-point vector (xyzw).
///
/// Primarily used internally for homogeneous clip-space coordinates.
/// @see Vec3, Mat4
using Vec4 = glm::vec4;

/// @brief 4×4 floating-point matrix.
///
/// Used for model, view, and projection transforms. GLM matrices are
/// **column-major**, matching OpenGL conventions.
///
/// @par Example
/// @code{.cpp}
/// Mat4 identity(1.0f);                   // identity matrix
/// Mat4 scaled = glm::scale(identity, Vec3(2.0f));  // scale by 2
/// @endcode
/// @see Vec3, Vec4
using Mat4 = glm::mat4;

// ---------------------------------------------------------------------------
//  Color
// ---------------------------------------------------------------------------

/// @brief An RGBA color with floating-point components.
///
/// Each channel is in the range `[0.0, 1.0]`, where 0.0 is none and 1.0 is
/// full intensity.  The alpha channel (`a`) controls opacity: 1.0 is fully
/// opaque, 0.0 is fully transparent.
///
/// Colors are used to describe vertex colors, face colors, light colors,
/// background colors, and more throughout the engine.
///
/// @par Constructing colors
/// @code{.cpp}
/// Color red(1.0f, 0.0f, 0.0f);          // opaque red
/// Color half_blue(0.0f, 0.0f, 0.5f);    // semi-dark blue
/// Color transparent_green(0.0f, 1.0f, 0.0f, 0.3f); // green, 30% opaque
/// @endcode
///
/// @see DEFAULT_COLOR, TRANSPARENT_BLACK
struct Color {
    float r = 0.0f;  ///< Red channel, [0, 1].
    float g = 0.0f;  ///< Green channel, [0, 1].
    float b = 0.0f;  ///< Blue channel, [0, 1].
    float a = 1.0f;  ///< Alpha (opacity) channel, [0, 1].  1.0 = fully opaque.

    constexpr Color() = default;

    /// @brief Construct a Color from RGBA values.
    /// @param r_ Red channel   (0.0–1.0).
    /// @param g_ Green channel (0.0–1.0).
    /// @param b_ Blue channel  (0.0–1.0).
    /// @param a_ Alpha channel (0.0–1.0), defaults to 1.0 (opaque).
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}
};

// ---------------------------------------------------------------------------
//  Triangle
// ---------------------------------------------------------------------------

/// @brief A triangle defined by three vertex indices.
///
/// A `Triangle` does **not** store actual 3D positions.  Instead it stores
/// three **indices** (`v0`, `v1`, `v2`) into a `Mesh::vertices` array.
/// This is the standard "indexed face set" representation used throughout
/// computer graphics: vertices are stored once and triangles reference them.
///
/// @par How it works (indexed face set)
/// @code{.cpp}
/// // Imagine a mesh with 4 vertices:
/// mesh.vertices = { vA, vB, vC, vD };
/// mesh.triangles = {
///     {0, 1, 2},  // first triangle:  vertices[0], vertices[1], vertices[2]
///     {1, 2, 3}   // second triangle: vertices[1], vertices[2], vertices[3]
/// };
/// @endcode
///
/// @see Mesh, Mesh::vertices, Mesh::triangles
struct Triangle {
    /// @brief Index of the first vertex in Mesh::vertices.
    uint32_t v0 = 0;
    /// @brief Index of the second vertex in Mesh::vertices.
    uint32_t v1 = 0;
    /// @brief Index of the third vertex in Mesh::vertices.
    uint32_t v2 = 0;
};

// ---------------------------------------------------------------------------
//  Named color constants
// ---------------------------------------------------------------------------

/// @brief The default mesh color: a neutral light gray (0.7, 0.7, 0.7).
///
/// Used when no explicit color is assigned to a mesh.
/// @see Color, Mesh::default_color
constexpr Color DEFAULT_COLOR{0.7f, 0.7f, 0.7f, 1.0f};

/// @brief A fully transparent black color (0, 0, 0, 0).
///
/// Convenience constant for transparent backgrounds or clearing.
/// @see Color
constexpr Color TRANSPARENT_BLACK{0.0f, 0.0f, 0.0f, 0.0f};

/// @brief An opaque white color (1, 1, 1, 1).
///
/// Convenience constant for backgrounds.
/// @see Color
constexpr Color WHITE{1.0f, 1.0f, 1.0f, 1.0f};

// ---------------------------------------------------------------------------
//  Light
// ---------------------------------------------------------------------------

/// @brief A light source for the Blinn-Phong shading model.
///
/// Lights illuminate meshes during rendering.  A directional light (the
/// default) shines from a direction toward the origin — its `position` is
/// treated as a direction vector.  Point lights are not yet supported.
///
/// @par Example: setting up two lights
/// @code{.cpp}
/// RenderOptions opts;
/// opts.lights.push_back(Light{Vec3(0, 0, 1), Color(1,1,1), 1.0f, true});
/// opts.lights.push_back(Light{Vec3(1, 0, 0), Color(1,0,0), 0.5f, true});
/// // Light 1: white from +Z, full intensity
/// // Light 2: red from +X, half intensity
/// @endcode
///
/// @see RenderOptions, RenderOptions::lights, RenderOptions::ambient
/// @see shade_pixel(), shade_pixel_multi()
struct Light {
    /// @brief Position of the light.
    ///
    /// When `is_directional == true` (the default), this is interpreted as
    /// a **direction vector** (the light is infinitely far away).
    Vec3 position = Vec3(0.0f, 0.0f, 1.0f);

    /// @brief Color of the light (default: white).
    ///
    /// Each channel multiplies the surface color during shading.
    Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);

    /// @brief Brightness multiplier (default: 1.0).
    ///
    /// Values > 1.0 produce brighter-than-white illumination (may saturate).
    float intensity = 1.0f;

    /// @brief Whether this is a directional light (default: true).
    ///
    /// When `true`, light rays are parallel (like the sun).
    /// Point lights (not yet implemented) would set this to `false`.
    bool is_directional = true;
};

// ---------------------------------------------------------------------------
//  ClipPlane
// ---------------------------------------------------------------------------

/// @brief A clipping plane that can hide parts of the scene.
///
/// A clip plane is defined by a normal vector and an offset.  Anything on
/// the "negative" side of the plane (where
/// `dot(point, normal) + offset < 0`) is discarded during rendering.
///
/// Clip planes are useful for cross-section views (e.g., slicing through a
/// brain mesh to show internal structures).
///
/// @par Example: clip everything behind Z=0
/// @code{.cpp}
/// RenderOptions opts;
/// opts.clip_planes.push_back(ClipPlane{Vec3(0, 0, -1), 0.0f});
/// // negates everything with Z < 0
/// @endcode
///
/// @see RenderOptions, RenderOptions::clip_planes
struct ClipPlane {
    /// @brief The plane normal vector (should be unit-length).
    ///
    /// Points toward the "keep" side of the plane.
    /// Default: (0, 0, -1), i.e., removing things behind Z=0.
    Vec3 normal = Vec3(0.0f, 0.0f, -1.0f);

    /// @brief Offset along the normal.
    ///
    /// A point `p` is kept when `dot(p, normal) + offset >= 0`.
    float offset = 0.0f;
};

} // namespace scimesh
