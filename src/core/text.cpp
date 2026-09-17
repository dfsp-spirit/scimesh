/// @file text.cpp
/// @brief Implementation of the text layer (see text.h).
///
/// Rendering works in "screen" coordinates with the origin at the top left and
/// y growing downwards, which is both what ndc_to_screen() returns and the row
/// order of Image (row 0 is the top row of the written file).  Glyph bitmaps
/// from stb_truetype are top-down as well, so no flipping is involved anywhere.

#include <scimesh/text.h>

#include <scimesh/math_utils.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace scimesh {

namespace {

/// Depth tolerance for the depth test, in NDC units.
///
/// Labels are usually anchored *on* a surface (a vertex, an atom center), where
/// the projected depth is exactly the depth of that surface.  Without a
/// tolerance such labels would be hidden by their own anchor surface, so points
/// that are up to this much behind the closest surface still count as visible.
constexpr float kDepthTolerance = 1.0e-3f;

/// @brief Convert a normalized [0, 1] value to a byte.
uint8_t to_byte(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(clamped * 255.0f));
}

/// @brief Split a string into lines at newline characters.
///
/// Handles "\r\n" and drops one trailing empty line, so "a\n" is one line.
std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    lines.push_back(current);
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

/// @brief Alpha-blend one coverage value into a single pixel of the image.
///
/// `coverage` is a value in [0, 1] (glyph coverage times the text alpha).  Uses
/// the standard "source over" formula with straight (non-premultiplied) alpha,
/// so labels composite correctly onto opaque and transparent backgrounds alike.
void blend_pixel(Image &image, int x, int y, float coverage, const Color &color) {
    if (coverage <= 0.0f || color.a <= 0.0f) {
        return;
    }
    if (x < 0 || x >= image.width || y < 0 || y >= image.height) {
        return;
    }
    const float sa = std::clamp(color.a, 0.0f, 1.0f) * coverage;
    if (sa <= 0.0f) {
        return;
    }

    uint8_t dr, dg, db, da;
    image.get_pixel(x, y, dr, dg, db, da);
    const float dst_alpha = static_cast<float>(da) / 255.0f;
    const float out_alpha = sa + dst_alpha * (1.0f - sa);
    if (out_alpha <= 0.0f) {
        return;
    }
    const float inv_alpha = 1.0f / out_alpha;
    const float keep = dst_alpha * (1.0f - sa);
    const float r = (color.r * sa + (static_cast<float>(dr) / 255.0f) * keep) * inv_alpha;
    const float g = (color.g * sa + (static_cast<float>(dg) / 255.0f) * keep) * inv_alpha;
    const float b = (color.b * sa + (static_cast<float>(db) / 255.0f) * keep) * inv_alpha;
    image.set_pixel(x, y, to_byte(r), to_byte(g), to_byte(b), to_byte(out_alpha));
}

/// @brief Alpha-blend one glyph bitmap into the image, unrotated.
///
/// `dst_x`/`dst_y` are the position of the bitmap's top left corner, in image
/// pixels (y growing downwards).  This is the exact, integer-aligned path used
/// whenever no rotation is involved.
void blend_coverage(Image &image, const GlyphBitmap &glyph, int dst_x, int dst_y,
                    const Color &color) {
    if (glyph.empty() || color.a <= 0.0f) {
        return;
    }
    for (int y = 0; y < glyph.height; ++y) {
        const int py = dst_y + y;
        if (py < 0 || py >= image.height) {
            continue;
        }
        for (int x = 0; x < glyph.width; ++x) {
            const int px = dst_x + x;
            if (px < 0 || px >= image.width) {
                continue;
            }
            const uint8_t coverage = glyph.at(x, y);
            if (coverage == 0) {
                continue;
            }
            blend_pixel(image, px, py, static_cast<float>(coverage) / 255.0f, color);
        }
    }
}

/// @brief Bilinear coverage lookup in a glyph bitmap.
///
/// `u`/`v` are positions in bitmap pixel space, where integer values sit on
/// pixel corners (0.5 is the centre of the first pixel).  Samples outside the
/// bitmap are clamped to its edge, which keeps rotated glyph edges from
/// darkening.
float sample_coverage(const GlyphBitmap &glyph, float u, float v) {
    const float max_u = static_cast<float>(glyph.width - 1);
    const float max_v = static_cast<float>(glyph.height - 1);
    const float fu = std::clamp(u - 0.5f, 0.0f, max_u);
    const float fv = std::clamp(v - 0.5f, 0.0f, max_v);
    const int x0 = static_cast<int>(std::floor(fu));
    const int y0 = static_cast<int>(std::floor(fv));
    const int x1 = std::min(x0 + 1, glyph.width - 1);
    const int y1 = std::min(y0 + 1, glyph.height - 1);
    const float sx = fu - static_cast<float>(x0);
    const float sy = fv - static_cast<float>(y0);
    const float c00 = glyph.at(x0, y0);
    const float c10 = glyph.at(x1, y0);
    const float c01 = glyph.at(x0, y1);
    const float c11 = glyph.at(x1, y1);
    const float top = c00 * (1.0f - sx) + c10 * sx;
    const float bottom = c01 * (1.0f - sx) + c11 * sx;
    return (top * (1.0f - sy) + bottom * sy) / 255.0f;
}

/// @brief Alpha-blend one glyph bitmap into the image, rotated about a pivot.
///
/// The glyph keeps its place in the *unrotated* text layout (its top left
/// corner in text space is at `origin_x`/`origin_y`), and the whole text is
/// rotated about (`pivot_x`, `pivot_y`) by `rotation_degrees`
/// (counter-clockwise on screen).  Rendering works by inverse mapping: for
/// every destination pixel the corresponding text-space position is computed,
/// and the glyph coverage is sampled there.
void blend_glyph_rotated(Image &image, const GlyphBitmap &glyph, float origin_x,
                         float origin_y, float rotation_degrees, float pivot_x,
                         float pivot_y, const Color &color) {
    if (glyph.empty() || color.a <= 0.0f) {
        return;
    }
    const float radians = rotation_degrees * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    // Destination bounding box: rotate the four corners of the glyph rect.
    // Forward transform (text space -> image space) is a rotation by
    // -rotation_degrees, so that positive angles turn counter-clockwise on
    // screen (where y grows downwards).
    const float corners_x[4] = {origin_x, origin_x + static_cast<float>(glyph.width),
                                origin_x, origin_x + static_cast<float>(glyph.width)};
    const float corners_y[4] = {origin_y, origin_y,
                                origin_y + static_cast<float>(glyph.height),
                                origin_y + static_cast<float>(glyph.height)};
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    for (int i = 0; i < 4; ++i) {
        const float dx = corners_x[i] - pivot_x;
        const float dy = corners_y[i] - pivot_y;
        const float rx = pivot_x + dx * c + dy * s;
        const float ry = pivot_y - dx * s + dy * c;
        min_x = std::min(min_x, rx);
        min_y = std::min(min_y, ry);
        max_x = std::max(max_x, rx);
        max_y = std::max(max_y, ry);
    }

    const int x_start = std::max(0, static_cast<int>(std::floor(min_x)) - 1);
    const int x_end = std::min(image.width - 1, static_cast<int>(std::ceil(max_x)) + 1);
    const int y_start = std::max(0, static_cast<int>(std::floor(min_y)) - 1);
    const int y_end = std::min(image.height - 1, static_cast<int>(std::ceil(max_y)) + 1);

    for (int py = y_start; py <= y_end; ++py) {
        for (int px = x_start; px <= x_end; ++px) {
            // Inverse transform (image space -> text space): rotate the pixel
            // centre back by +rotation_degrees.
            const float dx = static_cast<float>(px) + 0.5f - pivot_x;
            const float dy = static_cast<float>(py) + 0.5f - pivot_y;
            const float tx = pivot_x + dx * c - dy * s;
            const float ty = pivot_y + dx * s + dy * c;

            const float u = tx - origin_x;
            const float v = ty - origin_y;
            if (u < 0.0f || v < 0.0f || u >= static_cast<float>(glyph.width) ||
                v >= static_cast<float>(glyph.height)) {
                continue;
            }
            const float coverage = sample_coverage(glyph, u, v);
            if (coverage <= 0.002f) {
                continue;
            }
            blend_pixel(image, px, py, coverage, color);
        }
    }
}

/// @brief A glyph bitmap placed at its final pixel position.
struct PlacedGlyph {
    const GlyphBitmap *bitmap = nullptr;
    int x = 0;  ///< Left edge of the bitmap, in image pixels.
    int y = 0;  ///< Top edge of the bitmap, in image pixels.
};

/// @brief Draw one line of text with the left edge at `x` and the given baseline.
///
/// Halos are drawn for all glyphs of the line first and the glyphs afterwards,
/// so that a halo can never cover a neighbouring glyph.  When
/// `style.rotation` is nonzero, everything is rotated about (`pivot_x`,
/// `pivot_y`) in the image plane; a rotation of exactly zero uses the exact
/// integer-aligned path (no resampling), so unrotated text is unaffected.
void draw_line(Image &image, const std::string &line, const Font &font, float x,
               float baseline_y, const TextDrawStyle &style, float pivot_x,
               float pivot_y) {
    const std::vector<uint32_t> codepoints = Font::decode_utf8(line);

    std::vector<PlacedGlyph> placed;
    placed.reserve(codepoints.size());
    float pen = x;
    uint32_t previous = 0;
    bool have_previous = false;
    for (uint32_t codepoint : codepoints) {
        if (have_previous) {
            pen += font.kerning(previous, codepoint);
        }
        const GlyphBitmap *glyph = font.glyph(codepoint);
        if (glyph != nullptr) {
            if (!glyph->empty()) {
                PlacedGlyph pg;
                pg.bitmap = glyph;
                pg.x = static_cast<int>(std::lround(pen)) + glyph->x_offset;
                pg.y = static_cast<int>(std::lround(baseline_y)) + glyph->y_offset;
                placed.push_back(pg);
            }
            pen += glyph->advance;
        }
        previous = codepoint;
        have_previous = true;
    }

    const bool rotated = std::abs(style.rotation) > 1.0e-4f;
    const auto stamp = [&](const GlyphBitmap &bitmap, float glyph_x, float glyph_y,
                           const Color &color) {
        if (rotated) {
            blend_glyph_rotated(image, bitmap, glyph_x, glyph_y, style.rotation,
                                pivot_x, pivot_y, color);
        } else {
            blend_coverage(image, bitmap, static_cast<int>(std::lround(glyph_x)),
                           static_cast<int>(std::lround(glyph_y)), color);
        }
    };

    if (style.halo_color.a > 0.0f && style.halo_width > 0.0f) {
        const int width = std::max(1, static_cast<int>(std::lround(style.halo_width)));
        const int limit = width * width + width;  // slightly rounded kernel
        for (const PlacedGlyph &pg : placed) {
            for (int dy = -width; dy <= width; ++dy) {
                for (int dx = -width; dx <= width; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;  // the glyph itself is drawn below
                    }
                    if (dx * dx + dy * dy > limit) {
                        continue;
                    }
                    stamp(*pg.bitmap, static_cast<float>(pg.x + dx),
                          static_cast<float>(pg.y + dy), style.halo_color);
                }
            }
        }
    }

    for (const PlacedGlyph &pg : placed) {
        stamp(*pg.bitmap, static_cast<float>(pg.x), static_cast<float>(pg.y),
              style.color);
    }
}

/// @brief Draw all labels of one text layer.
///
/// @param transform Model matrix applied to world-space positions.
void render_one_layer(const TextLayer &layer, Image &output,
                      const Mat4 &view_projection, const Mat4 &transform,
                      float pixel_scale, const std::vector<float> &z_buffer,
                      const Color &default_color) {
    if (layer.empty() || output.width <= 0 || output.height <= 0) {
        return;
    }
    const float font_size = layer.size * pixel_scale;
    if (!(font_size > 0.0f)) {
        return;
    }

    const Font font = cached_font(layer.font_file, font_size);
    const FontMetrics metrics = font.metrics();
    const float line_step = metrics.box_height() * std::max(layer.line_spacing, 0.0f);

    const bool depth_test = layer.depth_test && (layer.space == TextSpace::WORLD) &&
                            !z_buffer.empty();

    for (size_t i = 0; i < layer.count(); ++i) {
        const std::string &text = layer.strings[i];
        if (text.empty()) {
            continue;
        }
        const Color color = layer.color_or(i, default_color);
        if (color.a <= 0.0f) {
            continue;
        }

        // --- anchor position in output pixels (origin top left, y down) -----
        float anchor_x = 0.0f, anchor_y = 0.0f;
        float anchor_depth = 0.0f;
        if (layer.space == TextSpace::WORLD) {
            const Vec3 world = Vec3(transform * Vec4(layer.position_or(i), 1.0f));
            const Vec4 clip = transform_point_homogeneous(view_projection, world);
            if (clip.w <= 1.0e-6f) {
                continue;  // behind the camera
            }
            float sx = 0.0f, sy = 0.0f, depth = 0.0f;
            ndc_to_screen(perspective_divide(clip), output.width, output.height, sx, sy, depth);
            anchor_x = sx;
            anchor_y = sy;
            anchor_depth = depth;
        } else {
            const Vec3 position = layer.position_or(i);
            anchor_x = position.x * pixel_scale;
            anchor_y = position.y * pixel_scale;
        }
        anchor_x += layer.offset.x * pixel_scale;
        anchor_y += layer.offset.y * pixel_scale;

        // A degenerate camera (or a point exactly on the camera plane) can
        // produce non-finite coordinates; such labels are simply skipped.
        if (!std::isfinite(anchor_x) || !std::isfinite(anchor_y)) {
            continue;
        }

        // --- hide labels that are behind the rendered geometry --------------
        if (depth_test) {
            const int px = static_cast<int>(std::floor(anchor_x));
            const int py = static_cast<int>(std::floor(anchor_y));
            if (px < 0 || px >= output.width || py < 0 || py >= output.height) {
                continue;  // anchor outside the image
            }
            const float surface_depth =
                z_buffer[static_cast<size_t>(py) * static_cast<size_t>(output.width) +
                         static_cast<size_t>(px)];
            if (anchor_depth > surface_depth + kDepthTolerance) {
                continue;  // geometry in front of the anchor
            }
        }

        // --- lay the text out around the anchor -----------------------------
        const std::vector<std::string> lines = split_lines(text);
        float block_width = 0.0f;
        for (const std::string &line : lines) {
            block_width = std::max(block_width, font.measure(line));
        }
        const float block_height =
            metrics.box_height() + static_cast<float>(lines.size() - 1) * line_step;
        const float left = anchor_x - layer.adj.x * block_width;
        const float first_baseline =
            anchor_y + metrics.ascent - (1.0f - layer.adj.y) * block_height;

        TextDrawStyle style;
        style.color = color;
        style.halo_color = layer.halo_color;
        style.halo_width = layer.halo_width * pixel_scale;
        style.line_spacing = layer.line_spacing;
        style.rotation = layer.rotation;

        for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
            // A rotated label rotates about its anchor, so the anchor stays put
            // while the text swings around it.
            draw_line(output, lines[line_index], font, left,
                      first_baseline + static_cast<float>(line_index) * line_step,
                      style, anchor_x, anchor_y);
        }
    }
}

} // namespace

void draw_text(Image &image, const std::string &text, const Font &font, float x,
               float baseline_y, const TextDrawStyle &style) {
    if (!font.valid()) {
        throw std::invalid_argument("scimesh: draw_text() needs a valid font");
    }
    const float line_step = font.metrics().box_height() * std::max(style.line_spacing, 0.0f);
    const std::vector<std::string> lines = split_lines(text);
    for (size_t i = 0; i < lines.size(); ++i) {
        // A rotated draw_text() rotates about the given pen origin, i.e. about
        // the left end of the first baseline.
        draw_line(image, lines[i], font, x,
                  baseline_y + static_cast<float>(i) * line_step, style, x,
                  baseline_y);
    }
}

TextExtent measure_text(const std::string &text, float size,
                        const std::string &font_file, float line_spacing) {
    if (!(size > 0.0f)) {
        throw std::invalid_argument("scimesh: text size must be > 0");
    }
    const Font font = cached_font(font_file, size);
    const FontMetrics metrics = font.metrics();
    const std::vector<std::string> lines = split_lines(text);

    TextExtent extent;
    extent.line_count = lines.size();
    for (const std::string &line : lines) {
        extent.width = std::max(extent.width, font.measure(line));
    }
    const float line_step = metrics.box_height() * std::max(line_spacing, 0.0f);
    extent.height = metrics.box_height() +
                    static_cast<float>(lines.size() - 1) * line_step;
    // The ascent/descent of the *block*: the first baseline sits one ascent
    // below the top of the block, the last one one descent above its bottom.
    extent.ascent = metrics.ascent;
    extent.descent = extent.height - metrics.ascent;
    return extent;
}

namespace detail {

void render_text_layers(const std::vector<TextNodeRef> &layers, Image &output,
                        const Mat4 &view_projection, float pixel_scale,
                        const std::vector<float> &z_buffer,
                        const Color &default_color) {
    for (const TextNodeRef &node : layers) {
        if (node.layer == nullptr) {
            continue;
        }
        render_one_layer(*node.layer, output, view_projection, node.transform,
                         pixel_scale, z_buffer, default_color);
    }
}

} // namespace detail

} // namespace scimesh
