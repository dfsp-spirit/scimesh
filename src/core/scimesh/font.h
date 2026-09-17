/// @file font.h
/// @brief TrueType font loading, glyph rasterization and text measurement.
///
/// scimesh draws text labels by rasterizing glyphs with
/// [stb_truetype](https://github.com/nothings/stb) (a single-header,
/// public-domain TrueType parser) and blending the resulting 8-bit coverage
/// bitmaps into the output image.  No system font, font server or graphics
/// stack is involved, so text rendering works headless exactly like the rest of
/// the renderer.
///
/// The package bundles **Inter-Regular.ttf** (SIL Open Font License 1.1), see
/// default_font_path(), so labels work out of the box.  Any other `.ttf` file
/// can be used instead — pass its path to TextLayer::font_file, or set the
/// `SCIMESH_FONT` environment variable to override the default.
///
/// This header is the low-level font API: loading, metrics, glyph coverage
/// bitmaps and text measurement.  Text *placement* (world vs. screen space,
/// anchoring, depth testing, halos) lives in TextLayer, see text.h.
///
/// @par Example
/// @code{.cpp}
/// scimesh::Font font = scimesh::cached_font("", 18.0f);  // "" = bundled font
/// float w = font.measure("anterior");
/// const scimesh::GlyphBitmap *glyph = font.glyph('A');
/// @endcode
///
/// @note Font sizes are in **output pixels** of the final image.  The renderer
///       scales them internally by the supersampling factor, so a label keeps
///       its physical size when anti-aliasing is enabled.
///
/// @warning stb_truetype performs no range checking on the font data.  Only
///          load font files you trust, see the warning in the stb_truetype
///          header.
///
/// @see TextLayer, draw_text(), text.h
///
/// @since 0.4.0

#pragma once

#include <scimesh/types.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scimesh {

/// @brief Vertical metrics of a font at a given pixel size.
///
/// All values are distances in output pixels.  They describe a single line of
/// text: the baseline sits at some pixel row, `ascent` pixels above it is the
/// top of the tallest glyph, `descent` pixels below it is the lowest point of
/// the line (positive value), and `line_gap` is the recommended additional
/// spacing between two lines.
///
/// @see Font::metrics()
struct FontMetrics {
    /// @brief Distance from the baseline to the top of the line, in pixels.
    float ascent = 0.0f;

    /// @brief Distance from the baseline to the bottom of the line, in pixels.
    ///
    /// Stored as a positive value (the raw TrueType descent is negative).
    float descent = 0.0f;

    /// @brief Recommended extra spacing between two lines, in pixels.
    float line_gap = 0.0f;

    /// @brief Total height of one line, in pixels (`ascent + descent + line_gap`).
    float line_height() const { return ascent + descent + line_gap; }

    /// @brief Height of the glyph box of one line, in pixels (`ascent + descent`).
    float box_height() const { return ascent + descent; }
};

/// @brief One rasterized glyph: an 8-bit coverage bitmap plus placement info.
///
/// The bitmap is a single-channel (coverage) image: 0 means fully transparent,
/// 255 means the pixel is fully covered by the glyph.  Coverage values are
/// fractional, which is what gives text its anti-aliasing.
///
/// Placement follows the usual TrueType convention: the glyph is drawn with
/// its bitmap *top left* corner at
/// `(pen_x + x_offset, baseline_y + y_offset)`, where the y axis points down.
/// Because the y offsets are negative for glyphs that sit above the baseline,
/// the renderer draws at `baseline_y + y_offset`.
///
/// @see Font::glyph()
struct GlyphBitmap {
    /// @brief Bitmap width in pixels.
    int width = 0;

    /// @brief Bitmap height in pixels.
    int height = 0;

    /// @brief Horizontal offset from the pen position to the bitmap's left edge.
    int x_offset = 0;

    /// @brief Vertical offset from the baseline to the bitmap's top edge.
    ///
    /// Negative for glyphs above the baseline (the common case), since screen
    /// y coordinates grow downwards.
    int y_offset = 0;

    /// @brief Pen advance for this glyph, in pixels.
    float advance = 0.0f;

    /// @brief Coverage values, row-major, `width * height` entries.
    std::vector<uint8_t> coverage;

    /// @brief Whether this glyph has no pixels to draw (e.g. a space).
    bool empty() const { return width <= 0 || height <= 0 || coverage.empty(); }

    /// @brief Coverage value at pixel (x, y), 0-255.  No bounds checking.
    uint8_t at(int x, int y) const {
        return coverage[static_cast<size_t>(y) * static_cast<size_t>(width) +
                        static_cast<size_t>(x)];
    }
};

/// @brief A TrueType font, loaded at a fixed pixel size, with a glyph cache.
///
/// A `Font` is a cheap handle: copying it shares the underlying font data and
/// the rasterized glyph cache (reference counting), so passing fonts around by
/// value is fine.  Glyphs are rasterized lazily on first use and then cached,
/// which makes repeated renders of the same labels cheap.
///
/// Fonts are immutable once loaded; to render at a different size use
/// with_pixel_size(), which re-uses the already loaded font bytes.
///
/// @par Example
/// @code{.cpp}
/// scimesh::Font font = scimesh::cached_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14.0f);
/// scimesh::FontMetrics m = font.metrics();
/// float width = font.measure("posterior");
/// @endcode
///
/// @see cached_font(), draw_text(), TextLayer
///
/// @since 0.4.0
class Font {
public:
    /// @brief Construct an empty (invalid) font.  See valid().
    Font() = default;

    /// @brief Load a TrueType font from a file.
    ///
    /// @param path       Path to a `.ttf` (or `.otf` with TrueType outlines) file.
    /// @param pixel_size Em size in output pixels, must be > 0.
    /// @return The loaded font.
    /// @throws std::runtime_error If the file cannot be read or is not a
    ///         supported font.
    static Font from_file(const std::string &path, float pixel_size);

    /// @brief Create a font from TTF data already in memory.
    ///
    /// @param data       Raw contents of a `.ttf` file (moved into the font).
    /// @param pixel_size Em size in output pixels, must be > 0.
    /// @return The loaded font.
    /// @throws std::runtime_error If the data is not a supported font.
    static Font from_memory(std::vector<uint8_t> data, float pixel_size);

    /// @brief Whether this handle refers to a loaded font.
    bool valid() const { return static_cast<bool>(impl_); }

    /// @brief Em size of this font in pixels (0 for an invalid font).
    float pixel_size() const;

    /// @brief Path this font was loaded from, or an empty string for
    ///        from_memory() fonts.
    std::string source_path() const;

    /// @brief Vertical metrics (ascent, descent, line gap) in pixels.
    ///
    /// Returns all zeros for an invalid font.
    FontMetrics metrics() const;

    /// @brief Width of a single line of text in pixels, including kerning.
    ///
    /// Newline characters are *not* handled specially here: callers that
    /// support multi-line text split the string first (see TextLayer).
    ///
    /// @param text A UTF-8 encoded string.
    float measure(const std::string &text) const;

    /// @brief Pen advance of one code point in pixels.
    float advance(uint32_t codepoint) const;

    /// @brief Kerning adjustment between two code points, in pixels.
    ///
    /// Only the legacy TrueType `kern` table is consulted (that is all
    /// stb_truetype supports).  Fonts that carry their kerning in the modern
    /// `GPOS` table — many recent fonts, including the bundled Inter — return
    /// 0.0 here, which simply means "no kerning information available".
    float kerning(uint32_t first, uint32_t second) const;

    /// @brief The rasterized glyph for a code point, rasterizing it on first use.
    ///
    /// The returned pointer stays valid as long as *any* copy of this font
    /// lives (the glyphs are cached in the shared font data).  Returns an empty
    /// glyph with a space-like advance when the font has no glyph for the code
    /// point, and nullptr only for an invalid font.
    ///
    /// @param codepoint Unicode code point (not a UTF-8 byte).
    const GlyphBitmap *glyph(uint32_t codepoint) const;

    /// @brief Family name of the font, e.g. `"Inter"` (empty if unavailable).
    std::string family_name() const;

    /// @brief A font handle for the same typeface at a different pixel size.
    ///
    /// The already loaded font bytes are shared, so this does not touch the
    /// file system again.  Used by the renderer to rasterize at the
    /// supersampled resolution, see RenderOptions::aa_samples.
    ///
    /// @param new_pixel_size New em size in pixels, must be > 0.
    Font with_pixel_size(float new_pixel_size) const;

    /// @brief Decode a UTF-8 string into Unicode code points.
    ///
    /// Invalid byte sequences are decoded as U+FFFD (replacement character)
    /// instead of throwing, so arbitrary user input is safe to pass in.
    ///
    /// @param text A UTF-8 encoded string.
    static std::vector<uint32_t> decode_utf8(const std::string &text);

    /// @brief Number of lines in a string (newline-separated, at least 1).
    static size_t line_count(const std::string &text);

private:
    struct Impl;

    explicit Font(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

    std::shared_ptr<Impl> impl_;
};

/// @brief Turn a possibly empty font path into a path that exists.
///
/// An empty `path` means "use the bundled default font", see
/// default_font_path().  Non-empty paths are returned unchanged, since the
/// point of an explicit path is that the user chose it.
///
/// @param path Path to a `.ttf` file, or an empty string.
/// @return A path to an existing font file, or an empty string if `path` was
///         empty and no default font could be found.
///
/// @see default_font_path()
std::string resolve_font_path(const std::string &path);

/// @brief Path of the bundled default font, or as overridden by the
///        `SCIMESH_FONT` environment variable.
///
/// Resolution order:
/// 1. the `SCIMESH_FONT` environment variable, when set and readable,
/// 2. the default font path compiled into the library: for builds from the
///    scimesh source tree that is the bundled
///    `<source>/inst/extdata/Inter-Regular.ttf`, and installed copies also ship
///    it as `<prefix>/share/scimesh/fonts/Inter-Regular.ttf` (pass that path
///    explicitly, or via `SCIMESH_FONT`, when the source tree is gone),
/// 3. a few paths relative to the current working directory, which is what
///    makes the bundled font work when running from the source tree or from an
///    example build directory.
///
/// @return A readable path to a `.ttf` file, or an empty string when none of
///         the candidates exist.
std::string default_font_path();

/// @brief Load a font, re-using an already loaded one when possible.
///
/// @param path       Path to a `.ttf` file, or an empty string for the
///                   bundled default font (see default_font_path()).
/// @param pixel_size Em size in output pixels, must be > 0.
/// @return A handle to the (possibly cached) font.
/// @throws std::runtime_error If no font can be found, or it cannot be loaded.
///
/// @see default_font_path(), clear_font_cache()
Font cached_font(const std::string &path, float pixel_size);

/// @brief Drop all fonts from the process-wide font cache.
///
/// Fonts in use stay valid (they are reference counted); only the cache is
/// emptied.  Mostly useful for tests and for releasing memory in long-running
/// sessions.
void clear_font_cache();

} // namespace scimesh
