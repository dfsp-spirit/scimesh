#pragma once

#include "types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace scimesh {

inline Vec3 compute_face_normal(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2) {
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 normal = glm::cross(edge1, edge2);
    float len = glm::length(normal);
    if (len < 1e-12f)
        return Vec3(0.0f, 0.0f, 1.0f);
    return normal / len;
}

inline Vec3 transform_point(const Mat4 &m, const Vec3 &p) {
    Vec4 result = m * Vec4(p, 1.0f);
    return Vec3(result);
}

inline Vec4 transform_point_homogeneous(const Mat4 &m, const Vec3 &p) {
    return m * Vec4(p, 1.0f);
}

inline Vec3 transform_direction(const Mat4 &m, const Vec3 &d) {
    Vec4 result = m * Vec4(d, 0.0f);
    return Vec3(result);
}

inline Vec3 perspective_divide(const Vec4 &clip) {
    if (std::abs(clip.w) < 1e-12f)
        return Vec3(clip.x, clip.y, clip.z);
    return Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
}

inline void ndc_to_screen(const Vec3 &ndc, int width, int height,
                          float &screen_x, float &screen_y, float &depth) {
    screen_x = (ndc.x + 1.0f) * 0.5f * static_cast<float>(width);
    screen_y = (1.0f - ndc.y) * 0.5f * static_cast<float>(height);
    depth = ndc.z;
}

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

inline Color shade_pixel(const Color &base_color, const Vec3 &normal,
                         const Vec3 &light_dir,
                         const Color &specular_color = Color(0, 0, 0, 0),
                         float shininess = 0.0f) {
    Vec3 n = glm::length(normal) > 1e-12f ? glm::normalize(normal) : Vec3(0.0f, 0.0f, 1.0f);
    Vec3 l = glm::normalize(light_dir);
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

} // namespace scimesh
