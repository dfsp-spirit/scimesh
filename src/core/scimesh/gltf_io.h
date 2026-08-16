/// @file gltf_io.h
/// @brief Write scenes to glTF 2.0 (.gltf + .bin) or binary glTF (.glb).
///
/// This is a one-way **export**: after a scene is written to a glTF file, the
/// browser/GPU side is out of scimesh's scope.  The library stays a CPU mesh
/// renderer and simply emits a standard, shareable scene file.
///
/// ## What is exported
///
/// - Geometry: vertex positions (FLOAT32), normals (FLOAT32) when present,
///   indices (UINT32).
/// - Vertex colors mapped to the glTF `COLOR_0` attribute (normalized UNSIGNED
///   BYTE RGBA).  Per-face colors have no direct glTF equivalent, so they are
///   exported by **splitting vertices** (each triangle gets its own three
///   vertices), increasing geometry ~3x.
/// - Per-mesh placement transforms as node `matrix` (column-major, matching
///   what the renderer applies) and node `name`s.
/// - An optional perspective camera (when a `Camera*` is supplied).
///
/// Renderer-specific settings (shading mode, fog, SSAO, ...) are deliberately
/// NOT exported — they are not part of the glTF standard.
///
/// ## Usage
///
/// @code{.cpp}
/// Scene scene;
/// scene.add(sphere);
/// scene.add("cube", cube, glm::translate(Mat4(1.0f), Vec3(2, 0, 0)));
/// gltf_io::write_gltf("out/model.gltf", scene);   // .gltf + .bin
/// gltf_io::write_glb("out/model.glb", scene);     // single binary file
/// @endcode
///
/// @see Scene, Scene::add(), write_stl()
#pragma once

#include <scimesh/scene.h>
#include <scimesh/camera.h>
#include <scimesh/mesh.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace scimesh {
namespace gltf_io {

namespace detail {

inline std::string json_escape(const std::string &s) {
    std::ostringstream o;
    for (char ch : s) {
        switch (ch) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n";  break;
            case '\r': o << "\\r";  break;
            case '\t': o << "\\t";  break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                      << static_cast<int>(ch) << std::dec;
                } else {
                    o << ch;
                }
        }
    }
    return o.str();
}

/// @brief Print a float compactly with enough digits to round-trip a float32.
inline std::string fmt(float v) {
    std::ostringstream o;
    o << std::setprecision(9) << static_cast<double>(v);
    return o.str();
}

inline std::string join_impl(const std::vector<std::string> &v);

/// @brief One mesh, packed into glTF-ready flat arrays.
struct PackedMesh {
    std::vector<float> positions;    // x,y,z per vertex
    std::vector<float> normals;      // x,y,z per vertex (empty if absent)
    std::vector<uint8_t> colors;     // r,g,b,a per vertex (empty if absent)
    std::vector<uint32_t> indices;   // 3 per triangle
    bool has_colors = false;         // whether a COLOR_0 attribute is present
    float base_r = 1.0f, base_g = 1.0f, base_b = 1.0f, base_a = 1.0f;  // material base color
};

inline void append_vertex(PackedMesh &pm, const Vec3 &v,
                          const std::vector<Vec3> *normals, uint32_t idx,
                          const Color *color) {
    pm.positions.push_back(v.x);
    pm.positions.push_back(v.y);
    pm.positions.push_back(v.z);
    if (normals != nullptr && !normals->empty()) {
        const Vec3 &n = (*normals)[idx];
        pm.normals.push_back(n.x);
        pm.normals.push_back(n.y);
        pm.normals.push_back(n.z);
    }
    if (color != nullptr) {
        pm.colors.push_back(static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, color->r)) * 255.0f + 0.5f));
        pm.colors.push_back(static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, color->g)) * 255.0f + 0.5f));
        pm.colors.push_back(static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, color->b)) * 255.0f + 0.5f));
        pm.colors.push_back(static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, color->a)) * 255.0f + 0.5f));
    }
}

inline PackedMesh pack_mesh(const Mesh &mesh) {
    PackedMesh pm;
    pm.base_r = mesh.default_color.r;
    pm.base_g = mesh.default_color.g;
    pm.base_b = mesh.default_color.b;
    pm.base_a = mesh.default_color.a;

    const std::vector<Vec3> *normals = nullptr;
    if (mesh.has_normals()) normals = &mesh.normals;

    if (mesh.has_face_colors()) {
        // glTF colors are per-vertex; split vertices so each face carries its
        // own color (matching scimesh's flat face-coloring).
        pm.has_colors = true;
        for (size_t t = 0; t < mesh.triangles.size(); ++t) {
            const Triangle &tri = mesh.triangles[t];
            const Color &fc = mesh.face_colors[t];
            uint32_t base = static_cast<uint32_t>(pm.positions.size() / 3);
            const uint32_t idxs[3] = {tri.v0, tri.v1, tri.v2};
            for (int k = 0; k < 3; ++k) {
                append_vertex(pm, mesh.vertices[idxs[k]], normals, idxs[k], &fc);
                pm.indices.push_back(base + static_cast<uint32_t>(k));
            }
        }
    } else {
        pm.has_colors = mesh.has_colors();
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const Color *c = nullptr;
            if (pm.has_colors) c = &mesh.colors[i];
            append_vertex(pm, mesh.vertices[i], normals, static_cast<uint32_t>(i), c);
        }
        for (const Triangle &tri : mesh.triangles) {
            pm.indices.push_back(tri.v0);
            pm.indices.push_back(tri.v1);
            pm.indices.push_back(tri.v2);
        }
    }
    return pm;
}

/// @brief Serialized glTF scene: the JSON document plus the binary buffer.
struct GltfOutput {
    std::string json;
    std::vector<uint8_t> bin;
    bool has_camera = false;
};

inline void pad_bin(std::vector<uint8_t> &bin, size_t align = 4) {
    while (bin.size() % align != 0) bin.push_back(0);
}

inline GltfOutput build(const Scene &scene, const Camera *camera) {
    std::vector<PackedMesh> meshes;
    meshes.reserve(scene.meshes.size());
    for (size_t i = 0; i < scene.meshes.size(); ++i) {
        if (!scene.meshes[i].empty())
            meshes.push_back(pack_mesh(scene.meshes[i]));
    }

    GltfOutput out;
    std::vector<uint8_t> &bin = out.bin;

    // ---- pack the binary buffer (positions, normals, colors, indices) ----
    std::vector<size_t> pos_off(meshes.size()), nor_off(meshes.size()),
                        col_off(meshes.size()), idx_off(meshes.size());
    std::vector<size_t> pos_cnt(meshes.size()), idx_cnt(meshes.size());
    for (size_t i = 0; i < meshes.size(); ++i) {
        const PackedMesh &m = meshes[i];
        pad_bin(bin);
        pos_off[i] = bin.size();
        for (float f : m.positions) { const uint8_t *p = reinterpret_cast<const uint8_t *>(&f); bin.insert(bin.end(), p, p + 4); }
        pos_cnt[i] = m.positions.size() / 3;

        if (!m.normals.empty()) {
            pad_bin(bin);
            nor_off[i] = bin.size();
            for (float f : m.normals) { const uint8_t *p = reinterpret_cast<const uint8_t *>(&f); bin.insert(bin.end(), p, p + 4); }
        }
        if (!m.colors.empty()) {
            pad_bin(bin);
            col_off[i] = bin.size();
            bin.insert(bin.end(), m.colors.begin(), m.colors.end());
        }
        pad_bin(bin);
        idx_off[i] = bin.size();
        for (uint32_t v : m.indices) {
            const uint8_t *p = reinterpret_cast<const uint8_t *>(&v);
            bin.insert(bin.end(), p, p + 4);
        }
        idx_cnt[i] = m.indices.size();
    }

    // ---- build the JSON document ----
    std::ostringstream j;

    j << "{\n";
    j << "  \"asset\": {\"version\": \"2.0\", \"generator\": \"scimesh\"},\n";

    // buffers
    j << "  \"buffers\": [{\"byteLength\": " << bin.size();
    j << "}],\n";  // uri filled in later for .gltf

    // bufferViews + accessors per mesh
    int next_accessor = 0;
    std::vector<std::string> accessors_json;
    std::vector<std::string> views_json;
    std::vector<std::vector<int>> mesh_accessors;   // {POS, NOR, COL, IDX} per mesh
    for (size_t i = 0; i < meshes.size(); ++i) {
        const PackedMesh &m = meshes[i];
        std::vector<int> acc;
        // POSITION
        {
            float mn[3] = {0,0,0}, mx[3] = {0,0,0};
            for (size_t k = 0; k < m.positions.size(); k += 3) {
                for (int d = 0; d < 3; ++d) {
                    float v = m.positions[k + d];
                    if (k == 0 || v < mn[d]) mn[d] = v;
                    if (k == 0 || v > mx[d]) mx[d] = v;
                }
            }
            views_json.push_back("{\"buffer\": 0, \"byteOffset\": " + std::to_string(pos_off[i]) +
                                 ", \"byteLength\": " + std::to_string(m.positions.size() * 4) + "}");
            accessors_json.push_back(
                "{\"bufferView\": " + std::to_string(views_json.size() - 1) +
                ", \"componentType\": 5126, \"count\": " + std::to_string(pos_cnt[i]) +
                ", \"type\": \"VEC3\", \"min\": [" + fmt(mn[0]) + ", " + fmt(mn[1]) + ", " + fmt(mn[2]) +
                "], \"max\": [" + fmt(mx[0]) + ", " + fmt(mx[1]) + ", " + fmt(mx[2]) + "]}");
            acc.push_back(next_accessor++);
        }
        // NORMAL
        if (!m.normals.empty()) {
            views_json.push_back("{\"buffer\": 0, \"byteOffset\": " + std::to_string(nor_off[i]) +
                                 ", \"byteLength\": " + std::to_string(m.normals.size() * 4) + "}");
            accessors_json.push_back(
                "{\"bufferView\": " + std::to_string(views_json.size() - 1) +
                ", \"componentType\": 5126, \"count\": " + std::to_string(pos_cnt[i]) +
                ", \"type\": \"VEC3\"}");
            acc.push_back(next_accessor++);
        } else {
            acc.push_back(-1);
        }
        // COLOR_0
        if (!m.colors.empty()) {
            views_json.push_back("{\"buffer\": 0, \"byteOffset\": " + std::to_string(col_off[i]) +
                                 ", \"byteLength\": " + std::to_string(m.colors.size()) + "}");
            accessors_json.push_back(
                "{\"bufferView\": " + std::to_string(views_json.size() - 1) +
                ", \"componentType\": 5121, \"count\": " + std::to_string(pos_cnt[i]) +
                ", \"normalized\": true, \"type\": \"VEC4\"}");
            acc.push_back(next_accessor++);
        } else {
            acc.push_back(-1);
        }
        // INDICES
        views_json.push_back("{\"buffer\": 0, \"byteOffset\": " + std::to_string(idx_off[i]) +
                             ", \"byteLength\": " + std::to_string(m.indices.size() * 4) + "}");
        accessors_json.push_back(
            "{\"bufferView\": " + std::to_string(views_json.size() - 1) +
            ", \"componentType\": 5125, \"count\": " + std::to_string(idx_cnt[i]) +
            ", \"type\": \"SCALAR\"}");
        acc.push_back(next_accessor++);
        mesh_accessors.push_back(acc);
    }

    j << "  \"bufferViews\": [\n    " << join_impl(views_json) << "\n  ],\n";
    j << "  \"accessors\": [\n    " << join_impl(accessors_json) << "\n  ],\n";

    // materials (one per mesh, dedup not required for correctness)
    std::vector<std::string> materials_json;
    for (const PackedMesh &m : meshes) {
        float r = m.has_colors ? 1.0f : m.base_r;
        float g = m.has_colors ? 1.0f : m.base_g;
        float b = m.has_colors ? 1.0f : m.base_b;
        float a = m.has_colors ? 1.0f : m.base_a;
        std::ostringstream mat;
        mat << "{\"pbrMetallicRoughness\": {\"baseColorFactor\": ["
            << fmt(r) << ", " << fmt(g) << ", " << fmt(b) << ", " << fmt(a)
            << "]}}";
        materials_json.push_back(mat.str());
    }
    j << "  \"materials\": [\n    " << join_impl(materials_json) << "\n  ],\n";

    // meshes
    std::vector<std::string> meshes_json;
    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto &acc = mesh_accessors[i];
        std::ostringstream attrs;
        attrs << "\"POSITION\": " << acc[0];
        if (acc[1] >= 0) attrs << ", \"NORMAL\": " << acc[1];
        if (acc[2] >= 0) attrs << ", \"COLOR_0\": " << acc[2];
        std::ostringstream prim;
        prim << "{\"attributes\": {" << attrs.str() << "}, \"indices\": "
             << acc[3] << ", \"material\": " << i << "}";
        meshes_json.push_back("{\"primitives\": [" + prim.str() + "]}");
    }
    j << "  \"meshes\": [\n    " << join_impl(meshes_json) << "\n  ],\n";

    // cameras
    out.has_camera = (camera != nullptr && camera->projection == ProjectionType::PERSPECTIVE);
    if (out.has_camera) {
        float yfov = camera->fov_degrees * 3.14159265358979323846f / 180.0f;
        j << "  \"cameras\": [{\"type\": \"perspective\", \"perspective\": {\"yfov\": "
          << fmt(yfov) << ", \"znear\": 0.1}}],\n";
    }

    // nodes
    std::vector<std::string> nodes_json;
    for (size_t i = 0; i < meshes.size(); ++i) {
        std::ostringstream n;
        n << "{\"mesh\": " << i;
        if (!scene.name(i).empty())
            n << ", \"name\": \"" << json_escape(scene.name(i)) << "\"";
        const Mat4 &m = scene.transform(i);
        bool identity = (m == Mat4(1.0f));
        if (!identity) {
            n << ", \"matrix\": [";
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r) {
                    if (c != 0 || r != 0) n << ", ";
                    n << fmt(m[c][r]);
                }
            n << "]";
        }
        n << "}";
        nodes_json.push_back(n.str());
    }
    if (out.has_camera) {
        std::ostringstream n;
        n << "{\"camera\": 0, \"name\": \"camera\"}";
        nodes_json.push_back(n.str());
    }
    j << "  \"nodes\": [\n    " << join_impl(nodes_json) << "\n  ],\n";

    // scenes
    std::vector<std::string> node_ids;
    for (size_t i = 0; i < nodes_json.size(); ++i) node_ids.push_back(std::to_string(i));
    j << "  \"scenes\": [{\"nodes\": [" << join_impl(node_ids) << "]}],\n";
    j << "  \"scene\": 0\n";
    j << "}\n";

    out.json = j.str();
    return out;
}

inline std::string join_impl(const std::vector<std::string> &v) {
    std::ostringstream o;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o << ", ";
        o << v[i];
    }
    return o.str();
}

inline std::string basename(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

inline std::string dirname(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? std::string() : path.substr(0, pos + 1);
}

inline std::string stem(const std::string &path) {
    std::string b = basename(path);
    size_t dot = b.find_last_of('.');
    return (dot == std::string::npos) ? b : b.substr(0, dot);
}

inline void write_bytes(const std::string &path, const std::vector<uint8_t> &data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("glTF: cannot open '" + path + "' for writing");
    f.write(reinterpret_cast<const char *>(data.data()),
            static_cast<std::streamsize>(data.size()));
}

} // namespace detail

/// @brief Write a scene as glTF 2.0 (JSON document + external .bin file).
///
/// @param path   Output path for the .gltf JSON document.  The binary buffer
///               is written next to it as `<stem>.bin`.
/// @param scene  The scene to export (meshes, transforms, names).
/// @param camera Optional camera; when non-null and perspective, exported as
///               a glTF perspective camera node.
///
/// @throws std::runtime_error if the output files cannot be written.
inline void write_gltf(const std::string &path, const Scene &scene,
                       const Camera *camera = nullptr) {
    detail::GltfOutput out = detail::build(scene, camera);

    // Fill in the buffer uri: relative reference to the sibling .bin file.
    std::string bin_name = detail::stem(path) + ".bin";
    std::string uri = detail::basename(bin_name);
    std::string uri_escaped = uri;
    // The buffer entry is exactly '{"byteLength": N}' — splice in the uri.
    std::string needle = "\"byteLength\": " + std::to_string(out.bin.size()) + "}";
    std::string repl = "\"byteLength\": " + std::to_string(out.bin.size()) +
                       ", \"uri\": \"" + uri_escaped + "\"}";
    size_t pos = out.json.find(needle);
    if (pos != std::string::npos)
        out.json.replace(pos, needle.size(), repl);

    std::string bin_path = detail::dirname(path) + bin_name;
    detail::write_bytes(bin_path, out.bin);
    std::ofstream jf(path);
    if (!jf) throw std::runtime_error("glTF: cannot open '" + path + "' for writing");
    jf << out.json;
}

/// @brief Write a scene as a single binary glTF file (.glb).
///
/// @param path   Output path for the .glb file (self-contained, no .bin).
/// @param scene  The scene to export.
/// @param camera Optional camera (see write_gltf()).
///
/// @throws std::runtime_error if the file cannot be written.
inline void write_glb(const std::string &path, const Scene &scene,
                      const Camera *camera = nullptr) {
    detail::GltfOutput out = detail::build(scene, camera);

    // JSON chunk must be padded to 4 bytes with spaces (0x20).
    std::vector<uint8_t> json_chunk(out.json.begin(), out.json.end());
    while (json_chunk.size() % 4 != 0) json_chunk.push_back(0x20);
    // BIN chunk padded to 4 bytes with zeros.
    std::vector<uint8_t> bin = out.bin;
    while (bin.size() % 4 != 0) bin.push_back(0);

    const uint32_t kMagic = 0x46546C67;  // "glTF"
    const uint32_t kVersion = 2;
    const uint32_t kJsonType = 0x4E4F534A;  // "JSON"
    const uint32_t kBinType = 0x004E4942;   // "BIN\0"
    uint32_t total = 12 + 8 + static_cast<uint32_t>(json_chunk.size()) +
                     8 + static_cast<uint32_t>(bin.size());

    std::vector<uint8_t> out_bytes;
    auto put_u32 = [&out_bytes](uint32_t v) {
        out_bytes.push_back(v & 0xFF);
        out_bytes.push_back((v >> 8) & 0xFF);
        out_bytes.push_back((v >> 16) & 0xFF);
        out_bytes.push_back((v >> 24) & 0xFF);
    };
    put_u32(kMagic);
    put_u32(kVersion);
    put_u32(total);
    put_u32(static_cast<uint32_t>(json_chunk.size()));
    put_u32(kJsonType);
    out_bytes.insert(out_bytes.end(), json_chunk.begin(), json_chunk.end());
    put_u32(static_cast<uint32_t>(bin.size()));
    put_u32(kBinType);
    out_bytes.insert(out_bytes.end(), bin.begin(), bin.end());

    detail::write_bytes(path, out_bytes);
}

/// @brief Write a scene as glTF, choosing the format from the file extension.
///
/// A path ending in `.glb` writes binary glTF; any other path writes a
/// `.gltf` JSON document (+ sibling `.bin`).
inline void write(const std::string &path, const Scene &scene,
                  const Camera *camera = nullptr) {
    std::string ext;
    std::string b = detail::basename(path);
    size_t dot = b.find_last_of('.');
    if (dot != std::string::npos)
        ext = b.substr(dot + 1);
    for (auto &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == "glb")
        write_glb(path, scene, camera);
    else
        write_gltf(path, scene, camera);
}

} // namespace gltf_io
} // namespace scimesh
