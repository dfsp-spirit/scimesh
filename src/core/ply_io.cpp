#define TINYPLY_IMPLEMENTATION
#include "tinyply.h"
#include "ply_io.h"
#include "types.h"

#include <fstream>
#include <stdexcept>

namespace scimesh {
namespace ply_io {

Mesh read_ply(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.good()) {
        throw std::runtime_error("Failed to open PLY file: " + path);
    }

    tinyply::PlyFile ply;
    ply.parse_header(ifs);

    bool has_vertex_colors = false;
    tinyply::Type vertex_color_type = tinyply::Type::UINT8;
    for (const auto &el : ply.get_elements()) {
        if (el.name == "vertex") {
            for (const auto &prop : el.properties) {
                if (prop.name == "red" || prop.name == "green" || prop.name == "blue") {
                    has_vertex_colors = true;
                    vertex_color_type = prop.propertyType;
                    break;
                }
            }
        }
    }

    auto vertices_data = ply.request_properties_from_element("vertex", {"x", "y", "z"});
    auto faces_data = ply.request_properties_from_element("face", {"vertex_indices"}, 3);
    std::shared_ptr<tinyply::PlyData> colors_data;
    if (has_vertex_colors) {
        colors_data = ply.request_properties_from_element("vertex", {"red", "green", "blue"});
    }

    ply.read(ifs);

    Mesh mesh;
    const size_t num_vertices = vertices_data->count;
    mesh.vertices.resize(num_vertices);

    const float *vptr = reinterpret_cast<const float *>(vertices_data->buffer.get_const());
    for (size_t i = 0; i < num_vertices; ++i) {
        mesh.vertices[i] = Vec3(vptr[i * 3], vptr[i * 3 + 1], vptr[i * 3 + 2]);
    }

    if (has_vertex_colors && colors_data) {
        mesh.colors.resize(num_vertices);
        if (vertex_color_type == tinyply::Type::UINT8) {
            const uint8_t *cptr = reinterpret_cast<const uint8_t *>(colors_data->buffer.get_const());
            for (size_t i = 0; i < num_vertices; ++i) {
                mesh.colors[i] = Color(
                    cptr[i * 3] / 255.0f,
                    cptr[i * 3 + 1] / 255.0f,
                    cptr[i * 3 + 2] / 255.0f,
                    1.0f);
            }
        } else if (vertex_color_type == tinyply::Type::FLOAT32) {
            const float *cptr = reinterpret_cast<const float *>(colors_data->buffer.get_const());
            for (size_t i = 0; i < num_vertices; ++i) {
                mesh.colors[i] = Color(cptr[i * 3], cptr[i * 3 + 1], cptr[i * 3 + 2], 1.0f);
            }
        }
    }

    const size_t num_faces = faces_data->count;
    mesh.triangles.resize(num_faces);
    const int32_t *fptr = reinterpret_cast<const int32_t *>(faces_data->buffer.get_const());
    for (size_t i = 0; i < num_faces; ++i) {
        mesh.triangles[i] = Triangle{
            static_cast<uint32_t>(fptr[i * 3]),
            static_cast<uint32_t>(fptr[i * 3 + 1]),
            static_cast<uint32_t>(fptr[i * 3 + 2])};
    }

    return mesh;
}

} // namespace ply_io
} // namespace scimesh
