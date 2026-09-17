// Text labels (TextLayer / Font / draw_text / world_to_screen).
//
// Text layers draw strings with a TrueType font, either at a location in the 3D
// scene (TextSpace::WORLD, projected with the camera and optionally hidden by
// geometry in front of the anchor) or at a fixed position in the output image
// (TextSpace::SCREEN).  These tests cover the font API (metrics, measurement,
// UTF-8, glyph coverage), the low-level draw_text() primitive including its
// orientation, and the placement/occlusion behaviour of the renderer pass.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/text.h>
#include <scimesh/font.h>
#include <scimesh/mesh.h>
#include <scimesh/image.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using Catch::Approx;

namespace {

/// Statistics over the pixels of an image that differ from the background.
///
/// Used to check *where* text was drawn without depending on exact glyph
/// shapes: the ink bounding box, its centre of mass and the pixel count.
struct InkStats {
    int count = 0;
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = -1;
    int max_y = -1;
    double sum_x = 0.0;
    double sum_y = 0.0;

    bool empty() const { return count == 0; }
    double mean_x() const { return count > 0 ? sum_x / count : 0.0; }
    double mean_y() const { return count > 0 ? sum_y / count : 0.0; }
    int width() const { return empty() ? 0 : max_x - min_x + 1; }
    int height() const { return empty() ? 0 : max_y - min_y + 1; }
};

/// Count "dark" pixels, i.e. anything that is not the white background.
InkStats ink(const Image &img, int threshold = 250) {
    InkStats stats;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r < threshold || g < threshold || b < threshold) {
                ++stats.count;
                stats.min_x = std::min(stats.min_x, x);
                stats.max_x = std::max(stats.max_x, x);
                stats.min_y = std::min(stats.min_y, y);
                stats.max_y = std::max(stats.max_y, y);
                stats.sum_x += x;
                stats.sum_y += y;
            }
        }
    }
    return stats;
}

/// Count pixels that are "close to" the given color (per-channel tolerance).
int count_color(const Image &img, const Color &color, int tolerance = 24) {
    const int tr = static_cast<int>(std::lround(color.r * 255.0f));
    const int tg = static_cast<int>(std::lround(color.g * 255.0f));
    const int tb = static_cast<int>(std::lround(color.b * 255.0f));
    int count = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (std::abs(static_cast<int>(r) - tr) <= tolerance &&
                std::abs(static_cast<int>(g) - tg) <= tolerance &&
                std::abs(static_cast<int>(b) - tb) <= tolerance) {
                ++count;
            }
        }
    }
    return count;
}

/// Number of pixels in which two equally sized images differ noticeably.
///
/// Used to isolate exactly those pixels a label contributed: rendering the same
/// scene with and without a label is deterministic, so any difference must come
/// from the text.
int pixel_differences(const Image &a, const Image &b, int tolerance = 8) {
    REQUIRE(a.width == b.width);
    REQUIRE(a.height == b.height);
    int count = 0;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            uint8_t ar, ag, ab, aa, br, bg, bb, ba;
            a.get_pixel(x, y, ar, ag, ab, aa);
            b.get_pixel(x, y, br, bg, bb, ba);
            if (std::abs(static_cast<int>(ar) - static_cast<int>(br)) > tolerance ||
                std::abs(static_cast<int>(ag) - static_cast<int>(bg)) > tolerance ||
                std::abs(static_cast<int>(ab) - static_cast<int>(bb)) > tolerance ||
                std::abs(static_cast<int>(aa) - static_cast<int>(ba)) > tolerance) {
                ++count;
            }
        }
    }
    return count;
}

/// Compare two colors channel by channel (Color has no operator==).
bool same_color(const Color &a, const Color &b) {
    return a.r == Approx(b.r) && a.g == Approx(b.g) && a.b == Approx(b.b) &&
           a.a == Approx(b.a);
}

/// The bundled font (SCIMESH_DEFAULT_FONT) at the given pixel size.
Font test_font(float size = 16.0f) {
    return cached_font("", size);
}

Image white_image(int size = 128) {
    Image img(size, size);
    img.clear(255, 255, 255, 255);
    return img;
}

RenderOptions text_options(int width = 128, int height = 128,
                           const Color &background = Color(1.0f, 1.0f, 1.0f, 1.0f)) {
    RenderOptions opts;
    opts.width = width;
    opts.height = height;
    opts.background_color = background;
    return opts;
}

Camera front_camera(float distance = 4.0f) {
    Camera cam;
    cam.eye = Vec3(0.0f, 0.0f, distance);
    cam.center = Vec3(0.0f, 0.0f, 0.0f);
    cam.up = Vec3(0.0f, 1.0f, 0.0f);
    cam.fov_degrees = 45.0f;
    return cam;
}

/// A single-line black label layer at the given pixel position (screen space).
TextLayer screen_label(const std::string &text, float x, float y, float size,
                       const Vec2 &adj = Vec2(0.0f, 0.0f)) {
    TextLayer layer;
    layer.strings = {text};
    layer.positions = {Vec3(x, y, 0.0f)};
    layer.size = size;
    layer.space = TextSpace::SCREEN;
    layer.adj = adj;
    layer.colors = {Color(0.0f, 0.0f, 0.0f, 1.0f)};
    layer.depth_test = false;
    return layer;
}

} // namespace

// ---------------------------------------------------------------------------
//  Font API
// ---------------------------------------------------------------------------

TEST_CASE("Font: the bundled font loads and reports sensible metrics", "[font]") {
    const Font font = test_font(16.0f);

    REQUIRE(font.valid());
    REQUIRE_FALSE(font.source_path().empty());  // bundled font was found
    REQUIRE(font.source_path().find("Inter-Regular.ttf") != std::string::npos);
    REQUIRE(font.family_name() == "Inter");
    REQUIRE(font.pixel_size() == Approx(16.0f));

    const FontMetrics m = font.metrics();
    REQUIRE(m.ascent > 0.0f);
    REQUIRE(m.descent >= 0.0f);
    REQUIRE(m.box_height() > 0.0f);
    REQUIRE(m.line_height() >= m.box_height());
    // An em box of 16 px should give an ascent somewhere in the usual range of
    // text fonts (roughly 0.7 to 1.1 em), not something wildly scaled.
    REQUIRE(m.ascent > 8.0f);
    REQUIRE(m.ascent < 20.0f);
}

TEST_CASE("Font: metrics and text width scale with the pixel size", "[font]") {
    const Font small = test_font(12.0f);
    const Font large = test_font(36.0f);

    REQUIRE(large.metrics().ascent > small.metrics().ascent);
    REQUIRE(large.measure("anterior") > small.measure("anterior"));

    // Tripling the size triples the advance (the scale is linear in size).
    const float ratio = large.measure("anterior") / small.measure("anterior");
    REQUIRE(ratio == Approx(3.0f).epsilon(0.02));
}

TEST_CASE("Font: invalid input is rejected with a useful error", "[font]") {
    REQUIRE_THROWS_AS(Font::from_file("does_not_exist.ttf", 16.0f), std::runtime_error);
    REQUIRE_THROWS_AS(Font::from_file("does_not_exist.ttf", 0.0f), std::invalid_argument);
    REQUIRE_THROWS_AS(Font::from_memory({}, 16.0f), std::runtime_error);

    const Font invalid;
    REQUIRE_FALSE(invalid.valid());
    REQUIRE(invalid.pixel_size() == 0.0f);
    REQUIRE(invalid.glyph('A') == nullptr);
    REQUIRE(invalid.measure("A") == 0.0f);
}

TEST_CASE("Font: with_pixel_size resizes without reloading from disk", "[font]") {
    const Font base = test_font(10.0f);
    const Font scaled = base.with_pixel_size(30.0f);

    REQUIRE(scaled.valid());
    REQUIRE(scaled.pixel_size() == Approx(30.0f));
    REQUIRE(scaled.source_path() == base.source_path());
    REQUIRE(scaled.family_name() == base.family_name());
    REQUIRE(scaled.metrics().ascent == Approx(3.0f * base.metrics().ascent).epsilon(0.01));

    // An invalid font stays invalid.
    REQUIRE_FALSE(Font().with_pixel_size(20.0f).valid());
}

TEST_CASE("Font: the cache hands out usable fonts and can be cleared", "[font]") {
    const Font first = cached_font("", 20.0f);
    const Font second = cached_font("", 20.0f);
    REQUIRE(first.valid());
    REQUIRE(second.valid());
    REQUIRE(first.source_path() == second.source_path());
    REQUIRE(first.measure("test") == Approx(second.measure("test")));

    // Fonts already handed out keep working when the cache is emptied.
    clear_font_cache();
    REQUIRE(first.measure("test") > 0.0f);
    REQUIRE(first.glyph('A') != nullptr);
}

TEST_CASE("Font: UTF-8 decoding and line counting", "[font]") {
    using CodePoints = std::vector<uint32_t>;

    REQUIRE(Font::decode_utf8("") == CodePoints{});
    REQUIRE(Font::decode_utf8("A") == CodePoints{65});
    REQUIRE(Font::decode_utf8("ab") == CodePoints{97, 98});
    // U+00B0 DEGREE SIGN followed by "C", as in a temperature axis label.
    REQUIRE(Font::decode_utf8("\xC2\xB0""C") == CodePoints{0xB0, 67});
    // U+20AC EURO SIGN (three bytes).
    REQUIRE(Font::decode_utf8("\xE2\x82\xAC") == CodePoints{0x20AC});
    // Invalid bytes decode to the replacement character instead of throwing.
    REQUIRE(Font::decode_utf8("\xFF") == CodePoints{0xFFFD});
    REQUIRE(Font::decode_utf8("\xC2") == CodePoints{0xFFFD});

    REQUIRE(Font::line_count("") == 1);
    REQUIRE(Font::line_count("abc") == 1);
    REQUIRE(Font::line_count("a\nb") == 2);
    REQUIRE(Font::line_count("a\n") == 2);
}

TEST_CASE("Font: glyph coverage bitmaps and advances", "[font]") {
    const Font font = test_font(24.0f);

    const GlyphBitmap *a = font.glyph('A');
    REQUIRE(a != nullptr);
    REQUIRE_FALSE(a->empty());
    REQUIRE(a->width > 0);
    REQUIRE(a->height > 0);
    REQUIRE(a->advance > 0.0f);
    REQUIRE(static_cast<int>(a->coverage.size()) == a->width * a->height);

    // The bitmap is anti-aliased: it has fully covered pixels, partially
    // covered ones, and empty ones.
    int full = 0, partial = 0, empty = 0;
    for (uint8_t coverage : a->coverage) {
        if (coverage == 255) {
            ++full;
        } else if (coverage == 0) {
            ++empty;
        } else {
            ++partial;
        }
    }
    REQUIRE(full > 0);
    REQUIRE(empty > 0);
    REQUIRE(partial > 0);

    // A space has an advance but no pixels.
    const GlyphBitmap *space = font.glyph(' ');
    REQUIRE(space != nullptr);
    REQUIRE(space->empty());
    REQUIRE(space->advance > 0.0f);

    // Measuring a single glyph equals the glyph's advance, and measuring two
    // glyphs adds up (including whatever kerning the font provides).
    REQUIRE(font.measure("A") == Approx(a->advance).margin(1e-4));
    const float expected = a->advance + font.kerning('A', 'A') + a->advance;
    REQUIRE(font.measure("AA") == Approx(expected).margin(1e-3));

    // Repeated access returns the cached bitmap (same address).
    REQUIRE(font.glyph('A') == a);

    // Kerning is a finite adjustment, whatever the font provides.
    REQUIRE(std::isfinite(font.kerning('A', 'V')));
    REQUIRE(std::isfinite(font.kerning(' ', ' ')));
}

// ---------------------------------------------------------------------------
//  draw_text: the low-level, screen-space primitive
// ---------------------------------------------------------------------------

TEST_CASE("draw_text: ink lands where the baseline and pen position say", "[text]") {
    const Font font = test_font(24.0f);
    const FontMetrics m = font.metrics();
    Image img = white_image(160);

    const float pen_x = 30.0f;
    const float baseline = 80.0f;
    TextDrawStyle style;
    style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
    draw_text(img, "T", font, pen_x, baseline, style);

    const InkStats stats = ink(img);
    REQUIRE_FALSE(stats.empty());
    // The glyph starts at the pen position (allow the side bearing).
    REQUIRE(stats.min_x >= static_cast<int>(pen_x));
    REQUIRE(stats.min_x < static_cast<int>(pen_x) + 6);
    // ...and it is one glyph wide, not more.
    REQUIRE(stats.width() < static_cast<int>(font.measure("T")) + 4);
    // "T" sits entirely above the baseline: nothing below it, and its ink stays
    // inside the line box (the ascent is the top of the box, which is a bit
    // higher than the cap height of the glyph).  This catches a vertical flip.
    REQUIRE(stats.max_y <= static_cast<int>(baseline));
    REQUIRE(stats.min_y < static_cast<int>(baseline) - 10);
    REQUIRE(stats.min_y >= static_cast<int>(baseline - m.ascent) - 1);
}

TEST_CASE("draw_text: descenders go below the baseline", "[text]") {
    const Font font = test_font(24.0f);
    const float baseline = 60.0f;

    Image g_img = white_image(120);
    TextDrawStyle style;
    draw_text(g_img, "g", font, 20.0f, baseline, style);
    const InkStats g = ink(g_img);
    REQUIRE_FALSE(g.empty());
    REQUIRE(g.max_y > static_cast<int>(baseline));  // descender below baseline
    REQUIRE(g.min_y < static_cast<int>(baseline));  // body above baseline

    // A capital letter must not reach below the baseline at all.
    Image t_img = white_image(120);
    draw_text(t_img, "T", font, 20.0f, baseline, style);
    const InkStats t = ink(t_img);
    REQUIRE_FALSE(t.empty());
    REQUIRE(t.max_y <= static_cast<int>(baseline));
}

TEST_CASE("draw_text: multiple lines are stacked downwards", "[text]") {
    const Font font = test_font(20.0f);
    const float baseline = 40.0f;

    Image single = white_image(160);
    Image multi = white_image(160);
    TextDrawStyle style;
    draw_text(single, "H", font, 20.0f, baseline, style);
    draw_text(multi, "H\nH", font, 20.0f, baseline, style);

    const InkStats one = ink(single);
    const InkStats two = ink(multi);
    REQUIRE_FALSE(one.empty());
    REQUIRE_FALSE(two.empty());
    REQUIRE(two.count > one.count);                    // twice the ink ...
    REQUIRE(two.count < 3 * one.count);
    REQUIRE(two.height() > one.height());              // ... spread vertically
    REQUIRE(two.max_y > one.max_y);
    // Line spacing 1.2 of the box height, i.e. the second line "H" starts lower.
    REQUIRE(two.max_y >= static_cast<int>(baseline + font.metrics().box_height()));

    // A trailing newline does not add an empty line.
    Image trailing = white_image(160);
    draw_text(trailing, "H\n", font, 20.0f, baseline, style);
    const InkStats clipped = ink(trailing);
    REQUIRE(clipped.count == one.count);

    // '\r' is ignored, so Windows line endings behave like Unix ones.
    Image crlf = white_image(160);
    draw_text(crlf, "H\r\nH", font, 20.0f, baseline, style);
    REQUIRE(ink(crlf).max_y == two.max_y);
}

TEST_CASE("draw_text: color, alpha and halo", "[text]") {
    const Font font = test_font(30.0f);

    Image black_text = white_image(120);
    TextDrawStyle style;
    style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
    draw_text(black_text, "H", font, 20.0f, 70.0f, style);
    const int black_pixels = count_color(black_text, Color(0.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(black_pixels > 0);

    // Half-transparent black on white gives gray, not black.
    Image translucent = white_image(120);
    style.color = Color(0.0f, 0.0f, 0.0f, 0.5f);
    draw_text(translucent, "H", font, 20.0f, 70.0f, style);
    REQUIRE(count_color(translucent, Color(0.0f, 0.0f, 0.0f, 1.0f), 8) == 0);
    REQUIRE(count_color(translucent, Color(0.5f, 0.5f, 0.5f, 1.0f), 24) > 0);

    // A halo adds pixels around the glyph and shows up in its own color.
    Image haloed = white_image(120);
    style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
    style.halo_color = Color(1.0f, 0.0f, 0.0f, 1.0f);
    style.halo_width = 2.0f;
    draw_text(haloed, "H", font, 20.0f, 70.0f, style);
    const InkStats plain = ink(black_text);
    const InkStats with_halo = ink(haloed);
    REQUIRE(with_halo.count > plain.count);
    REQUIRE(count_color(haloed, Color(1.0f, 0.0f, 0.0f, 1.0f), 40) > 0);
    REQUIRE(with_halo.min_x < plain.min_x);
    REQUIRE(with_halo.max_x > plain.max_x);
}

TEST_CASE("draw_text: clipping at the image border is safe", "[text]") {
    const Font font = test_font(30.0f);
    Image img = white_image(64);
    TextDrawStyle style;
    style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);

    // Partially and fully outside: must not throw or corrupt memory.
    draw_text(img, "clipped", font, -20.0f, 10.0f, style);
    draw_text(img, "clipped", font, 60.0f, 60.0f, style);
    draw_text(img, "clipped", font, -200.0f, -200.0f, style);
    REQUIRE(ink(img).count >= 0);

    REQUIRE_THROWS_AS(draw_text(img, "x", Font(), 0.0f, 0.0f, style),
                      std::invalid_argument);
}

// ---------------------------------------------------------------------------
//  TextLayer in a Scene
// ---------------------------------------------------------------------------

TEST_CASE("TextLayer: scene bookkeeping", "[text]") {
    Scene scene;
    REQUIRE(scene.text_count() == 0);
    REQUIRE(scene.text_nodes().empty());

    TextLayer layer = screen_label("A", 10.0f, 20.0f, 16.0f);
    scene.add_texts(layer, Mat4(1.0f), "annotations");
    REQUIRE(scene.text_count() == 1);
    REQUIRE(scene.text_nodes().size() == 1);
    REQUIRE(scene.text_node(0).layer == &scene.texts[0]);
    REQUIRE(scene.text_name(0) == "annotations");
    REQUIRE(scene.text_transform(0) == Mat4(1.0f));
    // Out-of-range lookups fall back to defaults instead of crashing.
    REQUIRE(scene.text_name(7).empty());
    REQUIRE(scene.text_transform(7) == Mat4(1.0f));
}

TEST_CASE("TextLayer: labels are ignored by the camera framing", "[text]") {
    Scene scene;
    scene.add(make_unit_cube());
    Vec3 min_before, max_before;
    scene.compute_bounding_box(min_before, max_before);

    TextLayer huge;
    huge.strings = {"a very long label that is far outside the mesh"};
    huge.positions = {Vec3(500.0f, 500.0f, 500.0f)};
    huge.size = 80.0f;
    scene.add_texts(huge);

    Vec3 min_after, max_after;
    scene.compute_bounding_box(min_after, max_after);
    REQUIRE(min_after == min_before);
    REQUIRE(max_after == max_before);

    // The camera fit therefore does not change either.
    Scene without = scene;
    without.texts.clear();
    const Camera cam_with = camera_fit_scene(scene, Vec3(0, 0, 1), Vec3(0, 1, 0), 45.0f);
    const Camera cam_without = camera_fit_scene(without, Vec3(0, 0, 1), Vec3(0, 1, 0), 45.0f);
    REQUIRE(cam_with.eye == cam_without.eye);
}

TEST_CASE("TextLayer: a scene with labels only renders the labels", "[text]") {
    Scene scene;
    scene.add_texts(screen_label("ANNO", 20.0f, 40.0f, 24.0f, Vec2(0.0f, 1.0f)));

    Renderer renderer;
    const Camera cam = front_camera();
    const Image img = renderer.render_scene(scene, cam, text_options(200, 80));

    REQUIRE(img.width == 200);
    REQUIRE(img.height == 80);
    const InkStats stats = ink(img);
    REQUIRE_FALSE(stats.empty());
    // The label starts near x = 20 and stays within the image.
    REQUIRE(stats.min_x >= 20);
    REQUIRE(stats.min_x <= 24);
    REQUIRE(stats.max_x < 200);
    REQUIRE(stats.max_y < 80);
}

TEST_CASE("TextLayer: count() and color_or() follow the LineLayer conventions", "[text]") {
    TextLayer layer;
    layer.strings = {"a", "b", "c"};
    layer.positions = {Vec3(0, 0, 0), Vec3(1, 0, 0)};
    REQUIRE(layer.count() == 2);  // limited by the shorter array

    const Color fallback(0.25f, 0.25f, 0.25f, 1.0f);
    REQUIRE(same_color(layer.color_or(0, fallback), fallback));  // no colors at all

    layer.colors = {Color(1, 0, 0, 1), Color(0, 1, 0, 1)};
    REQUIRE(same_color(layer.color_or(0, fallback), Color(1, 0, 0, 1)));
    REQUIRE(same_color(layer.color_or(1, fallback), Color(0, 1, 0, 1)));
    // The last color repeats for labels beyond the end of the array.
    REQUIRE(same_color(layer.color_or(9, fallback), Color(0, 1, 0, 1)));

    layer.strings.clear();
    REQUIRE(layer.empty());
    REQUIRE(layer.position_or(0) == Vec3(0.0f));
    REQUIRE(layer.string_or(0).empty());
}

// ---------------------------------------------------------------------------
//  Placement: screen space and world space
// ---------------------------------------------------------------------------

TEST_CASE("TextLayer: screen-space placement is independent of anti-aliasing", "[text]") {
    Renderer renderer;
    const Camera cam = front_camera();

    // The same layer, rendered with 1x and 4x supersampling: the label must
    // land at the same pixels because screen-space positions are final-image
    // pixels, and `size` is a final-image size.
    std::vector<InkStats> stats;
    for (int aa : {1, 4}) {
        Scene scene;
        scene.add_texts(screen_label("H", 40.0f, 60.0f, 24.0f, Vec2(0.0f, 1.0f)));
        RenderOptions opts = text_options(200, 200);
        opts.aa_samples = aa;
        const Image img = renderer.render_scene(scene, cam, opts);
        REQUIRE(img.width == 200);
        stats.push_back(ink(img));
    }
    REQUIRE_FALSE(stats[0].empty());
    REQUIRE_FALSE(stats[1].empty());
    REQUIRE(stats[0].min_x == stats[1].min_x);        // exact for the anchor
    REQUIRE(std::abs(stats[0].min_y - stats[1].min_y) <= 1);
    REQUIRE(std::abs(stats[0].height() - stats[1].height()) <= 2);
    // The physically meaningful size is the same in both renders.
    REQUIRE(stats[1].count > stats[0].count / 2);
}

TEST_CASE("TextLayer: adj anchors the text box on the position", "[text]") {
    Renderer renderer;
    const Camera cam = front_camera();
    const float anchor_x = 100.0f, anchor_y = 100.0f;
    const FontMetrics m = test_font(24.0f).metrics();

    // Bottom left anchor: the left edge and the bottom edge of the text *box*
    // sit on the anchor, so the ink is to the right of it and above it.  ("H"
    // has no descender, so its ink ends one descent above the box bottom.)
    Scene bottom_left;
    bottom_left.add_texts(screen_label("H", anchor_x, anchor_y, 24.0f, Vec2(0.0f, 0.0f)));
    const InkStats bl = ink(renderer.render_scene(bottom_left, cam, text_options(200, 200)));
    REQUIRE_FALSE(bl.empty());
    REQUIRE(std::abs(bl.min_x - anchor_x) <= 3);
    REQUIRE(bl.max_y <= static_cast<int>(anchor_y) + 1);
    REQUIRE(bl.max_y >= static_cast<int>(anchor_y - m.descent) - 2);
    REQUIRE(bl.min_y < anchor_y - 10.0f);
    REQUIRE(bl.max_x > anchor_x + 10.0f);

    // Top right anchor: the right edge and the top edge of the box sit on the
    // anchor, so the ink ends at it (x) and starts below it (y).
    Scene top_right;
    top_right.add_texts(screen_label("H", anchor_x, anchor_y, 24.0f, Vec2(1.0f, 1.0f)));
    const InkStats tr = ink(renderer.render_scene(top_right, cam, text_options(200, 200)));
    REQUIRE_FALSE(tr.empty());
    REQUIRE(std::abs(tr.max_x - anchor_x) <= 3);
    REQUIRE(tr.min_y >= static_cast<int>(anchor_y) - 1);
    REQUIRE(tr.min_y <= static_cast<int>(anchor_y + m.ascent) + 1);
    REQUIRE(tr.max_y > anchor_y + 10.0f);
    REQUIRE(tr.min_x < anchor_x - 10.0f);

    // Centred (the default): ink straddles the anchor in both directions.
    Scene centred;
    centred.add_texts(screen_label("H", anchor_x, anchor_y, 24.0f, Vec2(0.5f, 0.5f)));
    const InkStats c = ink(renderer.render_scene(centred, cam, text_options(200, 200)));
    REQUIRE_FALSE(c.empty());
    REQUIRE(c.min_x < anchor_x);
    REQUIRE(c.max_x > anchor_x);
    REQUIRE(c.min_y < anchor_y);
    REQUIRE(c.max_y > anchor_y);
}

TEST_CASE("TextLayer: pixel offset shifts a label", "[text]") {
    Renderer renderer;
    const Camera cam = front_camera();
    const RenderOptions opts = text_options(200, 200);

    Scene plain;
    plain.add_texts(screen_label("H", 50.0f, 100.0f, 24.0f, Vec2(0.0f, 1.0f)));
    const InkStats base = ink(renderer.render_scene(plain, cam, opts));

    TextLayer shifted = screen_label("H", 50.0f, 100.0f, 24.0f, Vec2(0.0f, 1.0f));
    shifted.offset = Vec2(12.0f, 7.0f);
    Scene with_offset;
    with_offset.add_texts(shifted);
    const InkStats moved = ink(renderer.render_scene(with_offset, cam, opts));

    REQUIRE_FALSE(base.empty());
    REQUIRE_FALSE(moved.empty());
    REQUIRE(std::abs((moved.min_x - base.min_x) - 12) <= 1);
    REQUIRE(std::abs((moved.min_y - base.min_y) - 7) <= 1);
}

TEST_CASE("TextLayer: world-space labels are placed by the projection", "[text]") {
    Renderer renderer;
    const Camera cam = front_camera(4.0f);
    const RenderOptions opts = text_options(200, 200, Color(1.0f, 1.0f, 1.0f, 1.0f));

    // A label at the origin, with the camera looking at the origin: the ink
    // must be centred in the image.
    TextLayer centre;
    centre.strings = {"H"};
    centre.positions = {Vec3(0.0f, 0.0f, 0.0f)};
    centre.size = 24.0f;
    centre.adj = Vec2(0.5f, 0.5f);
    centre.colors = {Color(0.0f, 0.0f, 0.0f, 1.0f)};
    Scene scene;
    scene.add_texts(centre);
    const InkStats mid = ink(renderer.render_scene(scene, cam, opts));
    REQUIRE_FALSE(mid.empty());
    REQUIRE(std::abs(mid.mean_x() - 100.0) <= 3.0);

    // The renderer's placement agrees with the public world_to_screen() helper.
    const ProjectedPoint p = world_to_screen(cam, Vec3(0.0f, 0.0f, 0.0f), 200, 200,
                                            ProjectionType::PERSPECTIVE, 0.1f, 100.0f);
    REQUIRE(p.in_front);
    REQUIRE(mid.min_x < static_cast<int>(p.pixel.x));
    REQUIRE(mid.max_x > static_cast<int>(p.pixel.x));

    // A label to the right in world space ends up on the right of the image.
    TextLayer right;
    right.strings = {"H"};
    right.positions = {Vec3(1.0f, 0.0f, 0.0f)};
    right.size = 24.0f;
    right.colors = {Color(0.0f, 0.0f, 0.0f, 1.0f)};
    Scene right_scene;
    right_scene.add_texts(right);
    const InkStats r = ink(renderer.render_scene(right_scene, cam, opts));
    REQUIRE_FALSE(r.empty());
    REQUIRE(r.mean_x() > 120.0);
}

TEST_CASE("TextLayer: a layer transform moves world-space anchors", "[text]") {
    Renderer renderer;
    const Camera cam = front_camera(4.0f);
    const RenderOptions opts = text_options(200, 200);

    TextLayer layer;
    layer.strings = {"H"};
    layer.positions = {Vec3(0.0f, 0.0f, 0.0f)};
    layer.size = 24.0f;
    layer.adj = Vec2(0.5f, 0.5f);
    layer.colors = {Color(0.0f, 0.0f, 0.0f, 1.0f)};

    Scene scene;
    scene.add_texts(layer);
    const InkStats base = ink(renderer.render_scene(scene, cam, opts));

    Scene shifted;
    shifted.add_texts(layer,
                      glm::translate(Mat4(1.0f), Vec3(1.5f, 0.0f, 0.0f)));
    const InkStats moved = ink(renderer.render_scene(shifted, cam, opts));

    REQUIRE_FALSE(base.empty());
    REQUIRE_FALSE(moved.empty());
    REQUIRE(moved.mean_x() > base.mean_x() + 20.0);

    // Screen-space layers ignore the transform (their positions are pixels).
    Scene screen_shifted;
    screen_shifted.add_texts(screen_label("H", 40.0f, 40.0f, 24.0f, Vec2(0.0f, 1.0f)),
                             glm::translate(Mat4(1.0f), Vec3(50.0f, 50.0f, 0.0f)));
    Scene screen_plain;
    screen_plain.add_texts(screen_label("H", 40.0f, 40.0f, 24.0f, Vec2(0.0f, 1.0f)));
    REQUIRE(ink(renderer.render_scene(screen_shifted, cam, opts)).min_x ==
            ink(renderer.render_scene(screen_plain, cam, opts)).min_x);
}

// ---------------------------------------------------------------------------
//  Occlusion (depth_test)
// ---------------------------------------------------------------------------

TEST_CASE("TextLayer: depth_test hides labels behind the geometry", "[text][depth]") {
    Renderer renderer;
    const Camera cam = front_camera(4.0f);
    const RenderOptions opts = text_options(160, 160);

    // The cube spans [-1, 1]^3 and the camera looks at it from +Z.  Two renders
    // of the same scene, with and without the label, isolate exactly the pixels
    // the label contributed.
    const auto label_pixels = [&](const Vec3 &anchor, bool depth_test) {
        Scene without;
        without.add(make_unit_cube());

        Scene with = without;
        TextLayer layer;
        layer.strings = {"H"};
        layer.positions = {anchor};
        layer.size = 20.0f;
        layer.adj = Vec2(0.5f, 0.5f);
        layer.colors = {Color(0.0f, 0.0f, 0.0f, 1.0f)};
        layer.depth_test = depth_test;
        with.add_texts(layer);

        return pixel_differences(renderer.render_scene(with, cam, opts),
                                 renderer.render_scene(without, cam, opts));
    };

    // In front of the cube's front face (z = 1): visible either way.
    REQUIRE(label_pixels(Vec3(0.0f, 0.0f, 2.0f), true) > 5);
    REQUIRE(label_pixels(Vec3(0.0f, 0.0f, 2.0f), false) > 5);

    // Inside the cube: hidden with depth_test, drawn without it.
    REQUIRE(label_pixels(Vec3(0.0f, 0.0f, 0.0f), false) > 5);
    REQUIRE(label_pixels(Vec3(0.0f, 0.0f, 0.0f), true) == 0);

    // Off to the side and clearly in front of the cube's front face: visible,
    // because the depth test only compares against what is at that pixel.
    REQUIRE(label_pixels(Vec3(0.7f, 0.0f, 2.0f), true) > 5);

    // The interesting case: the anchor is *outside* the mesh, but on a ray that
    // passes through the cube (the ray from the eye to (1.2, 0, 0) crosses the
    // front face at x = 0.9), so the label is correctly hidden by the cube.
    REQUIRE(label_pixels(Vec3(1.2f, 0.0f, 0.0f), true) == 0);
    REQUIRE(label_pixels(Vec3(1.2f, 0.0f, 0.0f), false) > 5);
}

TEST_CASE("TextLayer: unprojectable and off-screen anchors are skipped safely",
          "[text][depth]") {
    Renderer renderer;
    const Camera cam = front_camera(4.0f);
    const RenderOptions opts = text_options(120, 120);

    // A world-space label whose anchor is behind the camera (z = 10 with the
    // camera at z = 4 looking towards -Z) must not be drawn, and must not crash
    // or leave stray pixels behind.
    const auto only_label_difference = [&](const Vec3 &anchor) {
        Scene without;
        Scene with;
        TextLayer layer;
        layer.strings = {"H"};
        layer.positions = {anchor};
        layer.size = 20.0f;
        layer.colors = {Color(0.0f, 0.0f, 0.0f, 1.0f)};
        with.add_texts(layer);
        return pixel_differences(renderer.render_scene(with, cam, opts),
                                 renderer.render_scene(without, cam, opts));
    };

    REQUIRE(only_label_difference(Vec3(0.0f, 0.0f, 10.0f)) == 0);    // behind
    REQUIRE(only_label_difference(Vec3(50.0f, 0.0f, 0.0f)) == 0);    // off screen
    REQUIRE(only_label_difference(Vec3(0.0f, 0.0f, 0.0f)) > 5);      // control
}

// ---------------------------------------------------------------------------
//  Colors and font selection
// ---------------------------------------------------------------------------

TEST_CASE("TextLayer: default color and per-label colors", "[text]") {
    Renderer renderer;
    const Camera cam = front_camera();

    TextLayer layer;
    layer.strings = {"H", "H", "H"};
    layer.positions = {Vec3(20.0f, 40.0f, 0.0f), Vec3(80.0f, 40.0f, 0.0f),
                       Vec3(140.0f, 40.0f, 0.0f)};
    layer.size = 24.0f;
    layer.space = TextSpace::SCREEN;
    layer.adj = Vec2(0.0f, 1.0f);
    layer.depth_test = false;
    layer.colors = {Color(1.0f, 0.0f, 0.0f, 1.0f), Color(0.0f, 0.0f, 1.0f, 1.0f)};

    Scene scene;
    scene.add_texts(layer);
    const Image img = renderer.render_scene(scene, cam, text_options(200, 80));

    // Label 0 is red, label 1 blue, and label 2 reuses the last color (blue).
    Image left(img.width / 2, img.height), right(img.width / 2, img.height);
    left.clear(0, 0, 0, 255);
    right.clear(0, 0, 0, 255);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width / 2; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            left.set_pixel(x, y, r, g, b, a);
            img.get_pixel(x + img.width / 2, y, r, g, b, a);
            right.set_pixel(x, y, r, g, b, a);
        }
    }
    REQUIRE(count_color(left, Color(1.0f, 0.0f, 0.0f, 1.0f), 60) > 0);
    REQUIRE(count_color(right, Color(0.0f, 0.0f, 1.0f, 1.0f), 60) > 0);

    // Without own colors, the labels use RenderOptions::default_color.
    TextLayer plain = screen_label("H", 40.0f, 60.0f, 24.0f, Vec2(0.0f, 1.0f));
    plain.colors.clear();
    Scene plain_scene;
    plain_scene.add_texts(plain);
    RenderOptions opts = text_options(160, 160);
    opts.default_color = Color(0.0f, 0.6f, 0.0f, 1.0f);
    const Image green_img = renderer.render_scene(plain_scene, cam, opts);
    REQUIRE(count_color(green_img, Color(0.0f, 0.6f, 0.0f, 1.0f), 60) > 0);
}

TEST_CASE("TextLayer: another font file can be used", "[text]") {
    // The bundled font and the same font loaded explicitly must agree.
    const Font bundled = test_font(20.0f);
    const Font explicit_font = cached_font(bundled.source_path(), 20.0f);
    REQUIRE(explicit_font.measure("anterior") == Approx(bundled.measure("anterior")));

    // A text layer with an explicit font file renders.
    TextLayer layer = screen_label("H", 30.0f, 60.0f, 20.0f, Vec2(0.0f, 1.0f));
    layer.font_file = bundled.source_path();
    Scene scene;
    scene.add_texts(layer);
    Renderer renderer;
    REQUIRE_FALSE(ink(renderer.render_scene(scene, front_camera(), text_options(160, 160))).empty());

    // A missing font file fails loudly instead of rendering nothing.
    TextLayer broken = layer;
    broken.font_file = "no_such_font.ttf";
    Scene broken_scene;
    broken_scene.add_texts(broken);
    REQUIRE_THROWS_AS(renderer.render_scene(broken_scene, front_camera(), text_options(64, 64)),
                      std::runtime_error);
}

// ---------------------------------------------------------------------------
//  world_to_screen()
// ---------------------------------------------------------------------------

TEST_CASE("world_to_screen: matches the rendered image geometry", "[text][camera]") {
    const Camera cam = front_camera(4.0f);

    // The camera centre maps to the centre of the image.
    const ProjectedPoint centre =
        world_to_screen(cam, Vec3(0.0f, 0.0f, 0.0f), 200, 100,
                        ProjectionType::PERSPECTIVE, 0.1f, 100.0f);
    REQUIRE(centre.in_front);
    REQUIRE(centre.pixel.x == Approx(100.0f).margin(0.5));
    REQUIRE(centre.pixel.y == Approx(50.0f).margin(0.5));

    // +Y in world space is up, and screen y grows downwards.
    const ProjectedPoint above =
        world_to_screen(cam, Vec3(0.0f, 0.5f, 0.0f), 200, 100,
                        ProjectionType::PERSPECTIVE, 0.1f, 100.0f);
    REQUIRE(above.in_front);
    REQUIRE(above.pixel.y < 50.0f);

    // +X is to the right.
    const ProjectedPoint right =
        world_to_screen(cam, Vec3(0.5f, 0.0f, 0.0f), 200, 100,
                        ProjectionType::PERSPECTIVE, 0.1f, 100.0f);
    REQUIRE(right.in_front);
    REQUIRE(right.pixel.x > 100.0f);

    // Closer points have a smaller NDC depth.
    const ProjectedPoint near_point =
        world_to_screen(cam, Vec3(0.0f, 0.0f, 1.0f), 200, 100,
                        ProjectionType::PERSPECTIVE, 0.1f, 100.0f);
    REQUIRE(near_point.depth < centre.depth);

    // A point behind the camera is flagged, not silently projected.
    const ProjectedPoint behind =
        world_to_screen(cam, Vec3(0.0f, 0.0f, 10.0f), 200, 100,
                        ProjectionType::PERSPECTIVE, 0.1f, 100.0f);
    REQUIRE_FALSE(behind.in_front);

    // Orthographic projection is supported as well.
    const ProjectedPoint ortho =
        world_to_screen(cam, Vec3(0.0f, 0.0f, 0.0f), 200, 100,
                        ProjectionType::ORTHOGRAPHIC, 0.1f, 100.0f);
    REQUIRE(ortho.in_front);
    REQUIRE(ortho.pixel.x == Approx(100.0f).margin(0.5));

    REQUIRE_THROWS_AS(world_to_screen(cam, Vec3(0.0f), 0, 100,
                                      ProjectionType::PERSPECTIVE, 0.1f, 100.0f),
                      std::invalid_argument);
}
