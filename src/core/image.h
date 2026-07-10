#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace scimesh {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // RGBA, row-major, 4 bytes per pixel

    Image() = default;
    Image(int w, int h);

    void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void get_pixel(int x, int y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) const;

    void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void clear_float(float r, float g, float b, float a);

    // Write to PPM (for C++ test debugging - trivial format, no dependencies)
    bool write_ppm(const std::string &filename) const;

    // Write to BMP (for C++ test debugging - trivial format, no dependencies)
    bool write_bmp(const std::string &filename) const;
};

} // namespace scimesh
