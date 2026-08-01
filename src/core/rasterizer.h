/// @file rasterizer.h
/// @brief The Rasterizer — the per-pixel rendering engine.
///
/// The `Rasterizer` converts screen-space triangles into colored pixels,
/// performing depth testing, lighting, texture sampling, and fog/SSAO
/// post-processing.  You typically do **not** use this directly; the
/// `Renderer` class manages it for you.

#pragma once

#include "types.h"
#include "image.h"
#include <vector>

namespace scimesh {

/// @brief Low-level triangle rasterizer with depth buffering and lighting.
///
/// The `Rasterizer` is the heart of the rendering pipeline.  It takes
/// triangles in **screen space** (after all transforms have been applied)
/// and fills in the corresponding pixels, applying:
///
/// - **Z-buffering** (depth testing): only the closest surface at each pixel
///   is kept.
/// - **Blinn-Phong shading**: per-pixel lighting with ambient, diffuse, and
///   specular components.
/// - **Texture mapping**: bilinear sampling from an optional texture image.
/// - **Fog**: linear depth fog for atmospheric effects.
/// - **SSAO**: screen-space ambient occlusion for crevice darkening.
///
/// ## Usage
///
/// You typically don't create a `Rasterizer` directly; it is managed
/// internally by `Renderer`.  However, for advanced use:
///
/// @code{.cpp}
/// Rasterizer rast(800, 600);
/// rast.lights.push_back(Light{Vec3(0,0,1), Color(1,1,1), 1.0f, true});
/// rast.ambient = 0.4f;
/// rast.clear();  // clear depth and normal buffers
/// // ... feed triangles via rasterize_triangle() ...
/// @endcode
///
/// @see Renderer, Mesh, RenderOptions
struct Rasterizer {
    /// @name Output dimensions
    /// @{

    /// @brief Output image width in pixels.
    int width = 0;

    /// @brief Output image height in pixels.
    int height = 0;

    /// @}

    /// @name Depth and normal buffers
    /// @{

    /// @brief Z-buffer (depth buffer), one float per pixel.
    ///
    /// Stores the depth of the closest surface at each pixel.
    /// A new triangle only overwrites a pixel if its depth is closer.
    std::vector<float> z_buffer;

    /// @brief Normal buffer, one Vec3 per pixel.
    ///
    /// Stores the surface normal at each pixel for SSAO computations.
    std::vector<Vec3> normal_buffer;

    /// @}

    /// @name Blending
    /// @{

    /// @brief Enable alpha blending for transparent triangles (default: false).
    ///
    /// When enabled, transparent fragments blend with the existing pixel color
    /// instead of overwriting it.  When disabled, only fully opaque fragments
    /// are drawn (alpha is ignored after the depth test).
    bool blend_mode = false;

    /// @}

    /// @name Specular lighting
    /// @{

    /// @brief Specular highlight color.
    ///
    /// Set to a non-transparent color to enable specular highlights.
    Color specular_color = Color(0.0f, 0.0f, 0.0f, 0.0f);

    /// @brief Shininess exponent (Phong model).
    ///
    /// Higher values = sharper highlights.  Typical range: 16–128.
    float shininess = 0.0f;

    /// @}

    /// @name Lights
    /// @{

    /// @brief Light sources for Blinn-Phong shading.
    ///
    /// If empty, a default directional light from +Z is used.
    /// @see Light
    std::vector<Light> lights;

    /// @brief Ambient light level (0.0–1.0, default: 0.3).
    ///
    /// Uniform fill light that prevents completely black shadows.
    float ambient = 0.3f;

    /// @}

    /// @name Post-processing
    /// @{

    /// @brief Contrast adjustment (1.0 = no change).
    float contrast = 1.0f;

    /// @}

    /// @name Fog
    /// @{

    /// @brief Enable depth fog (default: false).
    bool fog_enabled = false;

    /// @brief Distance where fog begins.
    float fog_start = 0.0f;

    /// @brief Distance where fog is fully opaque.
    float fog_end = 1.0f;

    /// @brief The fog color (what distant objects blend into).
    Color fog_color = TRANSPARENT_BLACK;

    /// @}

    /// @name SSAO (Screen-Space Ambient Occlusion)
    /// @{

    /// @brief Enable SSAO (default: false).
    bool ssao_enabled = false;

    /// @brief SSAO sample radius in pixels (default: 16).
    float ssao_radius = 16.0f;

    /// @brief SSAO darkening intensity (0.0–1.0, default: 0.8).
    float ssao_intensity = 0.8f;

    /// @}

    /// @brief Construct a rasterizer for the given output size.
    ///
    /// Allocates depth and normal buffers.
    ///
    /// @param w Output width in pixels.
    /// @param h Output height in pixels.
    Rasterizer(int w, int h);

    /// @brief Clear the depth and normal buffers.
    ///
    /// Must be called at the start of each frame.
    ///
    /// @param clear_depth Initial depth value (default: 1.0 = farthest).
    void clear(float clear_depth = 1.0f);

    /// @brief Enable or disable alpha blending.
    ///
    /// @param enabled If `true`, transparent fragments blend with the
    ///                existing pixel color.
    void set_blend_mode(bool enabled) { blend_mode = enabled; }

    /// @brief Rasterize a single triangle into the output image.
    ///
    /// The triangle vertices are in **screen space** (pixel coordinates
    /// with depth).  This function performs:
    /// - Bounding-box culling
    /// - Barycentric interpolation of color, normal, UV, and depth
    /// - Z-buffer depth testing
    /// - Backface culling (if enabled)
    /// - Per-pixel shading (or flat shading if `smooth_shading` is false)
    /// - Wireframe edge drawing (if enabled)
    ///
    /// @param screen_v0, screen_v1, screen_v2  Screen-space vertex positions.
    /// @param color0, color1, color2           Per-vertex colors.
    /// @param normal0, normal1, normal2         Per-vertex normals.
    /// @param uv0, uv1, uv2                    Per-vertex texture coordinates.
    /// @param backface_culling                 Whether to cull backfaces.
    /// @param smooth_shading                   Whether to interpolate normals.
    /// @param light_direction                  Light direction for shading.
    /// @param wireframe                        Whether to draw wireframe edges.
    /// @param wireframe_color                  Color for wireframe edges.
    /// @param[in,out] output                   The output image to draw into.
    void rasterize_triangle(
        const Vec3 &screen_v0, const Color &color0, const Vec3 &normal0, const Vec2 &uv0,
        const Vec3 &screen_v1, const Color &color1, const Vec3 &normal1, const Vec2 &uv1,
        const Vec3 &screen_v2, const Color &color2, const Vec3 &normal2, const Vec2 &uv2,
        bool backface_culling,
        bool smooth_shading,
        const Vec3 &light_direction,
        bool wireframe,
        const Color &wireframe_color,
        Image &output);

    /// @brief Rasterize a single point (filled circle) into the output image.
    ///
    /// The point is drawn as a circle of the given radius (in screen pixels).
    /// Depth testing is performed at the circle center.
    ///
    /// @param screen_x, screen_y  Screen-space center position.
    /// @param depth               Depth value for z-buffering.
    /// @param radius              Circle radius in screen pixels.
    /// @param color               Point color.
    /// @param normal              Surface normal (for lighting).
    /// @param light_direction     Light direction for shading.
    /// @param[in,out] output      The output image.
    void rasterize_point(float screen_x, float screen_y, float depth,
                         float radius, const Color &color,
                         const Vec3 &normal, const Vec3 &light_direction,
                         Image &output);

    /// @brief Apply screen-space ambient occlusion to the output image.
    ///
    /// Uses the depth and normal buffers to darken crevices and corners.
    /// Must be called after all triangles have been rasterized.
    ///
    /// @param[in,out] output  The image to modify.
    /// @param z_near          Near plane distance.
    /// @param z_far           Far plane distance.
    void apply_ssao(Image &output, float z_near, float z_far);

    /// @brief Optional texture image for textured meshes.
    ///
    /// Set this before rasterizing textured triangles.  The triangle's UV
    /// coordinates are used to sample this image.
    /// @see Image, Mesh::texture
    Image *active_texture = nullptr;

private:
    /// @brief Compute final pixel color with lighting, then write to output.
    void shade_and_write(int x, int y, float depth,
                         const Color &color, const Vec3 &normal,
                         const Vec3 &light_direction, Image &output);
};

} // namespace scimesh
