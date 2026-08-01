/// @file render_options.h
/// @brief RenderOptions — all settings that control how meshes are drawn.
///
/// The `RenderOptions` struct is passed to every `Renderer::render_*()` call.
/// It controls output size, shading, lighting, anti-aliasing, fog, clipping
/// planes, and more.

#pragma once

#include "types.h"
#include "camera.h"

namespace scimesh {

// ---------------------------------------------------------------------------
//  ShadingMode
// ---------------------------------------------------------------------------

/// @brief How surface normals are interpolated across triangles.
///
/// @see RenderOptions::shading
enum class ShadingMode {
    /// @brief Smooth (Gouraud) shading: normals are interpolated across each
    ///        triangle, producing a smooth, rounded appearance.
    ///
    /// Best for curved surfaces like spheres.  Requires per-vertex normals
    /// (use compute_vertex_normals() to generate them).
    SMOOTH,

    /// @brief Flat shading: each triangle uses a single normal, giving a
    ///        faceted, low-poly look.
    ///
    /// Best for mechanical parts, cubes, or when you want to emphasize the
    /// mesh structure.
    FLAT
};

// ---------------------------------------------------------------------------
//  RenderOptions
// ---------------------------------------------------------------------------

/// @brief All settings that control rendering output.
///
/// ## Quick start
///
/// The defaults produce a reasonable result for most cases:
/// @code{.cpp}
/// RenderOptions opts;  // all defaults
/// opts.width  = 1920;
/// opts.height = 1080;  // Full HD output
/// @endcode
///
/// ## Adding lights
///
/// @code{.cpp}
/// opts.lights.push_back(Light{Vec3(0,0,1), Color(1,1,1), 1.0f, true});
/// opts.ambient = 0.4f;  // brighter ambient light
/// @endcode
///
/// ## Anti-aliasing
///
/// @code{.cpp}
/// opts.aa_samples = 4;  // 4× supersampling for smooth edges
/// @endcode
///
/// ## Wireframe overlay
///
/// @code{.cpp}
/// opts.wireframe = true;
/// opts.wireframe_color = Color(0,0,0);  // black edges
/// @endcode
///
/// @see Renderer, Light, ClipPlane, ShadingMode
struct RenderOptions {
    /// @name Output size
    /// @{

    /// @brief Image width in pixels (default: 800).
    int width = 800;

    /// @brief Image height in pixels (default: 600).
    int height = 600;

    /// @}

    /// @name Shading
    /// @{

    /// @brief Shading mode (default: SMOOTH).
    ///
    /// `SMOOTH` interpolates normals across triangles for a rounded look.
    /// `FLAT` uses one normal per triangle for a faceted look.
    ///
    /// @see ShadingMode
    ShadingMode shading = ShadingMode::SMOOTH;

    /// @brief Enable backface culling (default: true).
    ///
    /// When enabled, triangles facing away from the camera are not drawn.
    /// This is an optimization and is almost always what you want.
    /// Disable only if you need to see both sides of single-sided geometry.
    bool backface_culling = true;

    /// @brief Flip all surface normals (default: false).
    ///
    /// Useful when normals from a file point inward instead of outward.
    bool invert_normals = false;

    /// @}

    /// @name Colors
    /// @{

    /// @brief Background color (default: transparent black).
    ///
    /// Pixels not covered by any triangle get this color.
    /// @see TRANSPARENT_BLACK
    Color background_color = TRANSPARENT_BLACK;

    /// @brief Default color for meshes without explicit vertex/face colors.
    ///
    /// Overridden by per-mesh `Mesh::default_color`.
    /// @see DEFAULT_COLOR
    Color default_color = DEFAULT_COLOR;

    /// @}

    /// @name Wireframe
    /// @{

    /// @brief Draw triangle edges as lines (default: false).
    ///
    /// When enabled, each triangle edge is drawn on top of the filled
    /// triangle.  Wireframe thickness is 1 pixel.
    bool wireframe = false;

    /// @brief Color of the wireframe lines (default: black).
    Color wireframe_color = Color(0.0f, 0.0f, 0.0f, 1.0f);

    /// @}

    /// @name Anti-aliasing
    /// @{

    /// @brief Anti-aliasing sample count (default: 1 = no AA).
    ///
    /// Super-sampling anti-aliasing: each pixel is sampled `aa_samples` times
    /// in each direction, then averaged.  A value of 4 means 16 samples per
    /// pixel total.  Higher values produce smoother edges but are slower and
    /// use more memory.
    int aa_samples = 1;

    /// @}

    /// @name Specular highlights
    /// @{

    /// @brief Specular highlight color (default: transparent = no specular).
    ///
    /// Set to white or a tinted color to add shiny highlights.
    Color specular_color = Color(0.0f, 0.0f, 0.0f, 0.0f);

    /// @brief Shininess exponent (default: 0.0 = no specular).
    ///
    /// Higher values produce sharper, more focused highlights.
    /// Typical range: 16–128.  Only used when `specular_color.a > 0`.
    float shininess = 0.0f;

    /// @}

    /// @name Projection
    /// @{

    /// @brief Projection type (default: PERSPECTIVE).
    ///
    /// @see ProjectionType
    ProjectionType projection = ProjectionType::PERSPECTIVE;

    /// @}

    /// @name Lighting
    /// @{

    /// @brief List of light sources.
    ///
    /// If empty, a default single directional light from +Z is used.
    /// @see Light
    std::vector<Light> lights;

    /// @brief Ambient light level (default: 0.3).
    ///
    /// Ambient light is uniform "fill" light that illuminates all surfaces
    /// equally, preventing shadowed areas from being completely black.
    /// Range: 0.0 (no ambient) to 1.0 (fully lit).  Typical: 0.2–0.4.
    float ambient = 0.3f;

    /// @}

    /// @name Post-processing
    /// @{

    /// @brief Contrast adjustment (default: 1.0 = no change).
    ///
    /// > 1.0 increases contrast, < 1.0 reduces it.
    float contrast = 1.0f;

    /// @}

    /// @name Clipping planes
    /// @{

    /// @brief Optional clip planes for cross-section views.
    ///
    /// Each plane removes geometry on its negative side.
    /// @see ClipPlane
    std::vector<ClipPlane> clip_planes;

    /// @}

    /// @name Fog
    /// @{

    /// @brief Enable depth fog (default: false).
    ///
    /// When enabled, objects fade toward `fog_color` based on their distance
    /// from the camera.
    bool fog_enabled = false;

    /// @brief Distance at which fog begins (in world units).
    float fog_start = 0.0f;

    /// @brief Distance at which fog is fully opaque (in world units).
    float fog_end = 1.0f;

    /// @brief The color that distant objects fade into.
    Color fog_color = TRANSPARENT_BLACK;

    /// @}

    /// @name Threading
    /// @{

    /// @brief Number of render threads (default: 0 = auto-detect).
    ///
    /// The renderer can use multiple threads for triangle rasterization.
    /// Set to 1 to force single-threaded rendering.
    int threads = 0;

    /// @}

    /// @name Screen-space ambient occlusion (SSAO)
    /// @{

    /// @brief Enable SSAO (default: false).
    ///
    /// SSAO darkens crevices and corners to add depth and realism.
    bool ssao_enabled = false;

    /// @brief SSAO sample radius in pixels (default: 16).
    float ssao_radius = 16.0f;

    /// @brief SSAO darkening intensity (default: 0.8).
    ///
    /// 0.0 = no effect, 1.0 = full effect.
    float ssao_intensity = 0.8f;

    /// @}

    /// @name Clipping distances
    /// @{

    /// @brief Near clipping plane distance (default: 0.1).
    ///
    /// Geometry closer than this is not rendered.  Reduce if you need to
    /// see very close objects; increase if you have z-fighting issues.
    float near_plane = 0.1f;

    /// @brief Far clipping plane distance (default: 10000.0).
    ///
    /// Geometry farther than this is not rendered.  Increase for massive
    /// scenes; decrease to improve depth precision.
    float far_plane = 10000.0f;

    /// @}
};

} // namespace scimesh
