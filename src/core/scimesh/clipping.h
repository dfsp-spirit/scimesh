/// @file clipping.h
/// @brief Triangle clipping against planes (view frustum and clip planes).
///
/// Clipping ensures that only the visible portion of triangles that cross
/// the view frustum boundaries or user-specified clip planes is drawn.
/// Without clipping, triangles that extend outside the view would produce
/// visual artifacts.

#pragma once

#include <scimesh/types.h>
#include <scimesh/math_utils.h>
#include <vector>

namespace scimesh {

/// @brief A processed vertex in clip space, ready for perspective divide.
///
/// Used internally by the clipping functions.  Holds the full set of
/// per-vertex attributes needed after clipping.
struct ClipVertex {
    Vec4 position;  ///< Homogeneous clip-space position (before divide by w).
    Color color;    ///< Vertex color.
    Vec3 normal;    ///< Vertex normal (in view space).
    Vec2 uv;        ///< Texture coordinates.
};

/// @brief Clip a triangle against the near clipping plane in clip space.
///
/// This is part of the rendering pipeline — triangles that cross the near
/// plane are split so that only the visible portion (w > 0) is kept.
///
/// @param v0, v1, v2               Three vertices of the input triangle
///                                 (in homogeneous clip space).
/// @param[out] output_vertices     Clipped vertices are appended here.
/// @param[out] output_triangles    Resulting triangle indices (0, 1, or 2
///                                 triangles) are appended here.
/// @return Number of output triangles (0, 1, or 2).  0 means the triangle
///         is entirely behind the near plane and should be discarded.
///
/// @see clip_triangle_view_plane(), Renderer::render_pipeline()
int clip_triangle_near_plane(
    const ClipVertex &v0,
    const ClipVertex &v1,
    const ClipVertex &v2,
    std::vector<ClipVertex> &output_vertices,
    std::vector<Triangle> &output_triangles);

/// @brief Convert a ClipPlane to view (eye) space, ready for clipping.
///
/// Clipping always happens in view space, so user clip planes have to be
/// converted first.  The conversion is a no-op for `PlaneSpace::EYE` planes
/// (apart from normalizing the normal); for `PlaneSpace::WORLD` planes (the
/// default) the plane constant is shifted so that the plane stays fixed in
/// world space instead of travelling with the camera:
///
/// @code
/// world: dot(n, p_world) + offset >= 0
///        with p_world = R^T * p_view + eye  (view is rigid)
///     => dot(R * n, p_view) + (dot(n, eye) + offset) >= 0
/// @endcode
///
/// The returned plane has a unit-length normal, and its `offset` is a signed
/// distance in world units; `space` is copied from the input plane.
///
/// A plane with a zero-length normal is neutralized (it is turned into a
/// plane that keeps everything) so that a malformed plane can never blank
/// out a render silently.
///
/// @param plane The clip plane, in world or eye space (see ClipPlane::space).
/// @param eye   The camera position in world space (Camera::eye).
/// @param view  The world-to-view matrix (Camera::get_view_matrix()).
/// @return The same plane expressed in view space.
///
/// @par Example
/// @code{.cpp}
/// Camera cam;
/// Mat4 view = cam.get_view_matrix();
/// // World-space cut at x = 0; still cuts at x = 0 in view space.
/// ClipPlane world_plane{Vec3(-1, 0, 0), 0.0f};
/// ClipPlane view_plane = clip_plane_to_view_space(world_plane, cam.eye, view);
/// @endcode
///
/// @see ClipPlane, PlaneSpace, clip_triangle_view_plane()
inline ClipPlane clip_plane_to_view_space(const ClipPlane &plane,
                                          const Vec3 &eye,
                                          const Mat4 &view) {
    ClipPlane out;
    out.space = plane.space;

    float len = glm::length(plane.normal);
    if (len < 1e-12f) {
        // Degenerate normal: keep everything (no clipping) instead of
        // discarding the whole scene.
        out.normal = Vec3(0.0f, 0.0f, 0.0f);
        out.offset = 0.0f;
        return out;
    }

    Vec3 n = plane.normal / len;
    out.normal = transform_direction(view, n);
    if (plane.space == PlaneSpace::WORLD) {
        // Shift the plane constant so that the cut stays at its world
        // position (see the derivation above).
        out.offset = glm::dot(n, eye) + plane.offset;
    } else {
        out.offset = plane.offset;
    }
    return out;
}

/// @brief Clip a triangle against an arbitrary plane in view space.
///
/// This is used for user-specified clip planes (see ClipPlane).  The plane
/// must already be in view space — use clip_plane_to_view_space() to convert
/// world-space planes first.
/// A vertex is considered "inside" (kept) when:
/// `dot(view_pos, plane.normal) + plane.offset >= 0`.
///
/// @param v0, v1, v2      Triangle vertex positions in **view space**.
/// @param n0, n1, n2      Per-vertex normals (view space).
/// @param c0, c1, c2      Per-vertex colors.
/// @param uv0, uv1, uv2   Per-vertex texture coordinates.
/// @param plane            The clipping plane.
/// @param[out] output_vertices   Clipped vertices (positions are in view
///                               space — caller must transform to clip space).
/// @param[out] output_triangles  Resulting triangle indices.
/// @return Number of output triangles (0, 1, or 2).
///
/// @see clip_plane_to_view_space(), clip_triangle_near_plane(), ClipPlane,
///      RenderOptions::clip_planes
int clip_triangle_view_plane(
    const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
    const Vec3 &n0, const Vec3 &n1, const Vec3 &n2,
    const Color &c0, const Color &c1, const Color &c2,
    const Vec2 &uv0, const Vec2 &uv1, const Vec2 &uv2,
    const ClipPlane &plane,
    std::vector<ClipVertex> &output_vertices,
    std::vector<Triangle> &output_triangles);

} // namespace scimesh
