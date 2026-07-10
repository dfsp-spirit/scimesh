#include "rasterizer.h"
#include "math_utils.h"
#include <algorithm>
#include <cmath>

namespace scimesh {

Rasterizer::Rasterizer(int w, int h)
    : width(w), height(h), z_buffer(w * h, 1.0f) {}

void Rasterizer::clear(float clear_depth) {
    std::fill(z_buffer.begin(), z_buffer.end(), clear_depth);
}

void Rasterizer::shade_and_write(int x, int y, float depth,
                                 const Color &color, const Vec3 &normal,
                                 const Vec3 &light_direction, Image &output) {
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    int idx = y * width + x;
    if (depth < z_buffer[idx]) {
        z_buffer[idx] = depth;
        Color shaded = shade_pixel(color, normal, light_direction);
        output.set_pixel(x, y,
                         static_cast<uint8_t>(std::clamp(shaded.r, 0.0f, 1.0f) * 255.0f),
                         static_cast<uint8_t>(std::clamp(shaded.g, 0.0f, 1.0f) * 255.0f),
                         static_cast<uint8_t>(std::clamp(shaded.b, 0.0f, 1.0f) * 255.0f),
                         static_cast<uint8_t>(std::clamp(shaded.a, 0.0f, 1.0f) * 255.0f));
    }
}

void Rasterizer::rasterize_triangle(
    const Vec3 &screen_v0, const Color &color0, const Vec3 &normal0,
    const Vec3 &screen_v1, const Color &color1, const Vec3 &normal1,
    const Vec3 &screen_v2, const Color &color2, const Vec3 &normal2,
    bool backface_culling,
    bool smooth_shading,
    const Vec3 &light_direction,
    Image &output) {

    // Backface culling: compute signed area in screen space.
    // Screen space has Y pointing down (row 0 at top). ndc_to_screen flips Y from
    // NDC (where Y points up), so a CCW (front-facing) triangle in NDC becomes a
    // triangle with NEGATIVE signed area in screen space: area_screen = -k * area_ndc.
    // Therefore front-facing triangles have area < 0 here; back-facing have area > 0.
    float area = (screen_v1.x - screen_v0.x) * (screen_v2.y - screen_v0.y) -
                (screen_v2.x - screen_v0.x) * (screen_v1.y - screen_v0.y);

    if (backface_culling && area > 0.0f)
        return;

    // Skip degenerate triangles
    if (std::abs(area) < 1e-12f)
        return;

    // Bounding box (clamped to viewport)
    float min_x = std::min({screen_v0.x, screen_v1.x, screen_v2.x});
    float max_x = std::max({screen_v0.x, screen_v1.x, screen_v2.x});
    float min_y = std::min({screen_v0.y, screen_v1.y, screen_v2.y});
    float max_y = std::max({screen_v0.y, screen_v1.y, screen_v2.y});

    int x_start = std::max(0, static_cast<int>(std::floor(min_x)));
    int x_end = std::min(width - 1, static_cast<int>(std::ceil(max_x)));
    int y_start = std::max(0, static_cast<int>(std::floor(min_y)));
    int y_end = std::min(height - 1, static_cast<int>(std::ceil(max_y)));

    // Use the inverse of the signed area for barycentric computation
    float inv_area = 1.0f / area;

    for (int y = y_start; y <= y_end; ++y) {
        for (int x = x_start; x <= x_end; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float py = static_cast<float>(y) + 0.5f;

            // Barycentric coordinates using the signed area method
            float w0 = ((screen_v1.x - px) * (screen_v2.y - py) -
                        (screen_v2.x - px) * (screen_v1.y - py)) * inv_area;
            float w1 = ((screen_v2.x - px) * (screen_v0.y - py) -
                        (screen_v0.x - px) * (screen_v2.y - py)) * inv_area;
            float w2 = 1.0f - w0 - w1;

            // Inside test: all barycentric coords must be in [0, 1]
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                continue;

            // Interpolate depth
            float depth = w0 * screen_v0.z + w1 * screen_v1.z + w2 * screen_v2.z;

            if (smooth_shading) {
                // Interpolate color and normal, then shade
                Color interp_color(
                    w0 * color0.r + w1 * color1.r + w2 * color2.r,
                    w0 * color0.g + w1 * color1.g + w2 * color2.g,
                    w0 * color0.b + w1 * color1.b + w2 * color2.b,
                    w0 * color0.a + w1 * color1.a + w2 * color2.a);
                Vec3 interp_normal = w0 * normal0 + w1 * normal1 + w2 * normal2;
                shade_and_write(x, y, depth, interp_color, interp_normal, light_direction, output);
            } else {
                // Flat shading: use face normal (average of vertex normals or first vertex)
                Vec3 face_normal = normal0; // Caller should provide face normal for flat mode
                Color face_color = color0;
                shade_and_write(x, y, depth, face_color, face_normal, light_direction, output);
            }
        }
    }
}

} // namespace scimesh
