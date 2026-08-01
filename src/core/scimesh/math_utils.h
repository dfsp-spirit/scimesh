/// @file math_utils.h
/// @brief Low-level math utilities for the rendering pipeline.
///
/// These are inline helper functions used throughout the rasterizer and
/// pipeline.  Most users won't call these directly, but they can be useful
/// for custom rendering or debugging.

#pragma once

#include <scimesh/types.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace scimesh {

// ---------------------------------------------------------------------------
//  Geometry
// ---------------------------------------------------------------------------

/// @brief Compute the unit-length normal vector of a triangle face.
///
/// Uses the cross product of two edges.  The normal points according to
/// the right-hand rule: if vertices are ordered counter-clockwise when
/// viewed from the front, the normal points toward the viewer.
///
/// @param v0, v1, v2 The three triangle vertex positions.
/// @return A unit-length Vec3 perpendicular to the triangle.
///         Returns (0, 0, 1) for degenerate (zero-area) triangles.
///
/// @par Example
/// @code{.cpp}
/// Vec3 n = compute_face_normal({0,0,0}, {1,0,0}, {0,1,0});
/// // n = (0, 0, 1) — pointing out of the XY plane
/// @endcode
///
/// @see compute_vertex_normals()
inline Vec3 compute_face_normal(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2) {
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 normal = glm::cross(edge1, edge2);
    float len = glm::length(normal);
    if (len < 1e-12f)
        return Vec3(0.0f, 0.0f, 1.0f);
    return normal / len;
}

// ---------------------------------------------------------------------------
//  Transform helpers
// ---------------------------------------------------------------------------

/// @brief Transform a point by a 4×4 matrix (with implicit w=1).
///
/// Equivalent to `(M * vec4(p, 1)).xyz`.  Use for transforming positions.
///
/// @param m The transformation matrix.
/// @param p The input point.
/// @return The transformed point.
///
/// @see transform_direction(), transform_point_homogeneous()
inline Vec3 transform_point(const Mat4 &m, const Vec3 &p) {
    Vec4 result = m * Vec4(p, 1.0f);
    return Vec3(result);
}

/// @brief Transform a point by a 4×4 matrix, returning the full Vec4 result.
///
/// Unlike transform_point(), this returns the homogeneous result (including
/// the w component), which is needed for perspective division.
///
/// @param m The transformation matrix.
/// @param p The input point.
/// @return The homogeneous transformed point (Vec4).
///
/// @see transform_point(), perspective_divide()
inline Vec4 transform_point_homogeneous(const Mat4 &m, const Vec3 &p) {
    return m * Vec4(p, 1.0f);
}

/// @brief Transform a direction vector by a 4×4 matrix (with implicit w=0).
///
/// Direction vectors use w=0 so that translation does not affect them —
/// only rotation and scale are applied.  Use for transforming normals
/// and light directions.
///
/// @param m The transformation matrix.
/// @param d The input direction.
/// @return The transformed direction.
///
/// @see transform_point()
inline Vec3 transform_direction(const Mat4 &m, const Vec3 &d) {
    Vec4 result = m * Vec4(d, 0.0f);
    return Vec3(result);
}

/// @brief Perform perspective division: divide xyz by w.
///
/// Converts from homogeneous clip space to normalized device coordinates
/// (NDC).  If w is near zero (the point is at the camera plane), the
/// result is clamped to avoid division by zero.
///
/// @param clip A point in homogeneous clip space.
/// @return The point after division by w.
///
/// @see transform_point_homogeneous(), ndc_to_screen()
inline Vec3 perspective_divide(const Vec4 &clip) {
    if (std::abs(clip.w) < 1e-12f)
        return Vec3(clip.x, clip.y, clip.z);
    return Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
}

/// @brief Convert from normalized device coordinates (NDC) to screen
///        (pixel) coordinates.
///
/// NDC space is a cube from (-1,-1,-1) to (1,1,1).  This maps it to
/// pixel coordinates where (0,0) is the top-left corner.
///
/// @param ndc          Input NDC coordinates.
/// @param width        Screen width in pixels.
/// @param height       Screen height in pixels.
/// @param[out] screen_x Output X pixel coordinate.
/// @param[out] screen_y Output Y pixel coordinate (0 = top).
/// @param[out] depth    Output depth value (passed through from NDC z).
///
/// @see perspective_divide()
inline void ndc_to_screen(const Vec3 &ndc, int width, int height,
                          float &screen_x, float &screen_y, float &depth) {
    screen_x = (ndc.x + 1.0f) * 0.5f * static_cast<float>(width);
    screen_y = (1.0f - ndc.y) * 0.5f * static_cast<float>(height);
    depth = ndc.z;
}

// ---------------------------------------------------------------------------
//  Barycentric coordinates
// ---------------------------------------------------------------------------

/// @brief Compute barycentric coordinates (u, v, w) of a point in a triangle.
///
/// Barycentric coordinates tell you how much each vertex contributes to a
/// point inside the triangle.  They are used for interpolating colors,
/// normals, and depth across the triangle surface.
///
/// - `u + v + w = 1.0`
/// - All three are in `[0, 1]` if and only if the point is inside the
///   triangle.
///
/// @param px, py               The query point (2D screen coords).
/// @param x0,y0, x1,y1, x2,y2  Triangle vertices (2D screen coords).
/// @param[out] u, v, w         Output barycentric weights.
///
/// @par Example: checking if a point is inside a triangle
/// @code{.cpp}
/// float u, v, w;
/// compute_barycentric(px, py, x0, y0, x1, y1, x2, y2, u, v, w);
/// if (u >= 0 && v >= 0 && w >= 0) {
///     // point is inside the triangle
/// }
/// @endcode
inline void compute_barycentric(float px, float py,
                                float x0, float y0,
                                float x1, float y1,
                                float x2, float y2,
                                float &u, float &v, float &w) {
    float det = (y2 - y0) * (x1 - x0) - (x2 - x0) * (y1 - y0);
    if (std::abs(det) < 1e-12f) {
        u = v = w = 0.0f;
        return;
    }
    float inv_det = 1.0f / det;
    u = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * inv_det;
    v = ((y2 - y0) * (px - x2) - (x2 - x0) * (py - y2)) * inv_det;
    w = 1.0f - u - v;
}

// ---------------------------------------------------------------------------
//  Shading
// ---------------------------------------------------------------------------

/// @brief Compute the shaded color of a pixel with a single directional light.
///
/// Uses the Blinn-Phong reflection model with ambient and diffuse terms.
/// The ambient term prevents completely black shadows.
///
/// @param base_color      The surface (unlit) color.
/// @param normal          Surface normal at this pixel (should be unit-length).
/// @param light_direction Direction TO the light (should be unit-length).
/// @param specular_color  Specular highlight color (transparent = no specular).
/// @param shininess       Shininess exponent (higher = sharper highlights).
/// @return The final lit color.
///
/// @see shade_pixel_multi(), Light
inline Color shade_pixel(const Color &base_color, const Vec3 &normal,
                         const Vec3 &light_direction,
                         const Color &specular_color = Color(0, 0, 0, 0),
                         float shininess = 0.0f) {
    Vec3 n = glm::length(normal) > 1e-12f ? glm::normalize(normal) : Vec3(0.0f, 0.0f, 1.0f);
    Vec3 l = glm::normalize(light_direction);
    float ndotl = std::max(0.0f, glm::dot(n, l));
    float ambient = 0.3f;
    float diffuse_term = (1.0f - ambient) * ndotl;
    float intensity = ambient + diffuse_term;

    float spec_r = 0.0f, spec_g = 0.0f, spec_b = 0.0f;
    if (shininess > 0.0f && ndotl > 0.0f) {
        float spec = std::pow(ndotl, shininess);
        spec_r = specular_color.r * spec;
        spec_g = specular_color.g * spec;
        spec_b = specular_color.b * spec;
    }

    return Color(base_color.r * intensity + spec_r,
                 base_color.g * intensity + spec_g,
                 base_color.b * intensity + spec_b,
                 base_color.a);
}

/// @brief Compute the shaded color with multiple light sources.
///
/// Like shade_pixel(), but sums contributions from all lights in the
/// `lights` array.  Each light's intensity is divided by the number of
/// lights to normalize total brightness.
///
/// @param base_color     The surface (unlit) color.
/// @param normal         Surface normal (unit-length).
/// @param lights         Array of light sources.
/// @param ambient        Ambient light level (0.0–1.0).
/// @param specular_color Specular highlight color.
/// @param shininess      Shininess exponent.
/// @return The final lit color.
///
/// @see shade_pixel(), Light, RenderOptions::lights
inline Color shade_pixel_multi(const Color &base_color, const Vec3 &normal,
                               const std::vector<Light> &lights,
                               float ambient,
                               const Color &specular_color = Color(0, 0, 0, 0),
                               float shininess = 0.0f) {
    Vec3 n = glm::length(normal) > 1e-12f ? glm::normalize(normal) : Vec3(0.0f, 0.0f, 1.0f);

    float total_r = base_color.r * ambient;
    float total_g = base_color.g * ambient;
    float total_b = base_color.b * ambient;

    int nlights = static_cast<int>(lights.size());
    if (nlights == 0) return Color(total_r, total_g, total_b, base_color.a);

    for (int i = 0; i < nlights; i++) {
        const Light &light = lights[i];
        Vec3 light_dir = glm::normalize(light.position);
        float ndotl = std::max(0.0f, glm::dot(n, light_dir));
        float li = light.intensity / static_cast<float>(nlights);
        float diff = (1.0f - ambient) * ndotl * li;
        total_r += base_color.r * diff * light.color.r;
        total_g += base_color.g * diff * light.color.g;
        total_b += base_color.b * diff * light.color.b;

        if (shininess > 0.0f && ndotl > 0.0f) {
            float spec = std::pow(ndotl, shininess) * li;
            total_r += specular_color.r * spec * light.color.r;
            total_g += specular_color.g * spec * light.color.g;
            total_b += specular_color.b * spec * light.color.b;
        }
    }

    return Color(std::min(total_r, 1.0f), std::min(total_g, 1.0f),
                 std::min(total_b, 1.0f), base_color.a);
}

} // namespace scimesh
