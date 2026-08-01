/// @file camera.h
/// @brief Camera definition and helper functions for view setup.
///
/// The camera controls **what** part of the 3D scene is visible and **how**
/// it is projected onto the 2D image.  scimesh supports two projection types:
/// perspective (realistic, with foreshortening) and orthographic (flat, no
/// perspective, useful for technical diagrams).

#pragma once

#include "types.h"
#include "scene.h"

namespace scimesh {

// ---------------------------------------------------------------------------
//  ProjectionType
// ---------------------------------------------------------------------------

/// @brief The type of 3D→2D projection used by the camera.
///
/// @see Camera, Camera::projection
enum class ProjectionType {
    /// @brief Orthographic projection: parallel lines stay parallel.
    ///
    /// Objects do not get smaller with distance.  Useful for technical
    /// diagrams, architectural plans, and when you need to preserve
    /// relative sizes regardless of depth.
    ORTHOGRAPHIC,

    /// @brief Perspective projection: objects farther away appear smaller.
    ///
    /// This mimics how the human eye and real cameras work.  The field of
    /// view (FOV) controls how "wide" the lens is.  This is the default.
    PERSPECTIVE
};

// ---------------------------------------------------------------------------
//  Camera
// ---------------------------------------------------------------------------

/// @brief A virtual camera that defines the viewpoint for rendering.
///
/// ## How a camera works
///
/// A camera is defined by three things:
/// - **Position** (`eye`): where the camera sits in 3D space.
/// - **Look-at point** (`center`): what the camera is pointing at.
/// - **Up direction** (`up`): which way is "up" on screen (usually +Y).
///
/// Together these define a **view matrix** that transforms world coordinates
/// into camera coordinates.
///
/// The **projection matrix** (orthographic or perspective) then maps camera
/// coordinates to clip space, which the rasterizer converts to screen pixels.
///
/// ## Construction
///
/// You rarely need to set all fields manually.  Use one of the convenience
/// functions:
///
/// @code{.cpp}
/// // Fit a single mesh in view automatically
/// Camera cam = camera_fit_mesh(mesh, {0, 0, 1}, {0, 1, 0}, 45.0f);
///
/// // Manual setup
/// Camera cam;
/// cam.eye    = {5, 3, 10};      // camera position
/// cam.center = {0, 0, 0};       // look at origin
/// cam.up     = {0, 1, 0};       // Y is up
/// cam.fov_degrees = 45.0f;      // 45° field of view
/// cam.projection = ProjectionType::PERSPECTIVE;
/// @endcode
///
/// @see camera_look_at(), camera_fit_mesh(), camera_fit_scene()
/// @see camera_orbit(), ProjectionType
struct Camera {
    /// @brief Camera position in world space.
    ///
    /// Where the "eye" sits.  Default: (0, 0, 10), looking toward the
    /// origin from 10 units away on the +Z axis.
    Vec3 eye = Vec3(0.0f, 0.0f, 10.0f);

    /// @brief The point the camera looks at.
    ///
    /// Together with `eye`, this defines the view direction as
    /// `center - eye`.  Default: the origin (0, 0, 0).
    Vec3 center = Vec3(0.0f, 0.0f, 0.0f);

    /// @brief The camera's "up" direction.
    ///
    /// Usually (0, 1, 0) for Y-up scenes.  Must not be parallel to the
    /// view direction (`center - eye`).
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);

    /// @brief Which projection type to use.
    ///
    /// @see ProjectionType
    ProjectionType projection = ProjectionType::PERSPECTIVE;

    /// @brief Vertical field of view in degrees (perspective only).
    ///
    /// Controls how wide the lens is.  Typical values:
    /// - 30° — telephoto (narrow, zoomed in)
    /// - 45° — normal (default)
    /// - 60°–90° — wide angle
    ///
    /// Ignored when `projection == ORTHOGRAPHIC`.
    float fov_degrees = 45.0f;

    /// @brief Compute the view matrix (world → camera space).
    ///
    /// This is a standard look-at matrix.  You typically don't need to call
    /// this yourself — the renderer does it internally.
    ///
    /// @return A 4×4 view matrix.
    Mat4 get_view_matrix() const;

    /// @brief Compute the projection matrix (camera → clip space).
    ///
    /// @param aspect_ratio Image width / height (e.g., 800/600 = 1.333).
    /// @param near_plane   Distance to the near clipping plane.
    /// @param far_plane    Distance to the far clipping plane.
    /// @return A 4×4 projection matrix.
    Mat4 get_projection_matrix(float aspect_ratio, float near_plane, float far_plane) const;
};

// ---------------------------------------------------------------------------
//  Camera helper functions
// ---------------------------------------------------------------------------

/// @brief Compute the "perpendicular extent radius" of an axis-aligned
///        bounding box relative to a view direction.
///
/// This is an internal helper used by `camera_look_at()` to compute how far
/// the camera needs to be to keep the entire bounding box in view.  It is
/// tighter than a simple sphere-based method because it accounts for the
/// actual box shape.
///
/// @param bmin        Minimum corner of the AABB.
/// @param bmax        Maximum corner of the AABB.
/// @param center      The center point the camera looks at.
/// @param dir         The view direction (forward vector).
/// @param fov_radians Vertical FOV in radians.
/// @param[out] out_dist Optional: receives the computed camera distance.
/// @return The equivalent sphere radius.
///
/// @see camera_look_at()
inline float perp_extent_radius(const Vec3 &bmin, const Vec3 &bmax,
                                 const Vec3 &center, const Vec3 &dir,
                                 float fov_radians, float *out_dist = nullptr) {
    Vec3 corners[8] = {
        Vec3(bmin.x, bmin.y, bmin.z), Vec3(bmax.x, bmin.y, bmin.z),
        Vec3(bmin.x, bmax.y, bmin.z), Vec3(bmax.x, bmax.y, bmin.z),
        Vec3(bmin.x, bmin.y, bmax.z), Vec3(bmax.x, bmin.y, bmax.z),
        Vec3(bmin.x, bmax.y, bmax.z), Vec3(bmax.x, bmax.y, bmax.z),
    };
    float max_dist = 0.0f;
    float half_tan = std::tan(fov_radians * 0.5f);
    if (half_tan < 1e-6f) half_tan = 1e-6f;

    for (int i = 0; i < 8; i++) {
        Vec3 delta = corners[i] - center;
        float along  = glm::dot(delta, dir);
        float perp_sq = glm::dot(delta, delta) - along * along;
        float perp = std::sqrt(perp_sq);
        // At the corner's depth (centre_z + along), required eye distance
        // so that this corner stays inside the frustum:
        //   perp  ≤  (dist - along) * tan(half_fov)
        // ⇒ dist  ≥  perp / tan(half_fov)  +  along
        float d = perp / half_tan + along;
        if (d > max_dist) max_dist = d;
    }
    if (max_dist < 1e-6f) max_dist = 1.0f;

    if (out_dist) *out_dist = max_dist;
    // Convert back to the "sphere radius" that camera_look_at expects.
    float sin_half = std::sin(fov_radians * 0.5f);
    if (sin_half < 1e-6f) sin_half = 1e-6f;
    return max_dist * sin_half;
}

/// @brief Compute the maximum perpendicular extent of an AABB from a view ray
///        (for orthographic projection framing).
///
/// Unlike perp_extent_radius(), this does not depend on field of view —
/// it is used to set the orthographic frustum size.
///
/// @param bmin   Minimum corner of the AABB.
/// @param bmax   Maximum corner of the AABB.
/// @param center The center point the camera looks at.
/// @param dir    The view direction.
/// @return The maximum perpendicular distance from the ray through `center`.
///
/// @see perp_extent_radius(), camera_look_at()
inline float max_ortho_extent(const Vec3 &bmin, const Vec3 &bmax,
                               const Vec3 &center, const Vec3 &dir) {
    Vec3 corners[8] = {
        Vec3(bmin.x, bmin.y, bmin.z), Vec3(bmax.x, bmin.y, bmin.z),
        Vec3(bmin.x, bmax.y, bmin.z), Vec3(bmax.x, bmax.y, bmin.z),
        Vec3(bmin.x, bmin.y, bmax.z), Vec3(bmax.x, bmin.y, bmax.z),
        Vec3(bmin.x, bmax.y, bmax.z), Vec3(bmax.x, bmax.y, bmax.z),
    };
    float max_perp = 0.0f;
    for (int i = 0; i < 8; i++) {
        Vec3 delta = corners[i] - center;
        float along = glm::dot(delta, dir);
        float perp = std::sqrt(std::max(0.0f, glm::dot(delta, delta) - along * along));
        if (perp > max_perp) max_perp = perp;
    }
    if (max_perp < 1e-6f) max_perp = 1.0f;
    return max_perp;
}

/// @brief Create a camera that looks at a point from a given distance and
///        direction.
///
/// This is the low-level function for camera setup.  It computes the exact
/// eye position needed to frame a sphere of `radius` centered at `center`.
///
/// @param center      The point to look at (becomes Camera::center).
/// @param radius      The radius of a bounding sphere around the subject.
/// @param direction   View direction vector (e.g., {0,0,1} for front view).
/// @param up          Up vector (usually {0,1,0}).
/// @param fov_degrees Vertical field of view in degrees.
/// @param margin      Extra margin factor (>1.0 = zoomed out, <1.0 = tighter).
///                    Default 1.1 gives 10% padding.
/// @param projection  Projection type (default: PERSPECTIVE).
/// @return A configured Camera.
///
/// @par Example
/// @code{.cpp}
/// Camera cam = camera_look_at({0,0,0}, 5.0f, {0,0,1}, {0,1,0}, 45.0f, 1.2f);
/// // Looks at origin from +Z, framing a sphere of radius 5, with 20% margin
/// @endcode
///
/// @see camera_fit_mesh(), camera_fit_scene()
Camera camera_look_at(const Vec3 &center, float radius,
                      const Vec3 &direction, const Vec3 &up,
                      float fov_degrees, float margin = 1.1f,
                      ProjectionType projection = ProjectionType::PERSPECTIVE);

/// @brief Create a camera that automatically frames an entire Scene.
///
/// Computes the combined bounding box of all meshes and sets up the camera
/// to show everything.
///
/// @param scene       The scene to frame.
/// @param direction   View direction (e.g., {0,0,1} for front view).
/// @param up          Up vector (e.g., {0,1,0}).
/// @param fov_degrees Field of view in degrees.
/// @param margin      Extra zoom margin (default 1.1 = 10% padding).
/// @param projection  Projection type (default: PERSPECTIVE).
/// @return A configured Camera.
///
/// @par Example
/// @code{.cpp}
/// Scene scene;
/// scene.meshes.push_back(sphere1);
/// scene.meshes.push_back(cube1);
/// Camera cam = camera_fit_scene(scene, {1,1,1}, {0,1,0}, 45.0f);
/// // camera now frames both objects from a diagonal viewpoint
/// @endcode
///
/// @see camera_fit_mesh(), camera_look_at()
Camera camera_fit_scene(const Scene &scene, const Vec3 &direction,
                        const Vec3 &up, float fov_degrees, float margin = 1.1f,
                        ProjectionType projection = ProjectionType::PERSPECTIVE);

/// @brief Create a camera that automatically frames a single Mesh.
///
/// Convenience wrapper around `camera_fit_scene()` for the common case of
/// rendering a single mesh.
///
/// @param mesh        The mesh to frame.
/// @param direction   View direction.
/// @param up          Up vector.
/// @param fov_degrees Field of view in degrees.
/// @param margin      Extra zoom margin.
/// @param projection  Projection type.
/// @return A configured Camera.
///
/// @par Example
/// @code{.cpp}
/// Mesh brain = scimesh::obj_io::read_obj("brain.obj");
/// Camera cam = camera_fit_mesh(brain, {0,0,1}, {0,1,0}, 45.0f);
/// @endcode
///
/// @see camera_fit_scene(), camera_look_at()
Camera camera_fit_mesh(const Mesh &mesh, const Vec3 &direction,
                       const Vec3 &up, float fov_degrees, float margin = 1.1f,
                       ProjectionType projection = ProjectionType::PERSPECTIVE);

/// @brief Orbit the camera around its look-at point.
///
/// Rotates the camera's eye position around `camera.center` by the given
/// angle around the given axis.  The up vector is also rotated.
///
/// @param camera        The camera to orbit (modified copy returned).
/// @param axis          Rotation axis (should pass through `camera.center`).
/// @param angle_degrees Rotation angle in degrees.
/// @return A new Camera with the rotated eye position.
///
/// @par Example
/// @code{.cpp}
/// Camera cam = camera_fit_mesh(mesh, {0,0,1}, {0,1,0}, 45.0f);
/// // Rotate 30° around the Y axis (horizontal orbit)
/// Camera cam2 = camera_orbit(cam, {0,1,0}, 30.0f);
/// @endcode
///
/// @see camera_look_at()
Camera camera_orbit(const Camera &camera, const Vec3 &axis, float angle_degrees);

} // namespace scimesh
