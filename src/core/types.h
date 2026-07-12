#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <cstdint>
#include <string>

namespace scimesh {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}
};

struct Triangle {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
    uint32_t v2 = 0;
};

constexpr Color DEFAULT_COLOR{0.7f, 0.7f, 0.7f, 1.0f};
constexpr Color TRANSPARENT_BLACK{0.0f, 0.0f, 0.0f, 0.0f};

struct Light {
    Vec3 position = Vec3(0.0f, 0.0f, 1.0f);
    Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    bool is_directional = true;
};

struct ClipPlane {
    Vec3 normal = Vec3(0.0f, 0.0f, -1.0f);
    float offset = 0.0f;
};

} // namespace scimesh
