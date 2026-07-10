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

} // namespace scimesh
