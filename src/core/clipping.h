#pragma once

#include "types.h"
#include <vector>

namespace scimesh {

// Vertex in clip space (after view-projection transform, before perspective divide)
struct ClipVertex {
    Vec4 position;
    Color color;
    Vec3 normal;
    Vec2 uv;
};

// Clip a triangle against the near plane in clip space.
// Returns number of output triangles (0, 1, or 2).
// Appends vertices to output_vertices and triangles to output_triangles.
int clip_triangle_near_plane(
    const ClipVertex &v0,
    const ClipVertex &v1,
    const ClipVertex &v2,
    std::vector<ClipVertex> &output_vertices,
    std::vector<Triangle> &output_triangles);

// Clip a triangle against an arbitrary plane in view space.
// A vertex is "inside" if dot(view_pos, plane.normal) + plane.offset >= 0.
// Returns number of output triangles (0, 1, or 2).
// Output vertices have POSITION in VIEW space; caller must transform to
// clip space.
int clip_triangle_view_plane(
    const Vec3 &v0, const Vec3 &v1, const Vec3 &v2,
    const Vec3 &n0, const Vec3 &n1, const Vec3 &n2,
    const Color &c0, const Color &c1, const Color &c2,
    const Vec2 &uv0, const Vec2 &uv1, const Vec2 &uv2,
    const ClipPlane &plane,
    std::vector<ClipVertex> &output_vertices,
    std::vector<Triangle> &output_triangles);

} // namespace scimesh
