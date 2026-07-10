#include "catch_amalgamated.hpp"
#include "image.h"

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
