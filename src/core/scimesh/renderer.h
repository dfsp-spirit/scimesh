/// @file renderer.h
/// @brief The Renderer — the main entry point for drawing meshes to images.
///
/// The `Renderer` class takes meshes, a camera, and render options and
/// produces a 2D image.  This is the primary API surface for C++ users.

#pragma once

#include <scimesh/mesh.h>
#include <scimesh/scene.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/image.h>

namespace scimesh {

/// @brief The main rendering engine.
///
/// ## Overview
///
/// The `Renderer` produces 2D images from 3D geometry using a software
/// rasterization pipeline:
///
/// 1. **Vertex processing**: transform vertices through the
///    model→view→projection pipeline.
/// 2. **Clipping**: discard or clip triangles outside the view frustum
///    and clip planes.
/// 3. **Rasterization**: convert triangles to pixel fragments, computing
///    color, depth, and lighting per pixel.
/// 4. **Post-processing**: apply anti-aliasing, SSAO, fog, and contrast.
///
/// ## Basic usage
///
/// @code{.cpp}
/// #include <scimesh/core/renderer.h>
///
/// Mesh mesh = scimesh::generate_sphere({0,0,0}, 1.0f, 32,
///                                       Color(0.2f, 0.5f, 0.8f));
/// Camera cam = camera_fit_mesh(mesh, {0,0,1}, {0,1,0}, 45.0f);
/// RenderOptions opts;
/// opts.width = 800;
/// opts.height = 600;
///
/// Renderer renderer;
/// Image result = renderer.render_mesh(mesh, cam, opts);
/// result.write_png("sphere.png");
/// @endcode
///
/// ## Rendering multiple meshes
///
/// For multiple meshes, use a `Scene`:
///
/// @code{.cpp}
/// Scene scene;
/// scene.meshes.push_back(sphere1);
/// scene.meshes.push_back(cube1);
/// Image result = renderer.render_scene(scene, cam, opts);
/// @endcode
///
/// ## Raw rendering (no mesh needed)
///
/// For programmatic point clouds or triangle soups:
///
/// @code{.cpp}
/// // Render 100 random points as a point cloud
/// std::vector<Vec3> positions = /* ... */;
/// std::vector<Color> colors = /* ... */;
/// Image result = renderer.render_points_raw(positions, colors,
///                                            0.02f, cam, opts);
/// @endcode
///
/// @see Mesh, Scene, Camera, RenderOptions, Image, Rasterizer
class Renderer {
public:
    /// @brief Render a single mesh to an image.
    ///
    /// @param mesh    The mesh to render.
    /// @param camera  The camera defining the viewpoint.
    /// @param options Rendering settings (size, lighting, etc.).
    /// @return The rendered image.
    ///
    /// @see render_scene(), render_triangles_raw(), render_points_raw()
    Image render_mesh(const Mesh &mesh, const Camera &camera, const RenderOptions &options);

    /// @brief Render a scene (collection of meshes) to an image.
    ///
    /// All meshes are drawn into the same image in the order they appear
    /// in the scene.  Later meshes are drawn on top of earlier ones.
    ///
    /// @param scene   The scene containing one or more meshes.
    /// @param camera  The camera.
    /// @param options Rendering settings.
    /// @return The rendered image.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Scene scene;
    /// scene.meshes.push_back(background_plane);
    /// scene.meshes.push_back(main_object);
    /// Image result = renderer.render_scene(scene, cam, opts);
    /// @endcode
    ///
    /// @see render_mesh(), Scene
    Image render_scene(const Scene &scene, const Camera &camera, const RenderOptions &options);

    /// @brief Render raw triangles (no Mesh wrapper) to an image.
    ///
    /// Each consecutive triplet of positions forms a triangle.
    /// `positions.size()` must be a multiple of 3, and `colors` must have
    /// the same number of elements.
    ///
    /// @param positions Flat array of vertex positions (3 per triangle).
    /// @param colors    Per-vertex colors (same size as `positions`).
    /// @param camera    The camera.
    /// @param options   Rendering settings.
    /// @return The rendered image.
    ///
    /// @see render_points_raw(), render_mesh()
    Image render_triangles_raw(const std::vector<Vec3> &positions,
                                const std::vector<Color> &colors,
                                const Camera &camera,
                                const RenderOptions &options);

    /// @brief Render a point cloud (spheres at each position) to an image.
    ///
    /// Each point is drawn as a small filled circle of the given radius
    /// (in screen-space pixels).  Points are depth-tested against each other
    /// and against previously drawn geometry.
    ///
    /// @param positions Point positions in world space.
    /// @param colors    Per-point colors (same size as `positions`).
    /// @param radius    Screen-space radius of each point in pixels.
    /// @param camera    The camera.
    /// @param options   Rendering settings.
    /// @return The rendered image.
    ///
    /// @par Example
    /// @code{.cpp}
    /// std::vector<Vec3> pts = {{0,0,0}, {1,0,0}, {0,1,0}};
    /// std::vector<Color> cols = {Color(1,0,0), Color(0,1,0), Color(0,0,1)};
    /// Image result = renderer.render_points_raw(pts, cols, 3.0f, cam, opts);
    /// @endcode
    ///
    /// @see render_triangles_raw(), render_mesh()
    Image render_points_raw(const std::vector<Vec3> &positions,
                            const std::vector<Color> &colors,
                            float radius,
                            const Camera &camera,
                            const RenderOptions &options);

private:
    /// @brief Internal pipeline: transforms, clips, and rasterizes meshes.
    ///
    /// Each node's placement transform is applied as a model matrix before
    /// the view transform.
    void render_pipeline(const std::vector<SceneNodeRef> &nodes,
                         const Camera &camera,
                         const RenderOptions &options,
                         Image &output);
};

} // namespace scimesh
