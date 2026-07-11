#pragma once

#include "types.h"
#include "image.h"
#include <vector>

namespace scimesh {

struct Rasterizer {
    int width = 0;
    int height = 0;
    std::vector<float> z_buffer;
    bool blend_mode = false;

    Rasterizer(int w, int h);

    void clear(float clear_depth = 1.0f);
    void set_blend_mode(bool enabled) { blend_mode = enabled; }

    void rasterize_triangle(
        const Vec3 &screen_v0, const Color &color0, const Vec3 &normal0,
        const Vec3 &screen_v1, const Color &color1, const Vec3 &normal1,
        const Vec3 &screen_v2, const Color &color2, const Vec3 &normal2,
        bool backface_culling,
        bool smooth_shading,
        const Vec3 &light_direction,
        bool wireframe,
        const Color &wireframe_color,
        Image &output);

private:
    void shade_and_write(int x, int y, float depth,
                         const Color &color, const Vec3 &normal,
                         const Vec3 &light_direction, Image &output);
};

} // namespace scimesh
