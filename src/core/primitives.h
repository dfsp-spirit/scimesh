#pragma once

#include "mesh.h"

namespace scimesh {

Mesh generate_sphere(const Vec3 &center, float radius, int segments, const Color &color);

Mesh generate_cylinder(const Vec3 &start, const Vec3 &end, float radius,
                       int segments, const Color &color);

Mesh generate_cone(const Vec3 &base, const Vec3 &tip, float radius,
                   int segments, const Color &color);

Mesh generate_arrow(const Vec3 &from, const Vec3 &to,
                    float shaft_radius, float head_radius, float head_length,
                    int segments, const Color &color);

void merge_mesh(Mesh &dst, const Mesh &src);

Mesh generate_cuboid(const Vec3 &center, const Vec3 &half_extents,
                     const Color &color);

Mesh generate_pyramid(const Vec3 &base_center, const Vec3 &apex,
                      float half_width, const Color &color);

Mesh generate_tetrahedron(const Vec3 &p0, const Vec3 &p1,
                          const Vec3 &p2, const Vec3 &p3,
                          const Color &color);

Mesh generate_torus(const Vec3 &center, float major_radius, float minor_radius,
                    int major_segments, int minor_segments,
                    const Color &color);

Mesh generate_plane(const Vec3 &center, const Vec3 &normal,
                    float half_size_x, float half_size_y,
                    const Color &color);

Mesh generate_multi_spheres(const std::vector<Vec3> &centers,
                            const std::vector<float> &radii,
                            const std::vector<Color> &colors,
                            int segments = 16);

Mesh generate_multi_cylinders(const std::vector<Vec3> &starts,
                              const std::vector<Vec3> &ends,
                              const std::vector<float> &radii,
                              const std::vector<Color> &colors,
                              int segments = 12);

} // namespace scimesh
