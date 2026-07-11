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
    if (blend_mode || depth < z_buffer[idx]) {
        Color shaded = shade_pixel(color, normal, light_direction,
                                   specular_color, shininess);

        uint8_t r = static_cast<uint8_t>(std::clamp(shaded.r, 0.0f, 1.0f) * 255.0f);
        uint8_t g = static_cast<uint8_t>(std::clamp(shaded.g, 0.0f, 1.0f) * 255.0f);
        uint8_t b = static_cast<uint8_t>(std::clamp(shaded.b, 0.0f, 1.0f) * 255.0f);
        uint8_t a = static_cast<uint8_t>(std::clamp(shaded.a, 0.0f, 1.0f) * 255.0f);

        if (blend_mode) {
            uint8_t dr, dg, db, da;
            output.get_pixel(x, y, dr, dg, db, da);
            float src_a = a / 255.0f;
            float inv_a = 1.0f - src_a;
            r = static_cast<uint8_t>(r * src_a + dr * inv_a);
            g = static_cast<uint8_t>(g * src_a + dg * inv_a);
            b = static_cast<uint8_t>(b * src_a + db * inv_a);
            a = static_cast<uint8_t>(a + da * inv_a);
        } else {
            z_buffer[idx] = depth;
        }

        output.set_pixel(x, y, r, g, b, a);
    }
}

void Rasterizer::rasterize_triangle(
    const Vec3 &screen_v0, const Color &color0, const Vec3 &normal0,
    const Vec3 &screen_v1, const Color &color1, const Vec3 &normal1,
    const Vec3 &screen_v2, const Color &color2, const Vec3 &normal2,
    bool backface_culling,
    bool smooth_shading,
    const Vec3 &light_direction,
    bool wireframe,
    const Color &wireframe_color,
    Image &output) {

    float area = (screen_v1.x - screen_v0.x) * (screen_v2.y - screen_v0.y) -
                (screen_v2.x - screen_v0.x) * (screen_v1.y - screen_v0.y);

    if (backface_culling && area > 0.0f)
        return;

    if (std::abs(area) < 1e-12f)
        return;

    float abs_area = std::abs(area);

    float min_x = std::min({screen_v0.x, screen_v1.x, screen_v2.x});
    float max_x = std::max({screen_v0.x, screen_v1.x, screen_v2.x});
    float min_y = std::min({screen_v0.y, screen_v1.y, screen_v2.y});
    float max_y = std::max({screen_v0.y, screen_v1.y, screen_v2.y});

    int x_start = std::max(0, static_cast<int>(std::floor(min_x)));
    int x_end   = std::min(width - 1, static_cast<int>(std::ceil(max_x)));
    int y_start = std::max(0, static_cast<int>(std::floor(min_y)));
    int y_end   = std::min(height - 1, static_cast<int>(std::ceil(max_y)));

    float inv_area = 1.0f / area;

    float wire_thresh = 0.0f;
    if (wireframe) {
        wire_thresh = 1.5f / std::sqrt(abs_area > 1e-9f ? abs_area : 1.0f);
        if (wire_thresh > 0.5f) wire_thresh = 0.5f;
    }

    for (int y = y_start; y <= y_end; ++y) {
        for (int x = x_start; x <= x_end; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float py = static_cast<float>(y) + 0.5f;

            float w0 = ((screen_v1.x - px) * (screen_v2.y - py) -
                        (screen_v2.x - px) * (screen_v1.y - py)) * inv_area;
            float w1 = ((screen_v2.x - px) * (screen_v0.y - py) -
                        (screen_v0.x - px) * (screen_v2.y - py)) * inv_area;
            float w2 = 1.0f - w0 - w1;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                continue;

            if (wireframe) {
                if (w0 >= wire_thresh && w1 >= wire_thresh && w2 >= wire_thresh)
                    continue;
                float depth = w0 * screen_v0.z + w1 * screen_v1.z + w2 * screen_v2.z;
                int pidx = y * width + x;
                output.set_pixel(x, y,
                    static_cast<uint8_t>(std::clamp(wireframe_color.r, 0.0f, 1.0f) * 255.0f),
                    static_cast<uint8_t>(std::clamp(wireframe_color.g, 0.0f, 1.0f) * 255.0f),
                    static_cast<uint8_t>(std::clamp(wireframe_color.b, 0.0f, 1.0f) * 255.0f),
                    static_cast<uint8_t>(std::clamp(wireframe_color.a, 0.0f, 1.0f) * 255.0f));
                if (!blend_mode) z_buffer[pidx] = depth;
                continue;
            }

            float depth = w0 * screen_v0.z + w1 * screen_v1.z + w2 * screen_v2.z;

            if (smooth_shading) {
                Color interp_color(
                    w0 * color0.r + w1 * color1.r + w2 * color2.r,
                    w0 * color0.g + w1 * color1.g + w2 * color2.g,
                    w0 * color0.b + w1 * color1.b + w2 * color2.b,
                    w0 * color0.a + w1 * color1.a + w2 * color2.a);
                Vec3 interp_normal = w0 * normal0 + w1 * normal1 + w2 * normal2;
                shade_and_write(x, y, depth, interp_color, interp_normal, light_direction, output);
            } else {
                shade_and_write(x, y, depth, color0, normal0, light_direction, output);
            }
        }
    }
}

} // namespace scimesh
