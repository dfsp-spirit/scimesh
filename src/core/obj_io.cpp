#include <scimesh/obj_io.h>
#include <scimesh/fs_mesh_converter.h>
#include "libfs.h"

namespace scimesh {
namespace obj_io {

Mesh read_obj(const std::string &path) {
    fs::Mesh fs_mesh;
    fs::Mesh::from_obj(&fs_mesh, path);

    // Convert to scimesh Mesh.  Normals are not loaded from OBJ — call
    // compute_vertex_normals() on the result if lighting is needed.
    // Texture coordinates (vt) and materials (mtllib) are not loaded.
    return convert_fs_mesh(fs_mesh);
}

} // namespace obj_io
} // namespace scimesh
