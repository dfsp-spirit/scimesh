#include "image.h"
#include <fstream>
#include <cstring>
#include <algorithm>

// When building standalone examples (not the test suite), define the stb
// implementation here.  The test suite provides its own definition in
// test_primitives.cpp, so guard against double-definition at link time.
#ifdef SCIMESH_STB_WRITE_IMPL
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#endif
#include "stb_image_write.h"

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

void Image::crop(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        width = height = 0;
        pixels.clear();
        return;
    }
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width, x + w);
    int y1 = std::min(height, y + h);
    int new_w = std::max(0, x1 - x0);
    int new_h = std::max(0, y1 - y0);

    if (new_w == 0 || new_h == 0) {
        width = height = 0;
        pixels.clear();
        return;
    }

    std::vector<uint8_t> new_pixels(new_w * new_h * 4);
    for (int cy = 0; cy < new_h; ++cy) {
        int src_y = y0 + cy;
        int src_offset = (src_y * width + x0) * 4;
        int dst_offset = cy * new_w * 4;
        std::memcpy(new_pixels.data() + dst_offset,
                    pixels.data() + src_offset,
                    new_w * 4);
    }
    width = new_w;
    height = new_h;
    pixels = std::move(new_pixels);
}

void Image::merge(const Image &other, MergeDirection direction) {
    if (other.width <= 0 || other.height <= 0) return;

    int new_w = 0, new_h = 0;
    int this_off_x = 0, this_off_y = 0;
    int other_off_x = 0, other_off_y = 0;

    switch (direction) {
        case MergeDirection::LEFT:
        case MergeDirection::RIGHT:
            if (height != other.height) return;
            new_w = width + other.width;
            new_h = height;
            if (direction == MergeDirection::LEFT) {
                other_off_x = 0;
                this_off_x = other.width;
            } else {
                this_off_x = 0;
                other_off_x = width;
            }
            break;
        case MergeDirection::TOP:
        case MergeDirection::BOTTOM:
            if (width != other.width) return;
            new_w = width;
            new_h = height + other.height;
            if (direction == MergeDirection::TOP) {
                other_off_y = 0;
                this_off_y = other.height;
            } else {
                this_off_y = 0;
                other_off_y = height;
            }
            break;
    }

    std::vector<uint8_t> new_pixels(new_w * new_h * 4, 0);

    for (int y = 0; y < height; ++y) {
        std::memcpy(new_pixels.data() + ((y + this_off_y) * new_w + this_off_x) * 4,
                    pixels.data() + y * width * 4,
                    width * 4);
    }
    for (int y = 0; y < other.height; ++y) {
        std::memcpy(new_pixels.data() + ((y + other_off_y) * new_w + other_off_x) * 4,
                    other.pixels.data() + y * other.width * 4,
                    other.width * 4);
    }

    width = new_w;
    height = new_h;
    pixels = std::move(new_pixels);
}

void Image::grow(int top, int bottom, int left, int right, const Color &background) {
    if (top < 0 || bottom < 0 || left < 0 || right < 0) return;

    int new_w = width + left + right;
    int new_h = height + top + bottom;
    if (new_w <= 0 || new_h <= 0) return;

    uint8_t br = static_cast<uint8_t>(std::clamp(background.r, 0.0f, 1.0f) * 255.0f);
    uint8_t bg = static_cast<uint8_t>(std::clamp(background.g, 0.0f, 1.0f) * 255.0f);
    uint8_t bb = static_cast<uint8_t>(std::clamp(background.b, 0.0f, 1.0f) * 255.0f);
    uint8_t ba = static_cast<uint8_t>(std::clamp(background.a, 0.0f, 1.0f) * 255.0f);

    std::vector<uint8_t> new_pixels(new_w * new_h * 4);
    for (int i = 0; i < new_w * new_h; ++i) {
        new_pixels[i * 4]     = br;
        new_pixels[i * 4 + 1] = bg;
        new_pixels[i * 4 + 2] = bb;
        new_pixels[i * 4 + 3] = ba;
    }

    for (int y = 0; y < height; ++y) {
        std::memcpy(new_pixels.data() + ((y + top) * new_w + left) * 4,
                    pixels.data() + y * width * 4,
                    width * 4);
    }

    width = new_w;
    height = new_h;
    pixels = std::move(new_pixels);
}

void Image::rotate_90(bool clockwise) {
    int new_w = height;
    int new_h = width;
    std::vector<uint8_t> new_pixels(new_w * new_h * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int dst_x, dst_y;
            if (clockwise) {
                dst_x = height - 1 - y;
                dst_y = x;
            } else {
                dst_x = y;
                dst_y = width - 1 - x;
            }
            std::memcpy(new_pixels.data() + (dst_y * new_w + dst_x) * 4,
                        pixels.data() + (y * width + x) * 4, 4);
        }
    }
    width = new_w;
    height = new_h;
    pixels = std::move(new_pixels);
}

void Image::scale(int new_width, int new_height) {
    if (new_width <= 0 || new_height <= 0) {
        width = height = 0;
        pixels.clear();
        return;
    }
    if (new_width == width && new_height == height) return;

    std::vector<uint8_t> new_pixels(new_width * new_height * 4);
    for (int dy = 0; dy < new_height; ++dy) {
        int src_y = dy * height / new_height;
        for (int dx = 0; dx < new_width; ++dx) {
            int src_x = dx * width / new_width;
            std::memcpy(new_pixels.data() + (dy * new_width + dx) * 4,
                        pixels.data() + (src_y * width + src_x) * 4, 4);
        }
    }
    width = new_width;
    height = new_height;
    pixels = std::move(new_pixels);
}

void Image::crop_to_content(CropContentDirection direction, const Color &background) {
    if (width <= 0 || height <= 0) return;

    uint8_t br = static_cast<uint8_t>(std::clamp(background.r, 0.0f, 1.0f) * 255.0f);
    uint8_t bg = static_cast<uint8_t>(std::clamp(background.g, 0.0f, 1.0f) * 255.0f);
    uint8_t bb = static_cast<uint8_t>(std::clamp(background.b, 0.0f, 1.0f) * 255.0f);
    uint8_t ba = static_cast<uint8_t>(std::clamp(background.a, 0.0f, 1.0f) * 255.0f);

    auto is_bg = [&](int x, int y) {
        int idx = (y * width + x) * 4;
        return pixels[idx] == br && pixels[idx+1] == bg &&
               pixels[idx+2] == bb && pixels[idx+3] == ba;
    };

    int crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;

    bool do_left   = (direction == CropContentDirection::LEFT ||
                      direction == CropContentDirection::HORIZONTAL ||
                      direction == CropContentDirection::ALL);
    bool do_right  = (direction == CropContentDirection::RIGHT ||
                      direction == CropContentDirection::HORIZONTAL ||
                      direction == CropContentDirection::ALL);
    bool do_top    = (direction == CropContentDirection::TOP ||
                      direction == CropContentDirection::VERTICAL ||
                      direction == CropContentDirection::ALL);
    bool do_bottom = (direction == CropContentDirection::BOTTOM ||
                      direction == CropContentDirection::VERTICAL ||
                      direction == CropContentDirection::ALL);

    if (do_left) {
        for (int x = 0; x < width; ++x) {
            bool all_bg = true;
            for (int y = 0; y < height; ++y) {
                if (!is_bg(x, y)) { all_bg = false; break; }
            }
            if (!all_bg) break;
            crop_left = x + 1;
        }
    }

    if (do_right) {
        for (int x = width - 1; x >= crop_left; --x) {
            bool all_bg = true;
            for (int y = 0; y < height; ++y) {
                if (!is_bg(x, y)) { all_bg = false; break; }
            }
            if (!all_bg) break;
            crop_right = width - x;
        }
    }

    if (do_top) {
        for (int y = 0; y < height; ++y) {
            bool all_bg = true;
            for (int x = 0; x < width; ++x) {
                if (!is_bg(x, y)) { all_bg = false; break; }
            }
            if (!all_bg) break;
            crop_top = y + 1;
        }
    }

    if (do_bottom) {
        for (int y = height - 1; y >= crop_top; --y) {
            bool all_bg = true;
            for (int x = 0; x < width; ++x) {
                if (!is_bg(x, y)) { all_bg = false; break; }
            }
            if (!all_bg) break;
            crop_bottom = height - y;
        }
    }

    int new_w = width - crop_left - crop_right;
    int new_h = height - crop_top - crop_bottom;
    crop(crop_left, crop_top, std::max(0, new_w), std::max(0, new_h));
}

Color Image::sample_bilinear(float u, float v) const {
    if (width < 1 || height < 1) return Color();
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    float fx = u * (width - 1);
    float fy = v * (height - 1);
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, width - 1);
    int y1 = std::min(y0 + 1, height - 1);
    float sx = fx - x0;
    float sy = fy - y0;

    auto get = [this](int px, int py) -> Color {
        int idx = (py * width + px) * 4;
        return Color(pixels[idx] / 255.0f, pixels[idx+1] / 255.0f,
                     pixels[idx+2] / 255.0f, pixels[idx+3] / 255.0f);
    };
    Color c00 = get(x0, y0); Color c10 = get(x1, y0);
    Color c01 = get(x0, y1); Color c11 = get(x1, y1);

    return Color(
        c00.r * (1-sx)*(1-sy) + c10.r * sx*(1-sy) + c01.r * (1-sx)*sy + c11.r * sx*sy,
        c00.g * (1-sx)*(1-sy) + c10.g * sx*(1-sy) + c01.g * (1-sx)*sy + c11.g * sx*sy,
        c00.b * (1-sx)*(1-sy) + c10.b * sx*(1-sy) + c01.b * (1-sx)*sy + c11.b * sx*sy,
        c00.a * (1-sx)*(1-sy) + c10.a * sx*(1-sy) + c01.a * (1-sx)*sy + c11.a * sx*sy);
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

bool Image::write_png(const std::string &filename) const {
    if (width <= 0 || height <= 0 || pixels.empty())
        return false;
    int stride = width * 4;
    return stbi_write_png(filename.c_str(), width, height, 4,
                          pixels.data(), stride) != 0;
}

} // namespace scimesh
