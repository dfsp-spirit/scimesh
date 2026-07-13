#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "obj_io.h"
#include "types.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace scimesh {
namespace obj_io {

Mesh read_obj(const std::string &path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string base_dir;
    auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        base_dir = path.substr(0, slash);
    }

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                               path.c_str(), base_dir.empty() ? nullptr : base_dir.c_str(),
                               true, false);

    if (!warn.empty()) {
        std::cerr << "OBJ warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        throw std::runtime_error("Failed to load OBJ file '" + path + "': " + err);
    }
    if (!ok) {
        throw std::runtime_error("Failed to load OBJ file '" + path + "'");
    }

    Mesh mesh;

    for (const auto &shape : shapes) {
        const auto &indices = shape.mesh.indices;
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            Triangle tri;
            for (int c = 0; c < 3; ++c) {
                const auto &idx = indices[i + c];

                Vec3 pos(
                    attrib.vertices[idx.vertex_index * 3 + 0],
                    attrib.vertices[idx.vertex_index * 3 + 1],
                    attrib.vertices[idx.vertex_index * 3 + 2]);

                mesh.vertices.push_back(pos);

                if (idx.texcoord_index >= 0 &&
                    idx.texcoord_index * 2 + 1 < static_cast<int>(attrib.texcoords.size())) {
                    mesh.uvs.push_back(Vec2(
                        attrib.texcoords[idx.texcoord_index * 2 + 0],
                        1.0f - attrib.texcoords[idx.texcoord_index * 2 + 1]));
                } else {
                    mesh.uvs.push_back(Vec2(0.0f, 0.0f));
                }

                if (idx.normal_index >= 0 &&
                    idx.normal_index * 3 + 2 < static_cast<int>(attrib.normals.size())) {
                    mesh.normals.push_back(Vec3(
                        attrib.normals[idx.normal_index * 3 + 0],
                        attrib.normals[idx.normal_index * 3 + 1],
                        attrib.normals[idx.normal_index * 3 + 2]));
                }

                uint32_t vi = static_cast<uint32_t>(mesh.vertices.size() - 1);
                if (c == 0) tri.v0 = vi;
                if (c == 1) tri.v1 = vi;
                if (c == 2) tri.v2 = vi;
            }
            mesh.triangles.push_back(tri);
        }
    }

    return mesh;
}

} // namespace obj_io
} // namespace scimesh
