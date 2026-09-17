/// @file text.h
/// @brief Text labels — the TextLayer.
///
/// Text layers are the third kind of screen-oriented primitive in scimesh,
/// next to line layers (see lines.h) and meshes: they belong to a Scene, are
/// drawn after the meshes and lines, and are ignored by the bounding box and by
/// the mesh exporters, so adding labels never changes the camera framing.
///
/// A text layer is a list of strings with positions.  Positions are either
/// **world space** (projected with the scene camera, so a label sticks to the
/// 3D point it annotates — good for "this sulcus", "this atom") or **screen
/// space** (fixed output pixels — good for figure titles and axis labels).
/// Labels are drawn as *billboards*, i.e. they always face the camera and keep
/// their physical size; they are not 3D geometry, which is what makes them stay
/// readable from any viewpoint.
///
/// The font comes from a `.ttf` file (see font.h); by default the bundled
/// Inter-Regular.ttf is used.  Glyphs are rasterized with anti-aliasing at the
/// supersampled resolution, so labels are crisp and stay the same physical size
/// when RenderOptions::aa_samples is raised.
///
/// @par Example
/// @code{.cpp}
/// TextLayer labels;
/// labels.strings = {"anterior", "posterior"};
/// labels.positions = {Vec3(0, 1, 0), Vec3(0, -1, 0)};
/// labels.size = 18.0f;                       // output pixels
/// labels.color = Color(0, 0, 0, 1);          // (or use `colors` per label)
/// labels.adj = Vec2(0.5f, 0.5f);             // centred on the position
/// scene.add_texts(labels);
/// @endcode
///
/// @see Font, draw_text(), Scene::add_texts()

#pragma once

#include <scimesh/font.h>
#include <scimesh/image.h>
#include <scimesh/types.h>

#include <algorithm>
#include <string>
#include <vector>

namespace scimesh {

/// @brief The coordinate space of a TextLayer's positions.
///
/// @see TextLayer::space
enum class TextSpace {
    /// @brief 3D world coordinates, projected with the scene camera.
    ///
    /// Use this for labels that belong to a location in the scene: the label
    /// follows the annotated point when the camera moves, and it can be hidden
    /// by geometry in front of it (see TextLayer::depth_test).
    WORLD,

    /// @brief Output image pixels, origin at the **top left**, y growing down.
    ///
    /// Use this for labels whose place is defined by the figure layout rather
    /// than by the 3D scene (titles, panel tags, corner annotations).  The
    /// values are in the pixels of the final image, independent of the
    /// anti-aliasing factor.
    SCREEN
};

/// @brief How to draw text onto an image with draw_text().
///
/// @see draw_text()
struct TextDrawStyle {
    /// @brief Text color; alpha values below 1 blend with the background.
    Color color = Color(0.0f, 0.0f, 0.0f, 1.0f);

    /// @brief Color of the halo (outline) drawn behind the glyphs.
    ///
    /// The default has alpha 0, which disables the halo.  A halo makes labels
    /// readable on top of busy or dark geometry.
    Color halo_color = Color(0.0f, 0.0f, 0.0f, 0.0f);

    /// @brief Halo thickness in pixels (default: 1.0).
    float halo_width = 1.0f;

    /// @brief Distance between two lines of a multi-line string, as a multiple
    ///        of the height of the font's glyph box (default: 1.2).
    float line_spacing = 1.2f;

    /// @brief Rotation of the text in degrees, counter-clockwise, about the
    ///        point (`x`, `baseline_y`) passed to draw_text() (default: 0).
    ///
    /// Useful values are 90 (reads bottom to top, the usual orientation of a
    /// y-axis label), -90 or 270 (reads top to bottom) and 180 (upside down).
    /// Arbitrary angles work as well; glyphs are resampled with bilinear
    /// interpolation.
    float rotation = 0.0f;
};

/// @brief Size of a (possibly multi-line) piece of text.
///
/// The text box used by the renderer: `width` is the width of the widest line,
/// `height` the height of the whole block, and the anchor is placed on this box
/// with TextLayer::adj.  Use it for layout decisions such as "which corner is
/// still free for a caption", or to centre a title.
///
/// @see measure_text(), Font::measure()
struct TextExtent {
    /// @brief Width of the widest line, in pixels.
    float width = 0.0f;

    /// @brief Height of the whole text block, in pixels.
    float height = 0.0f;

    /// @brief Distance from the first baseline to the top of the block, in pixels.
    float ascent = 0.0f;

    /// @brief Distance from the last baseline to the bottom of the block, in pixels.
    float descent = 0.0f;

    /// @brief Number of lines (a trailing newline does not add one).
    size_t line_count = 0;
};

/// @brief Measure a (possibly multi-line) string in a given font and size.
///
/// Uses exactly the same layout as the renderer (same line splitting, same
/// line spacing), so the result matches the drawn label.
///
/// @param text         UTF-8 text; `\n` starts a new line.
/// @param size         Text height in output pixels.
/// @param font_file    Path to a `.ttf` file, or an empty string for the
///                     bundled default font.
/// @param line_spacing Distance between lines, as a multiple of the font's
///                     glyph box height (default: 1.2).
/// @return The text extent, see TextExtent.
/// @throws std::invalid_argument If `size` is not positive.
/// @throws std::runtime_error If the font cannot be loaded.
///
/// @par Example
/// @code{.cpp}
/// TextExtent ext = measure_text("anterior", 18.0f);
/// // place the text box in the top right corner of an 800x600 image
/// float x = 800.0f - ext.width - 10.0f;
/// @endcode
///
/// @see TextExtent, Font::measure()
TextExtent measure_text(const std::string &text, float size,
                        const std::string &font_file = "",
                        float line_spacing = 1.2f);

/// @brief Draw text into an image at a given position and baseline.
///
/// This is the low-level drawing primitive: it places the text's *left edge* at
/// `x` and its first *baseline* at `baseline_y`, in image pixels.  y grows
/// downwards, matching the image row order and the screen coordinates produced
/// by ndc_to_screen().  Multi-line strings (separated by `\n`) are drawn with
/// the line spacing from `style`, all lines left-aligned at `x`.
///
/// Glyphs are alpha-blended over whatever is already in the image, so this can
/// be called on an image produced by any of the renderers.  Measurements for
/// layouts come from Font::measure() and Font::metrics().
///
/// @par Example
/// @code{.cpp}
/// Font font = cached_font("", 20.0f);
/// draw_text(img, "anterior", font, 20.0f, 40.0f, TextDrawStyle());
///
/// // A y-axis style label, reading bottom to top:
/// TextDrawStyle rotated;
/// rotated.rotation = 90.0f;
/// draw_text(img, "intensity", font, 20.0f, 40.0f, rotated);
/// @endcode
///
/// @param[in,out] image  The image to draw into.
/// @param text           UTF-8 text; `\n` starts a new line.
/// @param font           A loaded font (see cached_font()).
/// @param x              X position of the left edge of the text, in pixels.
/// @param baseline_y     Y position of the first baseline, in pixels.
/// @param style          Colors, halo, line spacing and rotation.
///
/// @see TextLayer, Font, TextDrawStyle
void draw_text(Image &image, const std::string &text, const Font &font, float x,
               float baseline_y, const TextDrawStyle &style = TextDrawStyle());

/// @brief A batch of text labels drawn together, in one coordinate space.
///
/// A text layer belongs to a Scene (see Scene::add_texts()) and is drawn by the
/// Renderer after the meshes and the line layers, so labels appear on top of
/// the geometry — except when depth_test is on and the annotated point is
/// behind the geometry.
///
/// Text layers are deliberately **not** meshes:
///
/// - `Scene::compute_bounding_box()` ignores them, so adding labels never
///   changes the camera framing.
/// - The mesh exporters (`write_gltf()`, `write_stl()`, ...) skip them.
///
/// @par Example
/// @code{.cpp}
/// TextLayer layer;
/// layer.strings = {"C", "N"};
/// layer.positions = {Vec3(1, 0, 0), Vec3(0, 2, 0)};
/// layer.colors = {Color(0.1f, 0.1f, 0.1f, 1.0f), Color(0, 0, 1, 1)};
/// layer.size = 14.0f;
/// layer.halo_color = Color(1, 1, 1, 1);   // white outline for dark scenes
/// scene.add_texts(layer);
/// @endcode
///
/// @see Font, TextSpace, draw_text(), Scene::add_texts()
struct TextLayer {
    /// @brief Anchor positions, in world space or in screen pixels.
    ///
    /// Interpreted according to `space`.  For TextSpace::SCREEN only x and y
    /// are used (z is ignored).
    std::vector<Vec3> positions;

    /// @brief One string per entry in `positions` (UTF-8; `\n` for line breaks).
    std::vector<std::string> strings;

    /// @brief One color per label; may be empty (then the renderer uses
    ///        RenderOptions::default_color) or shorter than `positions`
    ///        (the last entry is reused for the remaining labels).
    std::vector<Color> colors;

    /// @brief Path to the `.ttf` file to use, or an empty string for the
    ///        bundled default font (see default_font_path()).
    ///
    /// This makes the typeface replaceable: point it at any `.ttf` on the
    /// system, or at the bundled font.
    std::string font_file;

    /// @brief Text height ("em" size) in output pixels (default: 16).
    ///
    /// The renderer scales this internally by the supersampling factor, so the
    /// physical size of a label does not change with RenderOptions::aa_samples.
    float size = 16.0f;

    /// @brief Coordinate space of `positions` (default: TextSpace::WORLD).
    TextSpace space = TextSpace::WORLD;

    /// @brief Where the position sits relative to the text box, in [0, 1].
    ///
    /// `(0, 0)` places the position at the bottom left of the text box, `(1, 1)`
    /// at the top right, and the default `(0.5, 0.5)` centres the text box on
    /// the position — the same idea as `adj` in R graphics and rgl.
    Vec2 adj = Vec2(0.5f, 0.5f);

    /// @brief Extra offset applied after anchoring, in output pixels
    ///        (positive x = right, positive y = down).
    Vec2 offset = Vec2(0.0f, 0.0f);

    /// @brief Distance between two lines of a multi-line string, as a multiple
    ///        of the height of the font's glyph box (default: 1.2).
    float line_spacing = 1.2f;

    /// @brief Rotation of the label in degrees, counter-clockwise, about the
    ///        anchor position (default: 0).
    ///
    /// The anchor is the position after `adj` and `offset` have been applied,
    /// so a rotated label keeps that point fixed.  The rotation happens in the
    /// image plane (in screen space) for world-space labels as well, which is
    /// all a billboard can do.  Use 90 to write along a vertical axis (the
    /// usual orientation of a y-axis label), 180 for an upside-down label, or
    /// any other angle to follow an annotation line; glyphs are resampled with
    /// bilinear interpolation.
    float rotation = 0.0f;

    /// @brief Whether the label is hidden by geometry in front of its anchor
    ///        (default: true).
    ///
    /// When true, a label whose anchor point is behind the rendered surfaces
    /// (e.g. an atom label on the far side of a protein) is skipped, which is
    /// what makes labels look like they belong to the scene.  Set to false to
    /// always draw the labels on top of everything, which is useful for
    /// annotating a point that lies *on* a surface.
    ///
    /// Only meaningful for TextSpace::WORLD.
    bool depth_test = true;

    /// @brief Color of the halo (outline) drawn behind the glyphs.
    ///
    /// Alpha 0 (the default) disables the halo.
    Color halo_color = Color(0.0f, 0.0f, 0.0f, 0.0f);

    /// @brief Halo thickness in output pixels (default: 1).
    float halo_width = 1.0f;

    /// @brief Number of labels in this layer.
    ///
    /// The `positions` and `strings` arrays are expected to have the same
    /// length; if they differ, the shorter one limits the number of labels.
    size_t count() const { return std::min(positions.size(), strings.size()); }

    /// @brief Whether this layer holds no label with a position and a string.
    bool empty() const { return count() == 0; }

    /// @brief Color of label `i`, falling back to `fallback` when the layer
    ///        holds no colors at all.
    Color color_or(size_t i, const Color &fallback) const {
        if (colors.empty()) {
            return fallback;
        }
        return (i < colors.size()) ? colors[i] : colors.back();
    }

    /// @brief Position of label `i` (origin when out of range).
    Vec3 position_or(size_t i) const {
        return (i < positions.size()) ? positions[i] : Vec3(0.0f);
    }

    /// @brief String of label `i` (empty when out of range).
    const std::string &string_or(size_t i) const {
        static const std::string kEmpty;
        return (i < strings.size()) ? strings[i] : kEmpty;
    }
};

/// @brief A non-owning reference to one text layer in a scene, together with
///        its placement transform (and optional name).
///
/// The text layer counterpart of SceneNodeRef and LineNodeRef.  The transform
/// is applied to the anchor positions; it is ignored for screen-space layers.
///
/// @see Scene::text_nodes(), Scene::add_texts()
struct TextNodeRef {
    const TextLayer *layer = nullptr;  ///< Pointer to the owned layer (never null for a valid node).
    Mat4 transform = Mat4(1.0f);       ///< Model matrix applied to the anchor positions.
    std::string name;                  ///< Optional node name (used by exporters / debugging).
};

/// @brief Internal helpers of the text renderer.
///
/// Everything in this namespace is an implementation detail: it is only public
/// because the renderer lives in a different translation unit.  Use TextLayer
/// with a Scene (or draw_text()) instead.
///
/// @internal
namespace detail {

/// @brief Draw all text layers into an already rendered image.
///
/// Called by the renderer at the end of the render pipeline, where the view
/// projection matrix and the depth buffer of the current frame are known.
///
/// @param layers          The text layers to draw, in order.
/// @param[in,out] output  The image to draw into (supersampled resolution).
/// @param view_projection The combined projection * view matrix of the frame.
/// @param pixel_scale     Supersampling factor the image was rendered with.
/// @param z_buffer        Depth buffer of the frame (empty disables depth
///                        testing); must have `width * height` entries.
/// @param default_color   Color for labels without an explicit color.
///
/// @internal
void render_text_layers(const std::vector<TextNodeRef> &layers, Image &output,
                        const Mat4 &view_projection, float pixel_scale,
                        const std::vector<float> &z_buffer,
                        const Color &default_color);

} // namespace detail

} // namespace scimesh
