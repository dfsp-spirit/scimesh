#pragma once

#include "types.h"
#include "image.h"
#include <vector>

namespace scimesh {

struct Rasterizer {
    int width = 0;
    int height = 0;
    std::vector<float> z_buffer;

    Rasterizer(int w, int h);

    void clear(float clear_depth = 1.0f);

    // Rasterize a single triangle in screen space.
    // screen_v{0,1,2}: (x, y, depth) in screen coordinates
    // color{0,1,2}: vertex colors
    // normal{0,1,2}: vertex normals (in view space for lighting)
    void rasterize_triangle(
        const Vec3 &screen_v0, const Color &color0, const Vec3 &normal0,
        const Vec3 &screen_v1, const Color &color1, const Vec3 &normal1,
        const Vec3 &screen_v2, const Color &color2, const Vec3 &normal2,
        bool backface_culling,
        bool smooth_shading,
        const Vec3 &light_direction,
        Image &output);

private:
    void shade_and_write(int x, int y, float depth,
                         const Color &color, const Vec3 &normal,
                         const Vec3 &light_direction, Image &output);
};

} // namespace scimesh
