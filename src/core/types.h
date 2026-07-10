#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <cstdint>
#include <string>

namespace scimesh {

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

} // namespace scimesh
