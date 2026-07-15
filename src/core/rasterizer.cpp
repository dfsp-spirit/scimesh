#include "rasterizer.h"
#include "math_utils.h"
#include <algorithm>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace scimesh {

Rasterizer::Rasterizer(int w, int h)
    : width(w), height(h), z_buffer(w * h, 1.0f), normal_buffer(w * h, Vec3(0.0f)) {}

void Rasterizer::clear(float clear_depth) {
    std::fill(z_buffer.begin(), z_buffer.end(), clear_depth);
    std::fill(normal_buffer.begin(), normal_buffer.end(), Vec3(0.0f));
}

void Rasterizer::shade_and_write(int x, int y, float depth,
                                 const Color &color, const Vec3 &normal,
                                 const Vec3 &light_direction, Image &output) {
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    int idx = y * width + x;
    if (blend_mode || depth < z_buffer[idx]) {
        Color shaded;
        if (lights.empty()) {
            shaded = shade_pixel(color, normal, light_direction,
                                 specular_color, shininess);
        } else {
            shaded = shade_pixel_multi(color, normal, lights, ambient,
                                       specular_color, shininess);
        }

        if (gamma != 1.0f) {
            float inv_gamma = 1.0f / gamma;
            shaded.r = std::pow(std::clamp(shaded.r, 0.0f, 1.0f), inv_gamma);
            shaded.g = std::pow(std::clamp(shaded.g, 0.0f, 1.0f), inv_gamma);
            shaded.b = std::pow(std::clamp(shaded.b, 0.0f, 1.0f), inv_gamma);
        }

        uint8_t r = static_cast<uint8_t>(std::clamp(shaded.r, 0.0f, 1.0f) * 255.0f);
        uint8_t g = static_cast<uint8_t>(std::clamp(shaded.g, 0.0f, 1.0f) * 255.0f);
        uint8_t b = static_cast<uint8_t>(std::clamp(shaded.b, 0.0f, 1.0f) * 255.0f);
        uint8_t a = static_cast<uint8_t>(std::clamp(shaded.a, 0.0f, 1.0f) * 255.0f);

        if (fog_enabled) {
            float fog_fac = (depth - fog_start) / (fog_end - fog_start);
            fog_fac = std::max(0.0f, std::min(1.0f, fog_fac));
            r = static_cast<uint8_t>(r * (1.0f - fog_fac) + fog_color.r * 255.0f * fog_fac);
            g = static_cast<uint8_t>(g * (1.0f - fog_fac) + fog_color.g * 255.0f * fog_fac);
            b = static_cast<uint8_t>(b * (1.0f - fog_fac) + fog_color.b * 255.0f * fog_fac);
            a = static_cast<uint8_t>(a * (1.0f - fog_fac) + fog_color.a * 255.0f * fog_fac);
        }

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
            normal_buffer[idx] = normal;
        }

        output.set_pixel(x, y, r, g, b, a);
    }
}

void Rasterizer::rasterize_triangle(
    const Vec3 &screen_v0, const Color &color0, const Vec3 &normal0, const Vec2 &uv0,
    const Vec3 &screen_v1, const Color &color1, const Vec3 &normal1, const Vec2 &uv1,
    const Vec3 &screen_v2, const Color &color2, const Vec3 &normal2, const Vec2 &uv2,
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

#ifdef _OPENMP
#pragma omp parallel for if((y_end - y_start) > 16) schedule(static)
#endif
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
                if (!blend_mode) {
                    z_buffer[pidx] = depth;
                    Vec3 wf_normal = w0 * normal0 + w1 * normal1 + w2 * normal2;
                    normal_buffer[pidx] = wf_normal;
                }
                continue;
            }

            float depth = w0 * screen_v0.z + w1 * screen_v1.z + w2 * screen_v2.z;

            Color base_color;
            Vec3 interp_normal;
            if (smooth_shading) {
                base_color = Color(
                    w0 * color0.r + w1 * color1.r + w2 * color2.r,
                    w0 * color0.g + w1 * color1.g + w2 * color2.g,
                    w0 * color0.b + w1 * color1.b + w2 * color2.b,
                    w0 * color0.a + w1 * color1.a + w2 * color2.a);
                interp_normal = w0 * normal0 + w1 * normal1 + w2 * normal2;
            } else {
                base_color = color0;
                interp_normal = normal0;
            }

            if (active_texture) {
                Vec2 interp_uv = smooth_shading
                    ? w0 * uv0 + w1 * uv1 + w2 * uv2
                    : uv0;
                Color tex = active_texture->sample_bilinear(interp_uv.x, interp_uv.y);
                base_color = Color(base_color.r * tex.r, base_color.g * tex.g,
                                   base_color.b * tex.b, base_color.a * tex.a);
            }

            shade_and_write(x, y, depth, base_color, interp_normal, light_direction, output);
        }
    }
}

void Rasterizer::rasterize_point(float screen_x, float screen_y, float depth,
                                 float radius, const Color &color,
                                 const Vec3 &normal, const Vec3 &light_direction,
                                 Image &output) {
    int cx = static_cast<int>(std::floor(screen_x));
    int cy = static_cast<int>(std::floor(screen_y));
    int r = static_cast<int>(std::ceil(radius));
    float r_sq = radius * radius;

    for (int dy = -r; dy <= r; ++dy) {
        int py = cy + dy;
        if (py < 0 || py >= height) continue;
        for (int dx = -r; dx <= r; ++dx) {
            int px = cx + dx;
            if (px < 0 || px >= width) continue;
            if (static_cast<float>(dx*dx + dy*dy) > r_sq) continue;
            shade_and_write(px, py, depth, color, normal, light_direction, output);
        }
    }
}


// You'll need to pass your camera's near and far clipping planes (e.g., 0.1f and 100.0f)
void Rasterizer::apply_ssao(Image &output, float z_near, float z_far) {
    if (!ssao_enabled || width < 2 || height < 2) return;

    // --- TUNABLE PARAMETERS (Now in linear world-units, e.g., meters) ---
    // How far a sample must jut out to cast a shadow (fixes ground plane acne)
    const float depth_bias = 0.05f;

    // Max distance before we assume it's a different object (fixes skybox/cow halo)
    const float max_occlusion_distance = 1.5f;

    // Multiplier to convert world-radius to screen pixels
    const float radius_scale = 10.0f;

    // Fixed normalized sample directions (8 samples on a spiral)
    const int ns = 8;
    const float dirs[ns][2] = {
        { 0.309f,  0.951f}, {-0.809f,  0.588f}, { 1.000f, -0.000f}, { 0.809f, -0.588f},
        {-0.309f, -0.951f}, { 0.588f,  0.809f}, {-0.588f,  0.809f}, {-1.000f, -0.000f},
    };

    // Precalculate linearization constants outside the loop to save CPU cycles
    const float depth_C = 2.0f * z_near * z_far;
    const float depth_D = z_far + z_near;
    const float depth_E = z_far - z_near;

    // Fast inline lambda for depth linearization
    auto linearize = [&](float raw_z) {
        float z_ndc = 2.0f * raw_z - 1.0f;
        return depth_C / (depth_D - z_ndc * depth_E);
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float center_depth_raw = z_buffer[idx];

            // Skip background pixels
            if (center_depth_raw >= 1.0f) continue;

            // Convert center depth to actual world units
            float center_depth = linearize(center_depth_raw);

            Vec3 center_normal = normal_buffer[idx];
            if (center_normal.x == 0.0f && center_normal.y == 0.0f && center_normal.z == 0.0f) continue;

            center_normal = glm::normalize(center_normal);

            // Scale the sampling radius based on distance (closer = bigger radius)
            int screen_radius = static_cast<int>((ssao_radius * radius_scale) / center_depth);
            screen_radius = std::max(1, std::min(screen_radius, 100)); // Clamp to sane bounds

            float occlusion = 0.0f;
            for (int s = 0; s < ns; ++s) {
                int sx = x + static_cast<int>(dirs[s][0] * screen_radius);
                int sy = y + static_cast<int>(dirs[s][1] * screen_radius);

                // Bounds check
                if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
                    continue;
                }

                float sample_depth_raw = z_buffer[sy * width + sx];
                float sample_depth = linearize(sample_depth_raw);

                // Delta: Positive means the sample is CLOSER to the camera than the center
                float depth_delta = center_depth - sample_depth;

                // 1. Bias Check: Is it actually jutting out, or is it just a tilted flat plane?
                if (depth_delta > depth_bias) {

                    // 2. Range Check: Smooth falloff to prevent halos across large gaps
                    float range_falloff = 1.0f - (depth_delta / max_occlusion_distance);

                    // Only add occlusion if it's within the max distance
                    if (range_falloff > 0.0f) {
                        // 3. Hemisphere check: only count occlusion from above the surface
                        float depth_ndc_delta = center_depth_raw - sample_depth_raw;
                        Vec3 offset_dir(
                            dirs[s][0] * screen_radius,
                            -dirs[s][1] * screen_radius,
                            -depth_ndc_delta * (width + height) * 0.1f);
                        offset_dir = glm::normalize(offset_dir);
                        float hem = glm::dot(center_normal, offset_dir);
                        if (hem > 0.0f) {
                            occlusion += range_falloff * hem;
                        }
                    }
                }
            }

            // Calculate final AO and apply intensity
            float ao = 1.0f - (ssao_intensity * (occlusion / static_cast<float>(ns)));
            ao = std::max(0.0f, std::min(1.0f, ao));

            // Apply to color buffer
            if (ao < 1.0f) {
                uint8_t r, g, b, a;
                output.get_pixel(x, y, r, g, b, a);
                r = static_cast<uint8_t>(r * ao);
                g = static_cast<uint8_t>(g * ao);
                b = static_cast<uint8_t>(b * ao);
                output.set_pixel(x, y, r, g, b, a);
            }
        }
    }
}
} // namespace scimesh
