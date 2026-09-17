#include <scimesh/rasterizer.h>
#include <scimesh/math_utils.h>
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

// ---- depth buffer <-> world units ------------------------------------------
//
// The rasterizer stores the normalized device depth of a fragment (the z of the
// clip-space position after the perspective divide, i.e. values in [-1, 1] with
// -1 at the near and +1 at the far plane) - see ndc_to_screen().  World-space
// fog and SSAO both need the distance from the camera in world units, so the
// inverse of the projection is applied here: linear for an orthographic
// projection, hyperbolic for a perspective one.
static float view_distance_from_ndc(float z_ndc, float z_near, float z_far,
                                    bool orthographic) {
    if (!(z_far > z_near) || z_near <= 0.0f) {
        // Degenerate/behind-the-camera projection: no meaningful mapping.
        return 0.0f;
    }
    if (orthographic) {
        // glm::ortho maps view z in [-n, -f] linearly onto [-1, 1]:
        //   z_ndc = -2/(f-n) * z_view - (f+n)/(f-n)
        // => distance from camera = -z_view = (z_ndc*(f-n) + f+n) / 2
        return (z_ndc * (z_far - z_near) + (z_far + z_near)) * 0.5f;
    }
    // glm::perspective maps 1/w linearly onto z_ndc, so inverting it gives the
    // exact view-space distance of the fragment:
    //   z_ndc = (f+n)/(f-n) - 2*n*f/((f-n) * d)
    // => d = 2*n*f / (f + n - z_ndc*(f-n))
    const float denom = (z_far + z_near) - z_ndc * (z_far - z_near);
    if (std::abs(denom) < 1e-12f) return z_far;
    return (2.0f * z_near * z_far) / denom;
}

float Rasterizer::fog_depth_from_ndc(float z_ndc) const {
    return view_distance_from_ndc(z_ndc, z_near, z_far, orthographic);
}

/// @brief Put the endpoints of a triangle edge into a canonical order, in place.
///
/// Two triangles that share an edge walk along it in opposite directions, so the
/// edge function of the *shared* edge is evaluated with the endpoints swapped.
/// The two evaluations are mathematically the negation of each other, but in
/// floating point they can differ in their last bits.  A pixel center that lies
/// exactly on the shared edge can then be rasterized by both triangles (double
/// covering, which blends translucency twice along the seam) or by neither (a
/// crack), and which of the two happens depends on the rounding of the platform -
/// the very same scene then renders differently on different CPUs/compilers.
///
/// Evaluating every edge function in a canonical endpoint order fixes that: all
/// triangles sharing the edge compute the *identical* value and only differ by
/// the sign that undoes the swap, which is exact.  The sign of the shared edge's
/// barycentric weight is therefore exactly opposite in the two triangles, so at
/// most one of them can contain a pixel that lies on the edge; the remaining tie
/// (weight exactly 0) is broken by that same sign, which turns it into the
/// "top-left" style fill rule applied in rasterize_triangle().
///
/// @param a, b Endpoints of the edge; swapped when they are out of order.
/// @param in_canonical_order True while (a, b) is in canonical order; toggled
///        on every swap.
static void canonical_edge_endpoints(Vec3 &a, Vec3 &b, bool &in_canonical_order) {
    if (b.x < a.x || (b.x == a.x && b.y < a.y)) {
        std::swap(a, b);
        in_canonical_order = !in_canonical_order;
    }
}

void Rasterizer::shade_and_write(int x, int y, float depth,
                                 const Color &color, const Vec3 &normal,
                                 const Vec3 &light_direction, Image &output,
                                 bool lit) {
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    int idx = y * width + x;
    // Translucent fragments never write the depth buffer (see below), so
    // `z_buffer` always holds the nearest *opaque* surface.  Testing against it
    // in the blended pass as well hides translucent geometry that lies behind
    // opaque geometry, while translucent geometry in front of it still blends;
    // the correct order among translucent fragments comes from the
    // back-to-front sort in Renderer::render_pipeline().
    if (depth < z_buffer[idx]) {
        Color shaded;
        if (!lit) {
            // Flat color, e.g. for line layers: skip the lighting calculation.
            shaded = color;
        } else if (lights.empty()) {
            shaded = shade_pixel(color, normal, light_direction,
                                 specular_color, shininess);
        } else {
            shaded = shade_pixel_multi(color, normal, lights, ambient,
                                       specular_color, shininess);
        }

        if (contrast != 1.0f) {
            auto apply_contrast = [&](float v) {
                return std::clamp((v - 0.5f) * contrast + 0.5f, 0.0f, 1.0f);
            };
            shaded.r = apply_contrast(shaded.r);
            shaded.g = apply_contrast(shaded.g);
            shaded.b = apply_contrast(shaded.b);
        }

        uint8_t r = static_cast<uint8_t>(std::clamp(shaded.r, 0.0f, 1.0f) * 255.0f);
        uint8_t g = static_cast<uint8_t>(std::clamp(shaded.g, 0.0f, 1.0f) * 255.0f);
        uint8_t b = static_cast<uint8_t>(std::clamp(shaded.b, 0.0f, 1.0f) * 255.0f);
        uint8_t a = static_cast<uint8_t>(std::clamp(shaded.a, 0.0f, 1.0f) * 255.0f);

        if (fog_enabled) {
            // World-space fog needs the fragment distance in world units;
            // NDC fog uses the raw depth-buffer value (legacy behaviour).
            float fog_depth = (fog_space == FogSpace::WORLD)
                                  ? fog_depth_from_ndc(depth)
                                  : depth;
            float span = fog_end - fog_start;
            float fog_fac;
            if (std::abs(span) < 1e-12f) {
                // Empty/degenerate range: hard step instead of dividing by 0.
                fog_fac = (fog_depth >= fog_start) ? 1.0f : 0.0f;
            } else {
                fog_fac = (fog_depth - fog_start) / span;
            }
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

    // Two-sided lighting.  The sign of the screen-space area is the winding
    // order, i.e. it tells which side of the triangle faces the viewer (a
    // positive area is the back-facing case that `backface_culling` drops
    // above).  A fragment whose back side is visible AND whose stored normal
    // points away from the viewer is shaded with that normal flipped towards the
    // camera, instead of falling back to the ambient term only (a dark, unlit
    // patch).  This matters for double-sided geometry - like generate_plane(),
    // whose front and back faces are exactly coplanar, so *which* of the two
    // wins the depth test is a floating point tie - and for open surfaces seen
    // from their back side.  Only the orientation of the *normal* is corrected:
    // a mesh whose normals disagree with its winding is left alone, so
    // RenderOptions::invert_normals keeps its effect.
    const bool back_facing = area > 0.0f;

    float min_x = std::min({screen_v0.x, screen_v1.x, screen_v2.x});
    float max_x = std::max({screen_v0.x, screen_v1.x, screen_v2.x});
    float min_y = std::min({screen_v0.y, screen_v1.y, screen_v2.y});
    float max_y = std::max({screen_v0.y, screen_v1.y, screen_v2.y});

    int x_start = std::max(0, static_cast<int>(std::floor(min_x)));
    int x_end   = std::min(width - 1, static_cast<int>(std::ceil(max_x)));
    int y_start = std::max(0, static_cast<int>(std::floor(min_y)));
    int y_end   = std::min(height - 1, static_cast<int>(std::ceil(max_y)));

    float inv_area = 1.0f / area;

    // The three edge functions, evaluated in a canonical endpoint order (see
    // canonical_edge_endpoints()).  `own<i>` states whether the triangle walks
    // along the edge opposite vertex <i> in the canonical direction: it undoes
    // the canonicalization in the barycentric weight (`k<i>`), and it decides
    // the fill rule for a pixel that lies exactly on that edge.
    Vec3 e0_a = screen_v1, e0_b = screen_v2; bool own0 = true;
    Vec3 e1_a = screen_v2, e1_b = screen_v0; bool own1 = true;
    Vec3 e2_a = screen_v0, e2_b = screen_v1; bool own2 = true;
    canonical_edge_endpoints(e0_a, e0_b, own0);
    canonical_edge_endpoints(e1_a, e1_b, own1);
    canonical_edge_endpoints(e2_a, e2_b, own2);
    const float k0 = own0 ? inv_area : -inv_area;
    const float k1 = own1 ? inv_area : -inv_area;
    const float k2 = own2 ? inv_area : -inv_area;

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

            // Barycentric weights, one per vertex: w0 belongs to screen_v0 and
            // is the normalized edge function of the opposite edge (v1 -> v2),
            // and so on.  Each weight is derived from its own edge function (and
            // not as 1 - w0 - w1), because only an edge function gives two
            // triangles that share the edge the same value - and hence exactly
            // opposite signs, see canonical_edge_endpoints().  The weights
            // therefore sum to 1 only up to a few ULP, which is irrelevant for
            // the interpolation.
            float w0 = k0 * ((e0_a.x - px) * (e0_b.y - py) -
                             (e0_b.x - px) * (e0_a.y - py));
            float w1 = k1 * ((e1_a.x - px) * (e1_b.y - py) -
                             (e1_b.x - px) * (e1_a.y - py));
            float w2 = k2 * ((e2_a.x - px) * (e2_b.y - py) -
                             (e2_b.x - px) * (e2_a.y - py));

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                continue;

            // Fill rule: a pixel lying exactly on an edge is rasterized by the
            // triangle that walks along that edge in the canonical direction, so
            // that exactly one of the triangles sharing the edge covers it.
            if (w0 == 0.0f && !own0)
                continue;
            if (w1 == 0.0f && !own1)
                continue;
            if (w2 == 0.0f && !own2)
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
                    if (back_facing && wf_normal.z < 0.0f) wf_normal = -wf_normal;
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

            // Shading normal of a visible back side (see the two-sided lighting
            // note above the pixel loop).
            if (back_facing && interp_normal.z < 0.0f) {
                interp_normal = -interp_normal;
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

void Rasterizer::rasterize_line(const Vec3 &screen_v0, const Color &color0,
                                const Vec3 &screen_v1, const Color &color1,
                                float width, bool lit, const Vec3 &normal,
                                const Vec3 &light_direction, Image &output) {
    const float dx = screen_v1.x - screen_v0.x;
    const float dy = screen_v1.y - screen_v0.y;

    // NOTE: the parameter `width` (line width in pixels) shadows the
    // Rasterizer::width member (image width), so the members have to be
    // qualified explicitly in the bounds checks below.
    const int img_width = this->width;
    const int img_height = this->height;

    // Walk along the dominant screen axis, one sample per device pixel, so that
    // the stamps of consecutive samples overlap without gaps.
    const float span = std::max(std::abs(dx), std::abs(dy));
    const int steps = std::max(1, static_cast<int>(std::ceil(span)));

    // The stamp is a disc, like rasterize_point().  `half_extent` is how many
    // whole pixels the stamp reaches from the sample point, `r_sq` is the
    // squared stamp radius used for the coverage test.
    const float stamp_radius = std::max(0.5f, width * 0.5f);
    const int half_extent = static_cast<int>(std::ceil(stamp_radius - 0.5f));
    const float r_sq = stamp_radius * stamp_radius;

    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float sx = screen_v0.x + t * dx;
        const float sy = screen_v0.y + t * dy;
        const float depth = screen_v0.z + t * (screen_v1.z - screen_v0.z);
        const Color c(color0.r + t * (color1.r - color0.r),
                      color0.g + t * (color1.g - color0.g),
                      color0.b + t * (color1.b - color0.b),
                      color0.a + t * (color1.a - color0.a));
        const int cx = static_cast<int>(std::lround(sx));
        const int cy = static_cast<int>(std::lround(sy));

        for (int py = cy - half_extent; py <= cy + half_extent; ++py) {
            if (py < 0 || py >= img_height) {
                continue;
            }
            for (int px = cx - half_extent; px <= cx + half_extent; ++px) {
                if (px < 0 || px >= img_width) {
                    continue;
                }
                const float ox = static_cast<float>(px - cx);
                const float oy = static_cast<float>(py - cy);
                if (ox * ox + oy * oy > r_sq) {
                    continue;
                }
                shade_and_write(px, py, depth, c, normal, light_direction,
                                output, lit);
            }
        }
    }
}


// Screen-space ambient occlusion.  The depth buffer holds NDC z in [-1, 1]
// (see view_distance_from_ndc() above); pass the camera's near and far clipping
// planes, and set `orthographic` for a parallel projection.
void Rasterizer::apply_ssao(Image &output, float z_near, float z_far) {
    if (!ssao_enabled || width < 2 || height < 2) return;

    // --- TUNABLE PARAMETERS (in linear world units, e.g. meters) ---
    // How far a sample must jut out to cast a shadow (fixes ground plane acne)
    const float depth_bias = 0.05f;

    // Max distance before we assume it's a different object (fixes skybox/cow halo)
    const float max_occlusion_distance = 1.5f;

    // Fixed normalized sample directions (8 samples on a spiral)
    const int ns = 8;
    const float dirs[ns][2] = {
        { 0.309f,  0.951f}, {-0.809f,  0.588f}, { 1.000f, -0.000f}, { 0.809f, -0.588f},
        {-0.309f, -0.951f}, { 0.588f,  0.809f}, {-0.588f,  0.809f}, {-1.000f, -0.000f},
    };

    // Fast inline lambda for depth linearization.  The depth buffer holds NDC z
    // ([-1, 1]); view_distance_from_ndc() inverts the projection (perspective or
    // orthographic) to world units.
    auto linearize = [&](float raw_z) {
        return view_distance_from_ndc(raw_z, z_near, z_far, orthographic);
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

            // `ssao_radius` is documented as a screen-space radius in pixels.
            int screen_radius = static_cast<int>(std::lround(ssao_radius));
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
                        // 3. Hemisphere check: only count samples that lie on the
                        // side of the surface the normal points to.  The offset
                        // is built in a common scale: x/y as screen pixels, z from
                        // the world-unit depth difference (a closer sample has a
                        // positive delta, i.e. it sits on the camera side).  The
                        // normal comes from the normal buffer and is in view space.
                        Vec3 offset_dir(
                            dirs[s][0] * screen_radius,
                            -dirs[s][1] * screen_radius,
                            depth_delta * (width + height) * 0.1f);
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
