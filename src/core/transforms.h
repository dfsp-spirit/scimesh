#pragma once

#include "mesh.h"

namespace scimesh {

void translate_mesh(Mesh &mesh, const Vec3 &translation);
void scale_mesh(Mesh &mesh, const Vec3 &scale);
void scale_mesh(Mesh &mesh, float uniform_scale);
void rotate_mesh(Mesh &mesh, float angle_radians, const Vec3 &axis);
void transform_mesh(Mesh &mesh, const Mat4 &matrix);

} // namespace scimesh
