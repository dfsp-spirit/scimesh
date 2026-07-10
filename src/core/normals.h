#pragma once

#include "mesh.h"
#include "math_utils.h"

namespace scimesh {

void compute_vertex_normals(const Mesh &mesh, std::vector<Vec3> &normals);

} // namespace scimesh
