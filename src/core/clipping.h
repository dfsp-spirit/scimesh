#pragma once

#include "types.h"
#include <vector>

namespace scimesh {

// Vertex in clip space (after view-projection transform, before perspective divide)
struct ClipVertex {
    Vec4 position;
    Color color;
    Vec3 normal;
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

} // namespace scimesh
