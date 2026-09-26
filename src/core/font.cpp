/// @file font.cpp
/// @brief Implementation of the TrueType font API (see font.h).
///
/// Glyph rasterization is done by stb_truetype; the implementation is compiled
/// here (and only here) so that the stb symbols stay out of the public headers
/// and out of the library's exported symbols (STBTT_STATIC).

#include <scimesh/font.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>

// stb_truetype is used with its "static" mode so that no stbtt_* symbol leaks
// out of this translation unit, mirroring how image.cpp handles stb_image.
// Asserts are turned into no-ops: dynamic libraries must not abort the host
// process on malformed input (the same reasoning as STBI_ASSERT in image.cpp).
#ifdef SCIMESH_STB_TRUETYPE_IMPL
#ifndef STBTT_STATIC
#define STBTT_STATIC
#endif
#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#endif
#endif
#define STBTT_assert(x) ((void)0)
#include "stb_truetype.h"

namespace scimesh {

namespace {

/// @brief Whether a file exists and can be opened for reading.
bool file_readable(const std::string &path) {
    if (path.empty()) {
        return false;
    }
    std::ifstream in(path.c_str(), std::ios::binary);
    return in.good();
}

/// @brief Read a whole file into memory.
/// @throws std::runtime_error If the file cannot be read or is empty.
std::vector<uint8_t> read_file_bytes(const std::string &path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in.good()) {
        throw std::runtime_error("scimesh: cannot open font file '" + path + "'");
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0) {
        throw std::runtime_error("scimesh: font file '" + path + "' is empty");
    }
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char *>(data.data()), size);
    if (!in) {
        throw std::runtime_error("scimesh: failed to read font file '" + path + "'");
    }
    return data;
}

/// @brief Convert a big-endian UTF-16 string (as stored in TrueType `name`
///        tables) to UTF-8.
///
/// Unpaired surrogates are replaced with U+FFFD.  Only used for reporting the
/// font family name.
std::string utf16be_to_utf8(const unsigned char *bytes, int length) {
    std::string out;
    for (int i = 0; i + 1 < length; i += 2) {
        uint32_t code_unit =
            static_cast<uint32_t>(bytes[i] << 8) | static_cast<uint32_t>(bytes[i + 1]);
        uint32_t cp = code_unit;
        if (code_unit >= 0xD800 && code_unit <= 0xDBFF && i + 3 < length) {
            uint32_t low = static_cast<uint32_t>(bytes[i + 2] << 8) |
                           static_cast<uint32_t>(bytes[i + 3]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((code_unit - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            } else {
                cp = 0xFFFD;
            }
        } else if (code_unit >= 0xDC00 && code_unit <= 0xDFFF) {
            cp = 0xFFFD;
        }

        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

/// @brief Process-wide font cache, keyed by "path<US>pixel_size".
std::mutex g_font_cache_mutex;
std::map<std::string, Font> g_font_cache;

} // namespace

// ---------------------------------------------------------------------------
//  Font::Impl
// ---------------------------------------------------------------------------

/// @brief Shared font state: the font bytes, the parsed font and the glyph cache.
///
/// Held by std::shared_ptr so that copying a Font is cheap and all copies share
/// the rasterized glyphs.  The raw bytes are kept alive because stbtt_fontinfo
/// stores pointers into them.
struct Font::Impl {
    Impl(std::shared_ptr<const std::vector<uint8_t>> bytes, float size, std::string path)
        : data(std::move(bytes)), source_path(std::move(path)), pixel_size(size) {
        if (!data || data->empty()) {
            throw std::runtime_error("scimesh: empty font data");
        }
        const int offset = stbtt_GetFontOffsetForIndex(data->data(), 0);
        if (offset < 0) {
            throw std::runtime_error(
                "scimesh: not a usable TrueType/OpenType font" +
                (source_path.empty() ? std::string() : " '" + source_path + "'"));
        }
        if (!stbtt_InitFont(&info, data->data(), offset)) {
            throw std::runtime_error(
                "scimesh: failed to parse font" +
                (source_path.empty() ? std::string() : " '" + source_path + "'"));
        }
        scale = stbtt_ScaleForPixelHeight(&info, pixel_size);
        int ascent = 0, descent = 0, line_gap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
        metrics.ascent = static_cast<float>(ascent) * scale;
        metrics.descent = std::fabs(static_cast<float>(descent) * scale);
        metrics.line_gap = static_cast<float>(line_gap) * scale;
    }

    std::shared_ptr<const std::vector<uint8_t>> data;  ///< Raw font file bytes.
    std::string source_path;                           ///< Origin of the font, if any.
    stbtt_fontinfo info{};                             ///< Parsed font tables.
    float pixel_size = 0.0f;                           ///< Em size in pixels.
    float scale = 0.0f;                                ///< Font units to pixel scale.
    FontMetrics metrics;                               ///< Vertical metrics in pixels.

    /// Guards `cache`; makes a Font safe to share between threads.
    mutable std::mutex mutex;

    /// Rasterized glyphs, filled on demand.  A std::map is used because
    /// inserting into it never invalidates pointers to existing elements
    /// (glyph() hands those out).
    mutable std::map<uint32_t, GlyphBitmap> cache;
};

// ---------------------------------------------------------------------------
//  Loading
// ---------------------------------------------------------------------------

Font Font::from_file(const std::string &path, float pixel_size) {
    if (!(pixel_size > 0.0f)) {
        throw std::invalid_argument("scimesh: font pixel size must be > 0");
    }
    auto bytes = std::make_shared<const std::vector<uint8_t>>(read_file_bytes(path));
    return Font(std::make_shared<Impl>(std::move(bytes), pixel_size, path));
}

Font Font::from_memory(std::vector<uint8_t> data, float pixel_size) {
    if (!(pixel_size > 0.0f)) {
        throw std::invalid_argument("scimesh: font pixel size must be > 0");
    }
    auto bytes = std::make_shared<const std::vector<uint8_t>>(std::move(data));
    return Font(std::make_shared<Impl>(std::move(bytes), pixel_size, std::string()));
}

Font Font::with_pixel_size(float new_pixel_size) const {
    if (!impl_) {
        return Font();
    }
    if (!(new_pixel_size > 0.0f)) {
        throw std::invalid_argument("scimesh: font pixel size must be > 0");
    }
    // Shares the font bytes, so no file access and no copy of the font data.
    return Font(std::make_shared<Impl>(impl_->data, new_pixel_size, impl_->source_path));
}

// ---------------------------------------------------------------------------
//  Metrics and measurement
// ---------------------------------------------------------------------------

float Font::pixel_size() const { return impl_ ? impl_->pixel_size : 0.0f; }

std::string Font::source_path() const { return impl_ ? impl_->source_path : std::string(); }

FontMetrics Font::metrics() const { return impl_ ? impl_->metrics : FontMetrics(); }

float Font::advance(uint32_t codepoint) const {
    if (!impl_) {
        return 0.0f;
    }
    int advance_width = 0, left_side_bearing = 0;
    stbtt_GetCodepointHMetrics(&impl_->info, static_cast<int>(codepoint),
                               &advance_width, &left_side_bearing);
    return static_cast<float>(advance_width) * impl_->scale;
}

float Font::kerning(uint32_t first, uint32_t second) const {
    if (!impl_) {
        return 0.0f;
    }
    const int kern = stbtt_GetCodepointKernAdvance(&impl_->info,
                                                   static_cast<int>(first),
                                                   static_cast<int>(second));
    return static_cast<float>(kern) * impl_->scale;
}

float Font::measure(const std::string &text) const {
    if (!impl_) {
        return 0.0f;
    }
    const std::vector<uint32_t> codepoints = decode_utf8(text);
    float width = 0.0f;
    uint32_t previous = 0;
    bool have_previous = false;
    for (uint32_t cp : codepoints) {
        if (have_previous) {
            width += kerning(previous, cp);
        }
        width += advance(cp);
        previous = cp;
        have_previous = true;
    }
    return width;
}

const GlyphBitmap *Font::glyph(uint32_t codepoint) const {
    if (!impl_) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->cache.find(codepoint);
    if (it != impl_->cache.end()) {
        return &it->second;
    }

    GlyphBitmap glyph;
    const int glyph_index = stbtt_FindGlyphIndex(&impl_->info, static_cast<int>(codepoint));
    int advance_width = 0, left_side_bearing = 0;
    stbtt_GetGlyphHMetrics(&impl_->info, glyph_index, &advance_width, &left_side_bearing);
    glyph.advance = static_cast<float>(advance_width) * impl_->scale;

    if (glyph_index != 0) {
        int width = 0, height = 0, x_offset = 0, y_offset = 0;
        unsigned char *bitmap = stbtt_GetGlyphBitmap(&impl_->info, impl_->scale, impl_->scale,
                                                     glyph_index, &width, &height,
                                                     &x_offset, &y_offset);
        if (bitmap && width > 0 && height > 0) {
            glyph.width = width;
            glyph.height = height;
            glyph.x_offset = x_offset;
            glyph.y_offset = y_offset;
            const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
            glyph.coverage.assign(bitmap, bitmap + count);
        }
        stbtt_FreeBitmap(bitmap, impl_->info.userdata);
    }

    // std::map never invalidates pointers to existing elements on insert.
    auto inserted = impl_->cache.emplace(codepoint, std::move(glyph));
    return &inserted.first->second;
}

std::string Font::family_name() const {
    if (!impl_) {
        return std::string();
    }
    // nameID 1 is the font family, 4 the full name; platform 3 = Microsoft,
    // encoding 1 = Unicode BMP, language 0x0409 = US English.
    const int name_ids[] = {1, 4};
    for (int name_id : name_ids) {
        int length = 0;
        const char *raw = stbtt_GetFontNameString(&impl_->info, &length,
                                                 STBTT_PLATFORM_ID_MICROSOFT,
                                                 STBTT_MS_EID_UNICODE_BMP,
                                                 STBTT_MS_LANG_ENGLISH, name_id);
        if (raw && length > 0) {
            std::string name = utf16be_to_utf8(
                reinterpret_cast<const unsigned char *>(raw), length);
            if (!name.empty()) {
                return name;
            }
        }
    }
    return std::string();
}

// ---------------------------------------------------------------------------
//  Text decoding helpers
// ---------------------------------------------------------------------------

std::vector<uint32_t> Font::decode_utf8(const std::string &text) {
    std::vector<uint32_t> out;
    out.reserve(text.size());
    const size_t n = text.size();
    size_t i = 0;
    while (i < n) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        uint32_t cp = 0;
        size_t extra = 0;
        if (byte < 0x80) {
            cp = byte;
        } else if ((byte & 0xE0) == 0xC0) {
            cp = byte & 0x1Fu;
            extra = 1;
        } else if ((byte & 0xF0) == 0xE0) {
            cp = byte & 0x0Fu;
            extra = 2;
        } else if ((byte & 0xF8) == 0xF0) {
            cp = byte & 0x07u;
            extra = 3;
        } else {
            out.push_back(0xFFFD);  // stray continuation or invalid lead byte
            ++i;
            continue;
        }

        bool valid = (i + extra < n);
        for (size_t k = 1; valid && k <= extra; ++k) {
            const unsigned char cont = static_cast<unsigned char>(text[i + k]);
            if ((cont & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (cont & 0x3Fu);
        }
        if (!valid) {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

size_t Font::line_count(const std::string &text) {
    size_t lines = 1;
    for (char c : text) {
        if (c == '\n') {
            ++lines;
        }
    }
    return lines;
}

// ---------------------------------------------------------------------------
//  Font resolution and caching
// ---------------------------------------------------------------------------

std::string default_font_path() {
    const char *env = std::getenv("SCIMESH_FONT");
    if (env && *env && file_readable(env)) {
        return std::string(env);
    }
#ifdef SCIMESH_DEFAULT_FONT
    if (file_readable(SCIMESH_DEFAULT_FONT)) {
        return std::string(SCIMESH_DEFAULT_FONT);
    }
#endif
    // Paths relative to the current working directory, which covers running
    // from the source tree or from a build directory below it.
    static const char *candidates[] = {
        "inst/extdata/Inter-Regular.ttf",
        "../inst/extdata/Inter-Regular.ttf",
        "../../inst/extdata/Inter-Regular.ttf",
        "../../../inst/extdata/Inter-Regular.ttf",
        "../../../../inst/extdata/Inter-Regular.ttf",
    };
    for (const char *candidate : candidates) {
        if (file_readable(candidate)) {
            return std::string(candidate);
        }
    }
    return std::string();
}

std::string resolve_font_path(const std::string &path) {
    if (!path.empty()) {
        return path;
    }
    return default_font_path();
}

Font cached_font(const std::string &path, float pixel_size) {
    const std::string resolved = resolve_font_path(path);
    if (resolved.empty()) {
        throw std::runtime_error(
            "scimesh: no font available. Pass a font file, use the bundled "
            "'inst/extdata/Inter-Regular.ttf', or set the SCIMESH_FONT "
            "environment variable to a readable .ttf file.");
    }
    if (!(pixel_size > 0.0f)) {
        throw std::invalid_argument("scimesh: font pixel size must be > 0");
    }

    const std::string key = resolved + '\x1f' + std::to_string(pixel_size);
    std::lock_guard<std::mutex> lock(g_font_cache_mutex);
    auto it = g_font_cache.find(key);
    if (it != g_font_cache.end()) {
        return it->second;
    }
    Font font = Font::from_file(resolved, pixel_size);
    g_font_cache.emplace(key, font);
    return font;
}

void clear_font_cache() {
    std::lock_guard<std::mutex> lock(g_font_cache_mutex);
    g_font_cache.clear();
}

} // namespace scimesh
