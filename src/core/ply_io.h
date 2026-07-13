#pragma once

#include "mesh.h"
#include <string>

namespace scimesh {
namespace ply_io {

Mesh read_ply(const std::string &path);

} // namespace ply_io
} // namespace scimesh
