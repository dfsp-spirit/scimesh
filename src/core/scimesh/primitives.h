/// @file primitives.h
/// @brief Procedural geometry generators.
///
/// This file provides functions to create common 3D shapes (spheres, cubes,
/// cylinders, arrows, etc.) without needing to load a mesh file.  These are
/// great for prototyping, creating coordinate axes, or adding simple
/// annotations to scenes.

#pragma once

#include <scimesh/mesh.h>

namespace scimesh {

// ---------------------------------------------------------------------------
//  Single primitives
// ---------------------------------------------------------------------------

/// @brief Generate a UV-sphere (latitude/longitude tessellation).
///
/// The sphere is centered at `center` with the given `radius`.  The
/// `segments` parameter controls how smooth it looks — more segments =
/// rounder but more triangles.
///
/// @param center   Center point of the sphere.
/// @param radius   Radius (half the diameter).
/// @param segments Number of subdivisions (≥ 3).  Typical: 16 (low-poly)
///                 to 64 (smooth).  The sphere has `segments * segments * 2`
///                 triangles.
/// @param color    Uniform color for all vertices.
/// @return A new Mesh with vertices, triangles, and per-vertex normals.
///
/// @par Example
/// @code{.cpp}
/// Mesh ball = generate_sphere({0,0,0}, 1.0f, 32, Color(0.2f, 0.5f, 0.8f));
/// @endcode
///
/// @see generate_torus(), generate_cylinder()
Mesh generate_sphere(const Vec3 &center, float radius, int segments, const Color &color);

/// @brief Generate a cylinder between two endpoints.
///
/// The cylinder runs from `start` to `end` with a circular cross-section
/// of the given radius.  End caps are included.
///
/// @param start    Starting point (center of bottom cap).
/// @param end      Ending point (center of top cap).
/// @param radius   Radius of the cylinder.
/// @param segments Number of sides around the circumference (≥ 3).
/// @param color    Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// Mesh pillar = generate_cylinder({0,0,0}, {0,5,0}, 0.5f, 16,
///                                  Color(0.7f, 0.7f, 0.7f));
/// @endcode
///
/// @see generate_cone(), generate_arrow()
Mesh generate_cylinder(const Vec3 &start, const Vec3 &end, float radius,
                       int segments, const Color &color);

/// @brief Generate a cone from a base circle to a tip point.
///
/// The cone has a circular base centered at `base`, tapering to a point
/// at `tip`.  The base cap is included.
///
/// @param base     Center of the circular base.
/// @param tip      The apex (pointy end) of the cone.
/// @param radius   Radius of the base.
/// @param segments Number of sides (≥ 3).
/// @param color    Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// Mesh cone = generate_cone({0,0,0}, {0,3,0}, 1.0f, 16,
///                            Color(1.0f, 0.5f, 0.0f));
/// @endcode
///
/// @see generate_cylinder(), generate_pyramid()
Mesh generate_cone(const Vec3 &base, const Vec3 &tip, float radius,
                   int segments, const Color &color);

/// @brief Generate a 3D arrow from `from` to `to`.
///
/// An arrow consists of a cylindrical shaft and a conical head.
/// The arrow points from `from` toward `to`.  Both parts share the
/// same color.
///
/// @param from         Starting point (tail of the arrow).
/// @param to           Ending point (tip of the arrowhead).
/// @param shaft_radius Radius of the cylindrical shaft.
/// @param head_radius  Radius of the cone base (arrowhead width).
/// @param head_length  Length of the arrowhead along the arrow direction.
/// @param segments     Number of sides around the circumference.
/// @param color        Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// // Red arrow from origin to (3, 0, 0)
/// Mesh arrow = generate_arrow({0,0,0}, {3,0,0}, 0.1f, 0.3f, 0.6f, 16,
///                               Color(1,0,0));
/// @endcode
///
/// @see generate_cylinder(), generate_cone()
Mesh generate_arrow(const Vec3 &from, const Vec3 &to,
                    float shaft_radius, float head_radius, float head_length,
                    int segments, const Color &color);

/// @brief Merge one mesh into another (append geometry).
///
/// All vertices, triangles, colors, normals, and UVs from `src` are appended
/// to `dst`.  Triangle indices in `src` are offset to account for existing
/// vertices in `dst`.
///
/// @param[in,out] dst The destination mesh (modified in-place).
/// @param         src The source mesh (not modified).
///
/// @par Example
/// @code{.cpp}
/// Mesh combined = generate_sphere({-1,0,0}, 0.5f, 16, Color(1,0,0));
/// merge_mesh(combined, generate_sphere({1,0,0}, 0.5f, 16, Color(0,1,0)));
/// // combined now has two spheres
/// @endcode
///
/// @see generate_multi_spheres(), generate_multi_cylinders()
void merge_mesh(Mesh &dst, const Mesh &src);

/// @brief Generate an axis-aligned cuboid (rectangular box).
///
/// The box is centered at `center` and extends `half_extents` in each
/// direction (i.e., the full dimensions are `2 * half_extents`).
///
/// @param center       Center of the box.
/// @param half_extents Half-width, half-height, half-depth (all positive).
/// @param color        Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// // A 2×1×3 box centered at the origin
/// Mesh box = generate_cuboid({0,0,0}, {1, 0.5, 1.5}, Color(0.5, 0.5, 0.5));
/// @endcode
///
/// @see generate_sphere(), generate_pyramid()
Mesh generate_cuboid(const Vec3 &center, const Vec3 &half_extents,
                     const Color &color);

/// @brief Generate a square-based pyramid.
///
/// The base is a square in the XZ plane centered at `base_center`.
/// The apex is above the base.
///
/// @param base_center Center of the square base.
/// @param apex        The top point (tip) of the pyramid.
/// @param half_width  Half the side length of the square base.
/// @param color       Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// Mesh pyramid = generate_pyramid({0,0,0}, {0,2,0}, 1.0f,
///                                   Color(0.8f, 0.6f, 0.2f));
/// @endcode
///
/// @see generate_cone(), generate_tetrahedron()
Mesh generate_pyramid(const Vec3 &base_center, const Vec3 &apex,
                      float half_width, const Color &color);

/// @brief Generate a tetrahedron (triangular pyramid) from four points.
///
/// A tetrahedron is the simplest 3D shape — a pyramid with a triangular
/// base and three triangular sides.
///
/// @param p0, p1, p2, p3 The four corner points.
/// @param color           Uniform color.
/// @return A new Mesh.
///
/// @par Example
/// @code{.cpp}
/// Mesh tet = generate_tetrahedron(
///     {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1},
///     Color(0.3f, 0.7f, 0.3f));
/// @endcode
///
/// @see generate_pyramid()
Mesh generate_tetrahedron(const Vec3 &p0, const Vec3 &p1,
                          const Vec3 &p2, const Vec3 &p3,
                          const Color &color);

/// @brief Generate a torus (donut shape).
///
/// A torus is the surface of a ring.  `major_radius` is the distance from
/// the center of the hole to the center of the tube.  `minor_radius` is
/// the radius of the tube itself.
///
/// @param center          Center of the torus.
/// @param major_radius    Distance from hole center to tube center.
/// @param minor_radius    Radius of the tube cross-section.
/// @param major_segments  Subdivisions around the ring (≥ 4).
/// @param minor_segments  Subdivisions around the tube (≥ 4).
/// @param color           Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// Mesh donut = generate_torus({0,0,0}, 2.0f, 0.5f, 32, 16,
///                               Color(0.9f, 0.6f, 0.3f));
/// @endcode
///
/// @see generate_sphere(), generate_cylinder()
Mesh generate_torus(const Vec3 &center, float major_radius, float minor_radius,
                    int major_segments, int minor_segments,
                    const Color &color);

/// @brief Generate a flat rectangular plane.
///
/// The plane is a single quad (two triangles) centered at `center`,
/// oriented perpendicular to the given `normal`.
///
/// @param center       Center point of the plane.
/// @param normal       Surface normal (direction the plane faces).
/// @param half_size_x  Half the width in the local X direction.
/// @param half_size_y  Half the height in the local Y direction.
/// @param color        Uniform color.
/// @return A new Mesh with normals.
///
/// @par Example
/// @code{.cpp}
/// Mesh floor = generate_plane({0,-1,0}, {0,1,0}, 10.0f, 10.0f,
///                               Color(0.3f, 0.3f, 0.3f));
/// @endcode
///
/// @see generate_cuboid()
Mesh generate_plane(const Vec3 &center, const Vec3 &normal,
                    float half_size_x, float half_size_y,
                    const Color &color);

// ---------------------------------------------------------------------------
//  Batch primitives (efficient for many small shapes)
// ---------------------------------------------------------------------------

/// @brief Generate multiple spheres in a single mesh (efficient batching).
///
/// Much faster than calling `generate_sphere()` many times and merging —
/// all spheres share a single mesh with one vertex/triangle array.
///
/// @param centers  Array of center points.
/// @param radii    Array of radii (same length as `centers`).
/// @param colors   Array of colors (same length as `centers`).
/// @param segments Subdivisions per sphere (default: 16).
/// @return A single Mesh containing all spheres.
///
/// @par Example
/// @code{.cpp}
/// std::vector<Vec3> pts = {{0,0,0}, {2,0,0}, {0,2,0}};
/// std::vector<float> rads = {0.5, 0.3, 0.4};
/// std::vector<Color> cls = {Color(1,0,0), Color(0,1,0), Color(0,0,1)};
/// Mesh spheres = generate_multi_spheres(pts, rads, cls, 16);
/// @endcode
///
/// @see generate_sphere(), generate_multi_cylinders()
Mesh generate_multi_spheres(const std::vector<Vec3> &centers,
                            const std::vector<float> &radii,
                            const std::vector<Color> &colors,
                            int segments = 16);

/// @brief Generate multiple cylinders in a single mesh (efficient batching).
///
/// Much faster than calling `generate_cylinder()` many times —
/// all cylinders share a single mesh.
///
/// @param starts   Array of start points.
/// @param ends     Array of end points (same length as `starts`).
/// @param radii    Array of radii (same length).
/// @param colors   Array of colors (same length).
/// @param segments Subdivisions per cylinder (default: 12).
/// @return A single Mesh containing all cylinders.
///
/// @par Example
/// @code{.cpp}
/// std::vector<Vec3> starts = {{0,0,0}, {1,0,0}};
/// std::vector<Vec3> ends   = {{0,3,0}, {1,3,0}};
/// std::vector<float> rads  = {0.1f, 0.1f};
/// std::vector<Color> cols  = {Color(1,1,1), Color(1,1,1)};
/// Mesh pillars = generate_multi_cylinders(starts, ends, rads, cols, 12);
/// @endcode
///
/// @see generate_cylinder(), generate_multi_spheres()
Mesh generate_multi_cylinders(const std::vector<Vec3> &starts,
                              const std::vector<Vec3> &ends,
                              const std::vector<float> &radii,
                              const std::vector<Color> &colors,
                              int segments = 12);

} // namespace scimesh
