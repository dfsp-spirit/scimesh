/// @file lines.h
/// @brief Screen-space line primitives — the LineLayer.
///
/// Line layers are the cheap counterpart of tube meshes (see
/// generate_multi_tubes()): they draw many independent line segments with a
/// width measured in *pixels* instead of world units, so no geometry is created
/// at all.  This is the same behaviour as hardware line rendering
/// (`rgl::segments3d()`, OpenGL `GL_LINES`), which makes line layers the
/// natural choice for wireframes, network/graph edges, trajectories and any
/// other large set of thin lines.

#pragma once

#include <scimesh/types.h>

#include <algorithm>
#include <vector>

namespace scimesh {

/// @brief A batch of independent line segments drawn with a fixed
///        screen-space width.
///
/// A line layer belongs to a Scene (see Scene::add_lines()) and is drawn by the
/// Renderer in the same pass as the meshes: same camera, same depth buffer, so
/// lines can be occluded by meshes and vice versa.  Segments whose colors have
/// an alpha value below 1 are drawn in the blended pass, back-to-front, exactly
/// like translucent triangles.
///
/// Line layers are deliberately **not** meshes:
///
/// - `Scene::compute_bounding_box()` ignores them, so adding lines never
///   changes the camera framing (edges usually live *inside* the meshes).
/// - The mesh exporters (write_gltf(), write_stl(), ...) skip them.
///
/// @par Example
/// @code{.cpp}
/// LineLayer layer;
/// layer.from = {{0,0,0}, {1,0,0}};
/// layer.to   = {{0,1,0}, {1,1,0}};
/// layer.colors = {Color(1,0,0), Color(0,0,1)};
/// layer.width = 2.0f;
/// scene.add_lines(layer);
/// @endcode
///
/// @see Scene::add_lines(), Rasterizer::rasterize_line()
struct LineLayer {
    /// @brief Segment start points, in world space.
    std::vector<Vec3> from;

    /// @brief Segment end points, in world space (one per entry in `from`).
    std::vector<Vec3> to;

    /// @brief One color per segment; may be empty (then the renderer uses
    ///        RenderOptions::default_color) or shorter than `from` (the last
    ///        entry is reused for the remaining segments).
    std::vector<Color> colors;

    /// @brief Line width in screen pixels (default: 1.0).
    ///
    /// The value is interpreted in output pixels; the renderer scales it by the
    /// supersampling factor internally.  A width of 1.0 draws a single-pixel
    /// line, values below 0.5 are treated as 0.5 to keep the line visible.
    float width = 1.0f;

    /// @brief Whether to test the lines against the depth buffer (default: true).
    ///
    /// Set to false to draw the lines on top of everything (e.g. annotation
    /// overlays), which is only useful together with an opaque color.
    bool depth_test = true;

    /// @brief Whether to apply lighting to the lines (default: false).
    ///
    /// Lines are flat by default, like hardware-rendered lines.  When set to
    /// true, the shading uses a fixed surface normal of (0, 0, 1) in view
    /// space, which is rarely what you want for thin lines.
    bool lit = false;

    /// @brief Number of segments in this layer.
    ///
    /// The `from` and `to` arrays are expected to have the same length; if they
    /// differ, the shorter one limits the number of drawn segments.
    size_t size() const { return std::min(from.size(), to.size()); }

    /// @brief Whether this layer holds no segments to draw.
    bool empty() const { return size() == 0; }

    /// @brief Color of segment `i`, falling back to `fallback` when the layer
    ///        holds no colors at all.
    Color color_or(size_t i, const Color &fallback) const {
        if (colors.empty()) {
            return fallback;
        }
        return (i < colors.size()) ? colors[i] : colors.back();
    }
};

/// @brief A non-owning reference to one line layer in a scene, together with
///        its placement transform (and optional name).
///
/// The line layer counterpart of SceneNodeRef.
///
/// @see Scene::line_nodes(), Scene::add_lines()
struct LineNodeRef {
    const LineLayer *layer = nullptr;  ///< Pointer to the owned layer (never null for a valid node).
    Mat4 transform = Mat4(1.0f);       ///< Model matrix placing the lines in world space.
    std::string name;                  ///< Optional node name (used by exporters / debugging).
};

} // namespace scimesh
