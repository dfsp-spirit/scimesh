#pragma once

#include "mesh.h"
#include <string>

namespace scimesh {
namespace obj_io {

Mesh read_obj(const std::string &path);

} // namespace obj_io
} // namespace scimesh
