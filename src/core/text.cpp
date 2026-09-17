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

/// @brief Alpha-blend one glyph bitmap into the image.
///
/// `dst_x`/`dst_y` are the position of the bitmap's top left corner, in image
/// pixels (y growing downwards).  Uses the standard "source over" formula with
/// straight (non-premultiplied) alpha, so labels composite correctly onto
/// opaque and transparent backgrounds alike.
void blend_coverage(Image &image, const GlyphBitmap &glyph, int dst_x, int dst_y,
                    const Color &color) {
    if (glyph.empty() || color.a <= 0.0f) {
        return;
    }
    const float src_alpha = std::clamp(color.a, 0.0f, 1.0f);

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
            const float sa = src_alpha * (static_cast<float>(coverage) / 255.0f);
            if (sa <= 0.0f) {
                continue;
            }

            uint8_t dr, dg, db, da;
            image.get_pixel(px, py, dr, dg, db, da);
            const float dst_alpha = static_cast<float>(da) / 255.0f;
            const float out_alpha = sa + dst_alpha * (1.0f - sa);
            if (out_alpha <= 0.0f) {
                continue;
            }
            const float inv_alpha = 1.0f / out_alpha;
            const float keep = dst_alpha * (1.0f - sa);
            const float r = (color.r * sa + (static_cast<float>(dr) / 255.0f) * keep) * inv_alpha;
            const float g = (color.g * sa + (static_cast<float>(dg) / 255.0f) * keep) * inv_alpha;
            const float b = (color.b * sa + (static_cast<float>(db) / 255.0f) * keep) * inv_alpha;
            image.set_pixel(px, py, to_byte(r), to_byte(g), to_byte(b), to_byte(out_alpha));
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
/// so that a halo can never cover a neighbouring glyph.
void draw_line(Image &image, const std::string &line, const Font &font, float x,
               float baseline_y, const TextDrawStyle &style) {
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
                    blend_coverage(image, *pg.bitmap, pg.x + dx, pg.y + dy,
                                   style.halo_color);
                }
            }
        }
    }

    for (const PlacedGlyph &pg : placed) {
        blend_coverage(image, *pg.bitmap, pg.x, pg.y, style.color);
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

        for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
            draw_line(output, lines[line_index], font, left,
                      first_baseline + static_cast<float>(line_index) * line_step,
                      style);
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
        draw_line(image, lines[i], font, x,
                  baseline_y + static_cast<float>(i) * line_step, style);
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
