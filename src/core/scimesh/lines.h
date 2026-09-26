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
/// - They contribute to the bounding box of their scene (see
///   LineLayer::affects_bounds), which is what makes a scene that consists only
///   of lines renderable, but a layer can opt out of it when it is decoration
///   rather than content, e.g. a leader line to a label or an axis cross.
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

    /// @brief Whether this layer contributes to the bounding box of its scene
    ///        (default: true).
    ///
    /// Line layers usually *are* the content of a figure (graph or connectome
    /// edges, streamlines, tracts), so they define the extent that the camera
    /// has to cover, exactly like the meshes of the scene do.  Set this to
    /// false for a layer that is decoration, e.g. a leader line pointing at a
    /// label outside the anatomy, or an axis cross: such a layer is then
    /// ignored by Scene::compute_bounding_box() and can never push the camera
    /// away from the data.
    ///
    /// Note that a scene without any mesh is framed by its line layers even if
    /// they all opted out, because there would otherwise be no geometry to
    /// derive a camera from, see Scene::compute_bounding_box().
    ///
    /// @see Scene::set_line_affects_bounds()
    bool affects_bounds = true;

    /// @brief Compute the bounding box of all segment endpoints.
    ///
    /// Both the `from` and the `to` points are considered, the width of the
    /// segments is a screen-space property and does not affect the box.
    ///
    /// @param[out] min_bound Lower corner of the box (unchanged when empty).
    /// @param[out] max_bound Upper corner of the box (unchanged when empty).
    /// @return true if the layer contains at least one point, false otherwise.
    ///
    /// @see Scene::compute_bounding_box()
    bool compute_bounding_box(Vec3 &min_bound, Vec3 &max_bound) const {
        bool first = true;
        auto add = [&](const Vec3 &p) {
            if (first) {
                min_bound = p;
                max_bound = p;
                first = false;
            } else {
                min_bound = glm::min(min_bound, p);
                max_bound = glm::max(max_bound, p);
            }
        };
        for (const Vec3 &p : from)
            add(p);
        for (const Vec3 &p : to)
            add(p);
        return !first;
    }

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
