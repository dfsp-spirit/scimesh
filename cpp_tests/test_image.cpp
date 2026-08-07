#include "catch_amalgamated.hpp"
#include <scimesh/image.h>
#include <scimesh/to_string.h>
#include <sstream>
#include <fstream>
#include <cstdint>

using namespace scimesh;

TEST_CASE("Image constructs with correct dimensions", "[image]") {
    Image img(100, 50);
    REQUIRE(img.width == 100);
    REQUIRE(img.height == 50);
    REQUIRE(img.pixels.size() == 100 * 50 * 4);
}

TEST_CASE("Image set_pixel and get_pixel work correctly", "[image]") {
    Image img(10, 10);
    img.set_pixel(5, 3, 255, 128, 64, 200);
    uint8_t r, g, b, a;
    img.get_pixel(5, 3, r, g, b, a);
    REQUIRE(r == 255);
    REQUIRE(g == 128);
    REQUIRE(b == 64);
    REQUIRE(a == 200);
}

TEST_CASE("Image get_pixel out of bounds returns zeros", "[image]") {
    Image img(10, 10);
    uint8_t r, g, b, a;
    img.get_pixel(-1, 5, r, g, b, a);
    REQUIRE(r == 0);
    REQUIRE(g == 0);
    REQUIRE(b == 0);
    REQUIRE(a == 0);
}

TEST_CASE("Image clear fills all pixels", "[image]") {
    Image img(5, 5);
    img.clear(255, 0, 0, 255);
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            uint8_t r, g, b, a;
            img.get_pixel(i, j, r, g, b, a);
            REQUIRE(r == 255);
            REQUIRE(g == 0);
            REQUIRE(b == 0);
            REQUIRE(a == 255);
        }
    }
}

TEST_CASE("Image clear with float values converts correctly", "[image]") {
    Image img(5, 5);
    img.clear_float(1.0f, 0.5f, 0.0f, 1.0f);
    uint8_t r, g, b, a;
    img.get_pixel(0, 0, r, g, b, a);
    REQUIRE(r == 255);
    REQUIRE(g == 127);
    REQUIRE(b == 0);
    REQUIRE(a == 255);
}

TEST_CASE("Image write_ppm creates valid file", "[image]") {
    Image img(4, 4);
    img.clear(255, 128, 64, 255);
    REQUIRE(img.write_ppm("test_output.ppm"));
}

TEST_CASE("Image write_bmp creates valid file", "[image]") {
    Image img(8, 8);
    img.set_pixel(0, 0, 255, 0, 0, 255);
    img.set_pixel(7, 7, 0, 255, 0, 255);
    REQUIRE(img.write_bmp("test_output.bmp"));
}

// ---- TGA test helpers ------------------------------------------------------

struct TgaImageData {
    int width = 0;
    int height = 0;
    int bpp = 0;
    unsigned char descriptor = 0;
    std::vector<uint8_t> pixels;  // RGBA, top-left origin
};

// Minimal TGA reader for tests — uncompressed true-color (type 2), 24/32-bit,
// no color map, no image ID. Assumes top-left origin (0x20 descriptor bit).
static bool read_tga(const std::string &path, TgaImageData &img) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    unsigned char hdr[18];
    in.read(reinterpret_cast<char *>(hdr), 18);
    if (!in) return false;
    if (hdr[0] != 0) return false;                  // no image ID
    if (hdr[1] != 0) return false;                  // no color map
    if (hdr[2] != 2) return false;                  // uncompressed true-color

    img.width  = hdr[12] | (hdr[13] << 8);
    img.height = hdr[14] | (hdr[15] << 8);
    img.bpp    = hdr[16];
    img.descriptor = hdr[17];
    if (img.bpp != 24 && img.bpp != 32) return false;
    if ((img.descriptor & 0x20) == 0) return false; // tests assume top-left

    const int bytes_per_pixel = img.bpp / 8;
    img.pixels.assign(static_cast<size_t>(img.width) * img.height * 4, 0);

    std::vector<uint8_t> row(static_cast<size_t>(img.width) * bytes_per_pixel);
    for (int y = 0; y < img.height; ++y) {
        in.read(reinterpret_cast<char *>(row.data()),
                static_cast<std::streamsize>(row.size()));
        if (!in) return false;
        size_t o = 0;
        for (int x = 0; x < img.width; ++x) {
            uint8_t b = row[o++];
            uint8_t g = row[o++];
            uint8_t r = row[o++];
            uint8_t a = (bytes_per_pixel == 4) ? row[o++] : 255;
            size_t idx = (static_cast<size_t>(y) * img.width + x) * 4;
            img.pixels[idx]     = r;
            img.pixels[idx + 1] = g;
            img.pixels[idx + 2] = b;
            img.pixels[idx + 3] = a;
        }
    }
    return true;
}

TEST_CASE("Image write_tga 32-bit roundtrip", "[image][tga]") {
    Image img(3, 2);
    img.set_pixel(0, 0, 255, 0, 0, 255);      // red
    img.set_pixel(1, 0, 0, 255, 0, 255);      // green
    img.set_pixel(2, 0, 0, 0, 255, 255);      // blue
    img.set_pixel(0, 1, 255, 255, 255, 255);  // white
    img.set_pixel(1, 1, 255, 0, 0, 128);      // red, alpha 128
    img.set_pixel(2, 1, 0, 0, 0, 255);        // black

    REQUIRE(img.write_tga("test_output.tga"));

    // File size must be exactly 18-byte header + W*H*4.
    std::ifstream in("test_output.tga", std::ios::binary | std::ios::ate);
    REQUIRE(in.good());
    REQUIRE(in.tellg() == 18 + 3 * 2 * 4);

    TgaImageData tga;
    REQUIRE(read_tga("test_output.tga", tga));
    REQUIRE(tga.width == 3);
    REQUIRE(tga.height == 2);
    REQUIRE(tga.bpp == 32);
    REQUIRE((tga.descriptor & 0x20) != 0);   // top-left origin
    REQUIRE((tga.descriptor & 0x0F) == 8);   // 8 alpha bits

    // Every pixel must round-trip exactly (RGBA) — proves the BGR swap.
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            size_t idx = (static_cast<size_t>(y) * 3 + x) * 4;
            REQUIRE(tga.pixels[idx] == r);
            REQUIRE(tga.pixels[idx + 1] == g);
            REQUIRE(tga.pixels[idx + 2] == b);
            REQUIRE(tga.pixels[idx + 3] == a);
        }
    }
}

TEST_CASE("Image write_tga 24-bit roundtrip", "[image][tga]") {
    Image img(3, 2);
    img.set_pixel(0, 0, 255, 0, 0, 255);
    img.set_pixel(1, 0, 0, 255, 0, 255);
    img.set_pixel(2, 0, 0, 0, 255, 255);
    img.set_pixel(0, 1, 255, 255, 255, 255);
    img.set_pixel(1, 1, 255, 0, 0, 128);      // alpha dropped in 24-bit
    img.set_pixel(2, 1, 0, 0, 0, 255);

    REQUIRE(img.write_tga("test_output_24.tga", true));

    // File size must be exactly 18-byte header + W*H*3.
    std::ifstream in("test_output_24.tga", std::ios::binary | std::ios::ate);
    REQUIRE(in.good());
    REQUIRE(in.tellg() == 18 + 3 * 2 * 3);

    TgaImageData tga;
    REQUIRE(read_tga("test_output_24.tga", tga));
    REQUIRE(tga.width == 3);
    REQUIRE(tga.height == 2);
    REQUIRE(tga.bpp == 24);
    REQUIRE((tga.descriptor & 0x20) != 0);   // top-left origin
    REQUIRE((tga.descriptor & 0x0F) == 0);   // no alpha bits

    // RGB preserved; alpha forced opaque (24-bit has no alpha channel).
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            size_t idx = (static_cast<size_t>(y) * 3 + x) * 4;
            REQUIRE(tga.pixels[idx] == r);
            REQUIRE(tga.pixels[idx + 1] == g);
            REQUIRE(tga.pixels[idx + 2] == b);
            REQUIRE(tga.pixels[idx + 3] == 255);
        }
    }
}

TEST_CASE("Image write_tga unwritable path returns false", "[image][tga]") {
    Image img(2, 2);
    img.clear(255, 255, 255, 255);
    REQUIRE_FALSE(img.write_tga("/nonexistent_dir_xyz/out.tga"));
}

TEST_CASE("Image write_tga rejects empty image", "[image][tga]") {
    Image img(0, 0);
    REQUIRE_FALSE(img.write_tga("test_empty.tga"));
    REQUIRE_FALSE(std::ifstream("test_empty.tga").good());
}

static Image make_2x2() {
    Image img(2, 2);
    img.set_pixel(0, 0, 255, 0, 0, 255);
    img.set_pixel(1, 0, 0, 255, 0, 255);
    img.set_pixel(0, 1, 0, 0, 255, 255);
    img.set_pixel(1, 1, 255, 255, 0, 255);
    return img;
}

static void check_pixel(const Image &img, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t cr, cg, cb, ca;
    img.get_pixel(x, y, cr, cg, cb, ca);
    REQUIRE(cr == r);
    REQUIRE(cg == g);
    REQUIRE(cb == b);
    REQUIRE(ca == a);
}

TEST_CASE("Image crop normal sub-rectangle", "[image]") {
    Image img = make_2x2();
    img.crop(1, 0, 1, 2);
    REQUIRE(img.width == 1);
    REQUIRE(img.height == 2);
    check_pixel(img, 0, 0, 0, 255, 0, 255);
    check_pixel(img, 0, 1, 255, 255, 0, 255);
}

TEST_CASE("Image crop partial out-of-bounds clamps", "[image]") {
    Image img(4, 4);
    img.clear(128, 64, 32, 255);
    img.crop(-1, -1, 6, 6);
    REQUIRE(img.width == 4);
    REQUIRE(img.height == 4);
    check_pixel(img, 0, 0, 128, 64, 32, 255);
}

TEST_CASE("Image crop fully out-of-bounds zeroes dims", "[image]") {
    Image img(4, 4);
    img.clear(1, 1, 1, 1);
    img.crop(10, 10, 2, 2);
    REQUIRE(img.width == 0);
    REQUIRE(img.height == 0);
    REQUIRE(img.pixels.empty());
}

TEST_CASE("Image merge LEFT", "[image]") {
    Image a(2, 2); a.clear(255, 0, 0, 255);
    Image b(1, 2); b.clear(0, 255, 0, 255);
    a.merge(b, MergeDirection::LEFT);
    REQUIRE(a.width == 3);
    REQUIRE(a.height == 2);
    check_pixel(a, 0, 0, 0, 255, 0, 255);
    check_pixel(a, 2, 0, 255, 0, 0, 255);
}

TEST_CASE("Image merge RIGHT", "[image]") {
    Image a(2, 2); a.clear(255, 0, 0, 255);
    Image b(1, 2); b.clear(0, 255, 0, 255);
    a.merge(b, MergeDirection::RIGHT);
    REQUIRE(a.width == 3);
    REQUIRE(a.height == 2);
    check_pixel(a, 0, 0, 255, 0, 0, 255);
    check_pixel(a, 2, 0, 0, 255, 0, 255);
}

TEST_CASE("Image merge TOP", "[image]") {
    Image a(2, 1); a.clear(255, 0, 0, 255);
    Image b(2, 2); b.clear(0, 255, 0, 255);
    a.merge(b, MergeDirection::TOP);
    REQUIRE(a.width == 2);
    REQUIRE(a.height == 3);
    check_pixel(a, 0, 0, 0, 255, 0, 255);
    check_pixel(a, 0, 2, 255, 0, 0, 255);
}

TEST_CASE("Image merge BOTTOM", "[image]") {
    Image a(2, 1); a.clear(255, 0, 0, 255);
    Image b(2, 2); b.clear(0, 255, 0, 255);
    a.merge(b, MergeDirection::BOTTOM);
    REQUIRE(a.width == 2);
    REQUIRE(a.height == 3);
    check_pixel(a, 0, 0, 255, 0, 0, 255);
    check_pixel(a, 0, 2, 0, 255, 0, 255);
}

TEST_CASE("Image merge height mismatch no-op", "[image]") {
    Image a(2, 2); a.clear(255, 0, 0, 255);
    Image b(1, 3); b.clear(0, 255, 0, 255);
    a.merge(b, MergeDirection::LEFT);
    REQUIRE(a.width == 2);
    REQUIRE(a.height == 2);
}

TEST_CASE("Image merge width mismatch no-op", "[image]") {
    Image a(2, 2); a.clear(255, 0, 0, 255);
    Image b(3, 2); b.clear(0, 255, 0, 255);
    a.merge(b, MergeDirection::TOP);
    REQUIRE(a.width == 2);
    REQUIRE(a.height == 2);
}

TEST_CASE("Image grow uniform padding", "[image]") {
    Image img(2, 2); img.clear(255, 0, 0, 255);
    img.grow(1, 1, 1, 1, Color(0, 1, 0, 1));
    REQUIRE(img.width == 4);
    REQUIRE(img.height == 4);
    check_pixel(img, 0, 0, 0, 255, 0, 255);
    check_pixel(img, 1, 1, 255, 0, 0, 255);
    check_pixel(img, 2, 1, 255, 0, 0, 255);
    check_pixel(img, 1, 2, 255, 0, 0, 255);
}

TEST_CASE("Image grow asymmetric padding", "[image]") {
    Image img(1, 1); img.clear(128, 128, 128, 255);
    img.grow(2, 0, 3, 1, Color(1, 0, 0, 1));
    REQUIRE(img.width == 5);
    REQUIRE(img.height == 3);
    check_pixel(img, 0, 0, 255, 0, 0, 255);
    check_pixel(img, 3, 2, 128, 128, 128, 255);
}

TEST_CASE("Image grow negative values ignored", "[image]") {
    Image img(2, 2); img.clear(255, 0, 0, 255);
    img.grow(-1, 0, 0, 0, Color(0, 1, 0, 1));
    REQUIRE(img.width == 2);
    REQUIRE(img.height == 2);
}

TEST_CASE("Image rotate_90 clockwise square", "[image]") {
    Image img = make_2x2();
    img.rotate_90(true);
    REQUIRE(img.width == 2);
    REQUIRE(img.height == 2);
    check_pixel(img, 1, 0, 255, 0, 0, 255);
    check_pixel(img, 1, 1, 0, 255, 0, 255);
    check_pixel(img, 0, 0, 0, 0, 255, 255);
    check_pixel(img, 0, 1, 255, 255, 0, 255);
}

TEST_CASE("Image rotate_90 CCW rectangular", "[image]") {
    Image img(3, 1);
    img.set_pixel(0, 0, 255, 0, 0, 255);
    img.set_pixel(1, 0, 0, 255, 0, 255);
    img.set_pixel(2, 0, 0, 0, 255, 255);
    img.rotate_90(false);
    REQUIRE(img.width == 1);
    REQUIRE(img.height == 3);
    check_pixel(img, 0, 2, 255, 0, 0, 255);
    check_pixel(img, 0, 1, 0, 255, 0, 255);
    check_pixel(img, 0, 0, 0, 0, 255, 255);
}

TEST_CASE("Image rotate_90 four times CCW restores image", "[image]") {
    Image orig = make_2x2();
    Image img = make_2x2();
    for (int i = 0; i < 4; ++i) img.rotate_90(false);
    REQUIRE(img.width == orig.width);
    REQUIRE(img.height == orig.height);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) {
            uint8_t r1, g1, b1, a1, r2, g2, b2, a2;
            orig.get_pixel(x, y, r1, g1, b1, a1);
            img.get_pixel(x, y, r2, g2, b2, a2);
            REQUIRE(r1 == r2);
            REQUIRE(g1 == g2);
            REQUIRE(b1 == b2);
            REQUIRE(a1 == a2);
        }
}

TEST_CASE("Image scale upscale 2x preserves values", "[image]") {
    Image img = make_2x2();
    img.scale(4, 4);
    REQUIRE(img.width == 4);
    REQUIRE(img.height == 4);
    check_pixel(img, 0, 0, 255, 0, 0, 255);
    check_pixel(img, 2, 2, 255, 255, 0, 255);
}

TEST_CASE("Image scale downscale from 4x4 to 2x2", "[image]") {
    Image img(4, 4);
    img.clear(128, 0, 0, 255);
    img.scale(2, 2);
    REQUIRE(img.width == 2);
    REQUIRE(img.height == 2);
    check_pixel(img, 0, 0, 128, 0, 0, 255);
}

TEST_CASE("Image scale same size is identity", "[image]") {
    Image img = make_2x2();
    img.scale(2, 2);
    REQUIRE(img.width == 2);
    REQUIRE(img.height == 2);
    check_pixel(img, 0, 0, 255, 0, 0, 255);
    check_pixel(img, 1, 1, 255, 255, 0, 255);
}

TEST_CASE("Image scale to zero clears image", "[image]") {
    Image img(2, 2);
    img.clear(1, 1, 1, 1);
    img.scale(0, 5);
    REQUIRE(img.width == 0);
    REQUIRE(img.height == 0);
    REQUIRE(img.pixels.empty());
}

TEST_CASE("Image crop_to_content LEFT with margin", "[image]") {
    Image img(4, 2);
    img.clear_float(0, 1, 0, 1);
    img.set_pixel(2, 0, 255, 0, 0, 255);
    img.set_pixel(3, 1, 255, 0, 0, 255);
    img.crop_to_content(CropContentDirection::LEFT, Color(0, 1, 0, 1));
    REQUIRE(img.width == 2);
    REQUIRE(img.height == 2);
    check_pixel(img, 0, 0, 255, 0, 0, 255);
}

TEST_CASE("Image crop_to_content HORIZONTAL crops both sides", "[image]") {
    Image img(5, 1);
    img.clear_float(0, 0, 1, 1);
    img.set_pixel(1, 0, 255, 0, 0, 255);
    img.set_pixel(3, 0, 255, 0, 0, 255);
    img.crop_to_content(CropContentDirection::HORIZONTAL, Color(0, 0, 1, 1));
    REQUIRE(img.width == 3);
    check_pixel(img, 0, 0, 255, 0, 0, 255);
    check_pixel(img, 2, 0, 255, 0, 0, 255);
}

TEST_CASE("Image crop_to_content VERTICAL crops top and bottom", "[image]") {
    Image img(1, 5);
    img.clear_float(1, 1, 0, 1);
    img.set_pixel(0, 1, 255, 0, 0, 255);
    img.set_pixel(0, 3, 255, 0, 0, 255);
    img.crop_to_content(CropContentDirection::VERTICAL, Color(1, 1, 0, 1));
    REQUIRE(img.height == 3);
    check_pixel(img, 0, 0, 255, 0, 0, 255);
    check_pixel(img, 0, 2, 255, 0, 0, 255);
}

TEST_CASE("Image crop_to_content ALL crops all sides", "[image]") {
    Image img(5, 5);
    img.clear_float(0, 0, 0, 1);
    img.set_pixel(1, 1, 255, 0, 0, 255);
    img.set_pixel(3, 3, 255, 0, 0, 255);
    img.crop_to_content(CropContentDirection::ALL, Color(0, 0, 0, 1));
    REQUIRE(img.width == 3);
    REQUIRE(img.height == 3);
}

TEST_CASE("Image crop_to_content fully background clears image", "[image]") {
    Image img(3, 3);
    img.clear_float(1, 0, 0, 1);
    img.crop_to_content(CropContentDirection::ALL, Color(1, 0, 0, 1));
    REQUIRE(img.width == 0);
    REQUIRE(img.height == 0);
    REQUIRE(img.pixels.empty());
}

TEST_CASE("Image crop_to_content no margin is no-op", "[image]") {
    Image img(2, 2);
    img.clear_float(0, 1, 0, 1);
    img.set_pixel(0, 0, 255, 0, 0, 255);
    img.set_pixel(1, 1, 255, 0, 0, 255);
    img.crop_to_content(CropContentDirection::LEFT, Color(0, 1, 0, 1));
    REQUIRE(img.width == 2);
    REQUIRE(img.height == 2);
}

TEST_CASE("to_string Image operator<<", "[to_string]") {
    Image img(100, 50);
    std::ostringstream os;
    os << img;
    std::string s = os.str();
    REQUIRE(s.find("Image") != std::string::npos);
    REQUIRE(s.find("100x50") != std::string::npos);
}

TEST_CASE("to_string Camera operator<<", "[to_string]") {
    Camera cam;
    std::ostringstream os;
    os << cam;
    std::string s = os.str();
    REQUIRE(s.find("Camera") != std::string::npos);
    REQUIRE(s.find("persp") != std::string::npos);
}

TEST_CASE("to_string RenderOptions operator<<", "[to_string]") {
    RenderOptions opts;
    opts.ssao_enabled = true;
    opts.aa_samples = 4;
    std::ostringstream os;
    os << opts;
    std::string s = os.str();
    REQUIRE(s.find("RenderOpts") != std::string::npos);
    REQUIRE(s.find("ssao") != std::string::npos);
    REQUIRE(s.find("aa=4") != std::string::npos);
}

// =========================================================================
//  read_image
// =========================================================================

static Image make_solid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    Image img(w, h);
    img.clear(r, g, b, a);
    return img;
}

TEST_CASE("Image read_image roundtrip via PNG", "[image][read]") {
    Image orig = make_solid(8, 6, 255, 128, 64, 255);
    REQUIRE(orig.write_png("test_read_roundtrip.png"));

    Image loaded = Image::read_image("test_read_roundtrip.png");
    REQUIRE(loaded.width == 8);
    REQUIRE(loaded.height == 6);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            check_pixel(loaded, x, y, 255, 128, 64, 255);
}

TEST_CASE("Image read_image roundtrip via TGA", "[image][read]") {
    Image orig(3, 2);
    orig.set_pixel(0, 0, 255, 0, 0, 255);
    orig.set_pixel(1, 0, 0, 255, 0, 255);
    orig.set_pixel(2, 0, 0, 0, 255, 128);
    REQUIRE(orig.write_tga("test_read_roundtrip.tga"));

    Image loaded = Image::read_image("test_read_roundtrip.tga");
    REQUIRE(loaded.width == 3);
    REQUIRE(loaded.height == 2);
    check_pixel(loaded, 0, 0, 255, 0, 0, 255);
    check_pixel(loaded, 1, 0, 0, 255, 0, 255);
    check_pixel(loaded, 2, 0, 0, 0, 255, 128);
}

TEST_CASE("Image read_image nonexistent file returns empty", "[image][read]") {
    Image img = Image::read_image("/nonexistent_path_xyz/nope.png");
    REQUIRE(img.width == 0);
    REQUIRE(img.height == 0);
    REQUIRE(img.pixels.empty());
}

// =========================================================================
//  pad_to_size
// =========================================================================

TEST_CASE("Image pad_to_size centers smaller image", "[image][pad]") {
    Image img = make_solid(2, 2, 255, 0, 0, 255);
    img.pad_to_size(6, 4, Color(0, 1, 0, 1));
    REQUIRE(img.width == 6);
    REQUIRE(img.height == 4);
    // center of padded image should contain original red
    check_pixel(img, 2, 1, 255, 0, 0, 255);  // top-left of original
    check_pixel(img, 3, 1, 255, 0, 0, 255);  // top-right of original
    check_pixel(img, 2, 2, 255, 0, 0, 255);  // bottom-left of original
    check_pixel(img, 3, 2, 255, 0, 0, 255);  // bottom-right of original
    // border should be green
    check_pixel(img, 0, 0, 0, 255, 0, 255);
    check_pixel(img, 5, 0, 0, 255, 0, 255);
    check_pixel(img, 0, 3, 0, 255, 0, 255);
    check_pixel(img, 5, 3, 0, 255, 0, 255);
}

TEST_CASE("Image pad_to_size already at target is no-op", "[image][pad]") {
    Image img = make_solid(4, 4, 255, 0, 0, 255);
    img.pad_to_size(4, 4, Color(0, 1, 0, 1));
    REQUIRE(img.width == 4);
    REQUIRE(img.height == 4);
    check_pixel(img, 2, 2, 255, 0, 0, 255);
}

TEST_CASE("Image pad_to_size already larger is no-op", "[image][pad]") {
    Image img = make_solid(8, 8, 255, 0, 0, 255);
    img.pad_to_size(4, 4, Color(0, 1, 0, 1));
    REQUIRE(img.width == 8);
    REQUIRE(img.height == 8);
}

TEST_CASE("Image pad_to_size asymmetric deficit distributes correctly", "[image][pad]") {
    // 3→7: deficit of 4, pad_left=2, pad_right=2  (even split)
    // 3→8: deficit of 5, pad_left=2, pad_right=3  (odd, right gets extra)
    Image img = make_solid(3, 3, 255, 255, 255, 255);
    img.pad_to_size(8, 7, Color(0, 0, 0, 255));
    REQUIRE(img.width == 8);
    REQUIRE(img.height == 7);
    // white at expected center positions
    check_pixel(img, 2, 2, 255, 255, 255, 255);
    check_pixel(img, 4, 2, 255, 255, 255, 255);
    check_pixel(img, 2, 4, 255, 255, 255, 255);
    // black at left border
    check_pixel(img, 1, 2, 0, 0, 0, 255);
    // black at right border (extra pixel on right from odd deficit)
    check_pixel(img, 5, 2, 0, 0, 0, 255);
}

// =========================================================================
//  grid_arrange
// =========================================================================

static bool pixel_is(const Image &img, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t cr, cg, cb, ca;
    img.get_pixel(x, y, cr, cg, cb, ca);
    return cr == r && cg == g && cb == b;
}

TEST_CASE("grid_arrange 2x2 four colors preserves positions", "[image][grid]") {
    auto red    = make_solid(5, 5, 255, 0, 0);
    auto green  = make_solid(5, 5, 0, 255, 0);
    auto blue   = make_solid(5, 5, 0, 0, 255);
    auto yellow = make_solid(5, 5, 255, 255, 0);

    Image result = grid_arrange({red, green, blue, yellow}, 2, 2);

    REQUIRE(result.width == 10);
    REQUIRE(result.height == 10);

    // Top row (y=5..9): red (left), green (right)
    REQUIRE(pixel_is(result, 2, 7, 255, 0, 0));
    REQUIRE(pixel_is(result, 7, 7, 0, 255, 0));
    // Bottom row (y=0..4): blue (left), yellow (right)
    REQUIRE(pixel_is(result, 2, 2, 0, 0, 255));
    REQUIRE(pixel_is(result, 7, 2, 255, 255, 0));
}

TEST_CASE("grid_arrange auto square layout for 3 images", "[image][grid]") {
    auto red   = make_solid(3, 3, 255, 0, 0);
    auto green = make_solid(3, 3, 0, 255, 0);
    auto blue  = make_solid(3, 3, 0, 0, 255);

    Image result = grid_arrange({red, green, blue}); // ncol=0, nrow=0 → 2×2

    REQUIRE(result.width == 6);
    REQUIRE(result.height == 6);
    // red at (0,0), green at (1,0), blue at (0,1), background fill at (1,1)
    REQUIRE(pixel_is(result, 1, 4, 255, 0, 0));
    REQUIRE(pixel_is(result, 4, 4, 0, 255, 0));
    REQUIRE(pixel_is(result, 1, 1, 0, 0, 255));
    REQUIRE(pixel_is(result, 4, 1, 255, 255, 255));  // empty cell → bg
}

TEST_CASE("grid_arrange auto rows from cols", "[image][grid]") {
    auto a = make_solid(2, 2, 255, 0, 0);
    auto b = make_solid(2, 2, 0, 255, 0);
    auto c = make_solid(2, 2, 0, 0, 255);

    Image result = grid_arrange({a, b, c}, 2, 0); // ncol=2 → nrow=2

    REQUIRE(result.width == 4);
    REQUIRE(result.height == 4);
}

TEST_CASE("grid_arrange auto cols from rows", "[image][grid]") {
    auto a = make_solid(2, 2, 255, 0, 0);
    auto b = make_solid(2, 2, 0, 255, 0);
    auto c = make_solid(2, 2, 0, 0, 255);

    Image result = grid_arrange({a, b, c}, 0, 2); // nrow=2 → ncol=2

    REQUIRE(result.width == 4);
    REQUIRE(result.height == 4);
}

TEST_CASE("grid_arrange PAD preserves pixel size on mismatched images", "[image][grid]") {
    auto big   = make_solid(6, 6, 255, 0, 0);
    auto small = make_solid(2, 2, 0, 255, 0);

    Image result = grid_arrange({big, small}, 2, 1, FitMode::PAD);

    // 1D horizontal: only heights normalized.  cell_h = max(6,2) = 6.
    // small padded to 2×6, big stays 6×6.  Output: 8×6.
    REQUIRE(result.width == 8);
    REQUIRE(result.height == 6);

    // big red at x=0..5, small green at x=6..7 (centered vertically at y=2..3)
    REQUIRE(pixel_is(result, 3, 3, 255, 0, 0));
    REQUIRE(pixel_is(result, 6, 3, 0, 255, 0));
    // white padding below small green
    REQUIRE(pixel_is(result, 6, 0, 255, 255, 255));
}

TEST_CASE("grid_arrange SCALE stretches to fill cell", "[image][grid]") {
    auto big   = make_solid(6, 6, 255, 0, 0);
    auto small = make_solid(2, 2, 0, 255, 0);

    Image result = grid_arrange({big, small}, 2, 1, FitMode::SCALE);

    REQUIRE(result.width == 12);
    REQUIRE(result.height == 6);

    // small green scaled to 6×6 → covers cell (6,0) to (11,5)
    REQUIRE(pixel_is(result, 7, 3, 0, 255, 0));
    // big red still 6×6
    REQUIRE(pixel_is(result, 3, 3, 255, 0, 0));
}

TEST_CASE("grid_arrange single image returns copy", "[image][grid]") {
    auto img = make_solid(5, 3, 128, 64, 32);
    Image result = grid_arrange({img});
    REQUIRE(result.width == 5);
    REQUIRE(result.height == 3);
    check_pixel(result, 2, 1, 128, 64, 32, 255);
}

TEST_CASE("grid_arrange empty input returns empty", "[image][grid]") {
    Image result = grid_arrange({});
    REQUIRE(result.width == 0);
    REQUIRE(result.height == 0);
}

TEST_CASE("grid_arrange custom background fills empty cells", "[image][grid]") {
    // 2×2 grid: empty cell gets custom background
    auto red = make_solid(2, 2, 255, 0, 0);

    Image result = grid_arrange({red}, 2, 2, FitMode::PAD,
                                Color(0.5f, 0.0f, 0.5f, 1.0f));

    REQUIRE(result.width == 4);
    REQUIRE(result.height == 4);
    REQUIRE(pixel_is(result, 1, 3, 255, 0, 0));   // top-left: red
    REQUIRE(pixel_is(result, 3, 3, 127, 0, 127)); // top-right: purple empty
    REQUIRE(pixel_is(result, 1, 1, 127, 0, 127)); // bottom-left: purple empty
    REQUIRE(pixel_is(result, 3, 1, 127, 0, 127)); // bottom-right: purple empty
}

// =========================================================================
//  stack_horizontal / stack_vertical
// =========================================================================

TEST_CASE("stack_horizontal three images side by side", "[image][stack]") {
    auto red   = make_solid(3, 5, 255, 0, 0);
    auto green = make_solid(4, 5, 0, 255, 0);
    auto blue  = make_solid(3, 5, 0, 0, 255);

    Image result = stack_horizontal({red, green, blue});

    // 1D horizontal: heights already match (all 5), no padding needed.
    // Widths sum naturally: 3+4+3 = 10.
    REQUIRE(result.width == 10);
    REQUIRE(result.height == 5);
    REQUIRE(pixel_is(result, 1, 2, 255, 0, 0));  // left: red (center of 3-wide)
    REQUIRE(pixel_is(result, 5, 2, 0, 255, 0));  // center: green (center of 4-wide)
    REQUIRE(pixel_is(result, 8, 2, 0, 0, 255));  // right: blue (center of 3-wide)
}

TEST_CASE("stack_horizontal mismatched heights pads vertically", "[image][stack]") {
    auto tall  = make_solid(2, 8, 255, 0, 0);
    auto short_ = make_solid(2, 4, 0, 255, 0);

    Image result = stack_horizontal({tall, short_}, FitMode::PAD,
                                    Color(0, 0, 0, 1));

    // 1D horizontal: normalize heights only. cell_h = max(8,4) = 8.
    // short is padded from 2×4 to 2×8, tall stays 2×8.  Output: 4×8.
    REQUIRE(result.width == 4);
    REQUIRE(result.height == 8);
    // short image content centered vertically: y=2..5 within its cell.
    REQUIRE(pixel_is(result, 3, 1, 0, 0, 0));     // black padding below short
    REQUIRE(pixel_is(result, 3, 3, 0, 255, 0));   // short content
    REQUIRE(pixel_is(result, 3, 6, 0, 0, 0));     // black padding above short
}

TEST_CASE("stack_vertical three images top to bottom", "[image][stack]") {
    auto red   = make_solid(5, 3, 255, 0, 0);
    auto green = make_solid(5, 4, 0, 255, 0);
    auto blue  = make_solid(5, 3, 0, 0, 255);

    Image result = stack_vertical({red, green, blue});

    // 1D vertical: widths already match (all 5), no padding needed.
    // Heights sum naturally: 3+4+3 = 10.
    REQUIRE(result.width == 5);
    REQUIRE(result.height == 10);
    // y=7..9: red, y=3..6: green, y=0..2: blue
    REQUIRE(pixel_is(result, 2, 8, 255, 0, 0));  // top: red
    REQUIRE(pixel_is(result, 2, 5, 0, 255, 0));  // middle: green
    REQUIRE(pixel_is(result, 2, 1, 0, 0, 255));  // bottom: blue
}

TEST_CASE("stack_vertical single image", "[image][stack]") {
    auto img = make_solid(5, 7, 128, 64, 32);
    Image result = stack_vertical({img});
    REQUIRE(result.width == 5);
    REQUIRE(result.height == 7);
    check_pixel(result, 2, 3, 128, 64, 32, 255);
}

TEST_CASE("stack_horizontal single image", "[image][stack]") {
    auto img = make_solid(7, 5, 64, 128, 192);
    Image result = stack_horizontal({img});
    REQUIRE(result.width == 7);
    REQUIRE(result.height == 5);
    check_pixel(result, 3, 2, 64, 128, 192, 255);
}
