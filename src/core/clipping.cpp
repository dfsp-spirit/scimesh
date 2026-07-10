#include "clipping.h"
#include <algorithm>

namespace scimesh {

// In OpenGL clip space (used by GLM), the near plane is at z = -w.
// A vertex is "inside" (in front of near plane) if z + w >= 0.
static inline bool is_inside_near(const Vec4 &p) {
    return p.z + p.w >= 0.0f;
}

// Linear interpolation between two clip vertices based on parameter t in [0, 1]
static inline ClipVertex lerp_clip_vertex(const ClipVertex &a, const ClipVertex &b, float t) {
    ClipVertex result;
    result.position = a.position + t * (b.position - a.position);
    result.color = Color(
        a.color.r + t * (b.color.r - a.color.r),
        a.color.g + t * (b.color.g - a.color.g),
        a.color.b + t * (b.color.b - a.color.b),
        a.color.a + t * (b.color.a - a.color.a));
    result.normal = a.normal + t * (b.normal - a.normal);
    return result;
}

// Compute intersection parameter where edge a->b crosses the near plane (z + w = 0)
static inline float intersection_t(const Vec4 &a, const Vec4 &b) {
    float dist_a = a.z + a.w;
    float dist_b = b.z + b.w;
    float denom = dist_a - dist_b;
    if (std::abs(denom) < 1e-12f)
        return 0.0f;
    return dist_a / denom;
}

// Sutherland-Hodgman clipping of a polygon against the near plane.
// Input: list of polygon vertices (counter-clockwise). Output: clipped polygon.
static void clip_polygon_near_plane(
    const std::vector<ClipVertex> &input,
    std::vector<ClipVertex> &output) {
    output.clear();
    if (input.empty())
        return;

    int n = static_cast<int>(input.size());
    for (int i = 0; i < n; ++i) {
        const ClipVertex &curr = input[i];
        const ClipVertex &next = input[(i + 1) % n];
        bool curr_inside = is_inside_near(curr.position);
        bool next_inside = is_inside_near(next.position);

        if (curr_inside) {
            output.push_back(curr);
            if (!next_inside) {
                float t = intersection_t(curr.position, next.position);
                output.push_back(lerp_clip_vertex(curr, next, t));
            }
        } else {
            if (next_inside) {
                float t = intersection_t(curr.position, next.position);
                output.push_back(lerp_clip_vertex(curr, next, t));
            }
        }
    }
}

int clip_triangle_near_plane(
    const ClipVertex &v0,
    const ClipVertex &v1,
    const ClipVertex &v2,
    std::vector<ClipVertex> &output_vertices,
    std::vector<Triangle> &output_triangles) {

    // First pass: clip the triangle as a polygon
    std::vector<ClipVertex> polygon = {v0, v1, v2};
    std::vector<ClipVertex> clipped;
    clip_polygon_near_plane(polygon, clipped);

    if (clipped.size() < 3)
        return 0;

    // Fan triangulation from vertex 0
    uint32_t base_idx = static_cast<uint32_t>(output_vertices.size());
    for (const auto &cv : clipped) {
        output_vertices.push_back(cv);
    }

    int num_tris = static_cast<int>(clipped.size()) - 2;
    for (int i = 0; i < num_tris; ++i) {
        output_triangles.push_back(Triangle{
            base_idx,
            base_idx + i + 1,
            base_idx + i + 2});
    }
    return num_tris;
}

} // namespace scimesh
