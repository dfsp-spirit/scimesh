#include "catch_amalgamated.hpp"
#include "image.h"
#include "to_string.h"
#include <sstream>

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
