#define TINYPLY_IMPLEMENTATION
#include "tinyply.h"
#include <scimesh/ply_io.h>
#include <scimesh/types.h>

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace scimesh {
namespace ply_io {

namespace {

/// @brief Read the i-th alpha value of a PLY vertex element, normalized to [0, 1].
///
/// PLY files store alpha either as an 8-bit integer (the common case) or as a
/// float in [0, 1].  Types that are not handled fall back to fully opaque.
float read_alpha(const tinyply::PlyData &data, size_t i) {
    const uint8_t *raw = data.buffer.get_const();
    switch (data.t) {
        case tinyply::Type::UINT8:
            return raw[i] / 255.0f;
        case tinyply::Type::INT8: {
            int8_t v = 0;
            std::memcpy(&v, raw + i, sizeof(v));
            return std::max<int>(0, v) / 255.0f;
        }
        case tinyply::Type::UINT16: {
            uint16_t v = 0;
            std::memcpy(&v, raw + i * sizeof(v), sizeof(v));
            return v / 65535.0f;
        }
        case tinyply::Type::FLOAT32: {
            float v = 0.0f;
            std::memcpy(&v, raw + i * sizeof(v), sizeof(v));
            return v;
        }
        default:
            return 1.0f;
    }
}

} // anonymous namespace

Mesh read_ply(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.good()) {
        throw std::runtime_error("Failed to open PLY file: " + path);
    }

    tinyply::PlyFile ply;
    ply.parse_header(ifs);

    bool has_vertex_colors = false;
    bool has_vertex_alpha = false;
    tinyply::Type vertex_color_type = tinyply::Type::UINT8;
    for (const auto &el : ply.get_elements()) {
        if (el.name == "vertex") {
            for (const auto &prop : el.properties) {
                if (prop.name == "red" || prop.name == "green" || prop.name == "blue") {
                    if (!has_vertex_colors) vertex_color_type = prop.propertyType;
                    has_vertex_colors = true;
                } else if (prop.name == "alpha") {
                    has_vertex_alpha = true;
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
    std::shared_ptr<tinyply::PlyData> alpha_data;
    if (has_vertex_alpha) {
        // Requested separately from red/green/blue: tinyply requires all
        // properties of one request to share the same type, and RGBA files
        // commonly mix 8-bit color with float alpha.
        alpha_data = ply.request_properties_from_element("vertex", {"alpha"});
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
                    alpha_data ? read_alpha(*alpha_data, i) : 1.0f);
            }
        } else if (vertex_color_type == tinyply::Type::FLOAT32) {
            const float *cptr = reinterpret_cast<const float *>(colors_data->buffer.get_const());
            for (size_t i = 0; i < num_vertices; ++i) {
                mesh.colors[i] = Color(cptr[i * 3], cptr[i * 3 + 1], cptr[i * 3 + 2],
                                       alpha_data ? read_alpha(*alpha_data, i) : 1.0f);
            }
        } else if (alpha_data) {
            // No usable RGB type, but the file does carry alpha: keep the
            // transparency information rather than dropping it silently.
            for (size_t i = 0; i < num_vertices; ++i) {
                mesh.colors[i] = Color(1.0f, 1.0f, 1.0f, read_alpha(*alpha_data, i));
            }
        }
    } else if (alpha_data) {
        // Alpha without any color: white vertices with the file's alpha.
        mesh.colors.resize(num_vertices);
        for (size_t i = 0; i < num_vertices; ++i) {
            mesh.colors[i] = Color(1.0f, 1.0f, 1.0f, read_alpha(*alpha_data, i));
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

    // Keep Mesh::has_transparency in sync with the colors we just read.
    mesh.update_transparency();

    return mesh;
}

} // namespace ply_io
} // namespace scimesh
