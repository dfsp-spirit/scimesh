#include "image.h"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace scimesh {

Image::Image(int w, int h) : width(w), height(h), pixels(w * h * 4, 0) {}

void Image::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;
    int idx = (y * width + x) * 4;
    pixels[idx] = r;
    pixels[idx + 1] = g;
    pixels[idx + 2] = b;
    pixels[idx + 3] = a;
}

void Image::get_pixel(int x, int y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        r = g = b = a = 0;
        return;
    }
    int idx = (y * width + x) * 4;
    r = pixels[idx];
    g = pixels[idx + 1];
    b = pixels[idx + 2];
    a = pixels[idx + 3];
}

void Image::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }
}

void Image::clear_float(float r, float g, float b, float a) {
    clear(static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f),
          static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f),
          static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f),
          static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f));
}

Image Image::downsample_box(int factor) const {
    if (factor <= 1)
        return *this;

    int out_w = width / factor;
    int out_h = height / factor;
    Image result(out_w, out_h);

    int n = factor * factor;
    for (int y = 0; y < out_h; y++) {
        for (int x = 0; x < out_w; x++) {
            int r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;
            for (int dy = 0; dy < factor; dy++) {
                for (int dx = 0; dx < factor; dx++) {
                    int sx = x * factor + dx;
                    int sy = y * factor + dy;
                    int idx = (sy * width + sx) * 4;
                    r_sum += pixels[idx];
                    g_sum += pixels[idx + 1];
                    b_sum += pixels[idx + 2];
                    a_sum += pixels[idx + 3];
                }
            }
            result.set_pixel(x, y,
                static_cast<uint8_t>(r_sum / n),
                static_cast<uint8_t>(g_sum / n),
                static_cast<uint8_t>(b_sum / n),
                static_cast<uint8_t>(a_sum / n));
        }
    }
    return result;
}

bool Image::write_ppm(const std::string &filename) const {
    std::ofstream f(filename, std::ios::binary);
    if (!f)
        return false;
    f << "P6\n" << width << " " << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        f.put(pixels[i * 4]);
        f.put(pixels[i * 4 + 1]);
        f.put(pixels[i * 4 + 2]);
    }
    return f.good();
}

bool Image::write_bmp(const std::string &filename) const {
    // BMP with alpha (BGRA, bottom-up rows)
    int row_size = width * 4;
    int pixel_data_size = row_size * height;
    int file_size = 14 + 40 + 56 + pixel_data_size; // BITMAPV4 header

    std::ofstream f(filename, std::ios::binary);
    if (!f)
        return false;

    // BMP file header (14 bytes)
    uint8_t fh[14] = {0};
    fh[0] = 'B';
    fh[1] = 'M';
    std::memcpy(fh + 2, &file_size, 4);
    // reserved = 0
    uint32_t pixel_offset = 14 + 40 + 56;
    std::memcpy(fh + 10, &pixel_offset, 4);
    f.write(reinterpret_cast<const char *>(fh), 14);

    // BITMAPV4HEADER (108 bytes, but we use 40 + 56 = 96... actually let's use V4 = 108)
    // For simplicity, use BITMAPINFOHEADER (40 bytes) + bit masks (16 bytes) = 56 byte header
    uint32_t header_size = 56;
    f.write(reinterpret_cast<const char *>(&header_size), 4);
    int32_t w = width;
    int32_t h = height;
    f.write(reinterpret_cast<const char *>(&w), 4);
    f.write(reinterpret_cast<const char *>(&h), 4);
    uint16_t planes = 1;
    f.write(reinterpret_cast<const char *>(&planes), 2);
    uint16_t bpp = 32;
    f.write(reinterpret_cast<const char *>(&bpp), 2);
    uint32_t compression = 3; // BI_BITFIELDS
    f.write(reinterpret_cast<const char *>(&compression), 4);
    uint32_t img_size = pixel_data_size;
    f.write(reinterpret_cast<const char *>(&img_size), 4);
    int32_t ppm = 2835;
    f.write(reinterpret_cast<const char *>(&ppm), 4);
    f.write(reinterpret_cast<const char *>(&ppm), 4);
    uint32_t colors_used = 0;
    f.write(reinterpret_cast<const char *>(&colors_used), 4);
    uint32_t colors_important = 0;
    f.write(reinterpret_cast<const char *>(&colors_important), 4);
    // Color masks (16 bytes): RGBA
    uint32_t r_mask = 0x00FF0000;
    uint32_t g_mask = 0x0000FF00;
    uint32_t b_mask = 0x000000FF;
    uint32_t a_mask = 0xFF000000;
    f.write(reinterpret_cast<const char *>(&r_mask), 4);
    f.write(reinterpret_cast<const char *>(&g_mask), 4);
    f.write(reinterpret_cast<const char *>(&b_mask), 4);
    f.write(reinterpret_cast<const char *>(&a_mask), 4);

    // Pixel data (bottom-up, BGRA)
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            f.put(pixels[idx + 2]); // B
            f.put(pixels[idx + 1]); // G
            f.put(pixels[idx]);     // R
            f.put(pixels[idx + 3]); // A
        }
    }
    return f.good();
}

} // namespace scimesh
