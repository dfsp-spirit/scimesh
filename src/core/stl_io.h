/// @file stl_io.h
/// @brief Read and write STL files (.stl).
///
/// STL (STereoLithography) is the standard format for 3D printing.  It
/// stores only triangle geometry — no colors, normals, or materials.
/// Both ASCII and binary STL files are supported for reading; writing
/// supports both formats as well.

#pragma once

#include "mesh.h"
#include "stl_reader.h"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <string>

namespace scimesh {
/// @brief Functions for reading and writing STL files.
namespace stl_io {

/// @brief Load a mesh from an STL file (ASCII or binary).
///
/// STL files contain only raw triangle geometry — each triangle is stored
/// independently with its own vertices (no indexed face set sharing).
/// Colors and normals are **not** stored in STL files.
///
/// @param path Filesystem path to the `.stl` file.
/// @return A Mesh with vertices and triangles populated.
///
/// @throws std::runtime_error if the file cannot be opened or parsed.
///
/// @par Example
/// @code{.cpp}
/// Mesh part = scimesh::stl_io::read_stl("bracket.stl");
/// // STL has no colors — set a uniform color for rendering
/// part.default_color = Color(0.5f, 0.5f, 0.5f);
/// @endcode
///
/// @par Note on colors
/// STL files are geometry-only.  After loading, set `mesh.default_color`
/// or populate `mesh.colors` manually for rendering.
///
/// @see write_stl(), read_obj(), read_ply()
inline Mesh read_stl(const std::string &path) {
    std::vector<float> coords, normals;
    std::vector<unsigned int> tris, solids;

    try {
        stl_reader::ReadStlFile(path.c_str(), coords, normals, tris, solids);
    } catch (const std::exception &e) {
        throw std::runtime_error(std::string("Failed to read STL file '") +
                                 path + "': " + e.what());
    }

    Mesh mesh;

    int nv = static_cast<int>(coords.size()) / 3;
    for (int i = 0; i < nv; i++) {
        mesh.vertices.push_back(Vec3(
            coords[i * 3 + 0], coords[i * 3 + 1], coords[i * 3 + 2]));
    }

    int nt = static_cast<int>(tris.size()) / 3;
    for (int i = 0; i < nt; i++) {
        mesh.triangles.push_back(Triangle{
            tris[i * 3 + 0], tris[i * 3 + 1], tris[i * 3 + 2]});
    }

    int nn = static_cast<int>(normals.size()) / 3;
    for (int i = 0; i < nn; i++) {
        mesh.normals.push_back(Vec3(
            normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]));
    }

    return mesh;
}

/// @brief Write a mesh to a binary STL file.
///
/// Binary STL is more compact than ASCII.  The file begins with an 80-byte
/// header followed by a triangle count and per-triangle data.
///
/// @param path  Output file path.
/// @param mesh  The mesh to write.
///
/// @throws std::runtime_error if the file cannot be created.
///
/// @see write_stl_ascii(), write_stl()
inline void write_stl_binary(const std::string &path, const Mesh &mesh) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    char header[80] = {};
    std::memcpy(header, "scimesh STL export", 18);
    out.write(header, 80);

    uint32_t ntri = static_cast<uint32_t>(mesh.triangles.size());
    out.write(reinterpret_cast<const char *>(&ntri), 4);

    for (const auto &tri : mesh.triangles) {
        Vec3 v0 = mesh.vertices[tri.v0];
        Vec3 v1 = mesh.vertices[tri.v1];
        Vec3 v2 = mesh.vertices[tri.v2];

        float nx = 0.0f, ny = 0.0f, nz = 1.0f;
        if (mesh.has_normals() && tri.v0 < mesh.normals.size()) {
            nx = mesh.normals[tri.v0].x;
            ny = mesh.normals[tri.v0].y;
            nz = mesh.normals[tri.v0].z;
        }

        float normal[3] = {nx, ny, nz};
        out.write(reinterpret_cast<const char *>(normal), 12);

        float vert_data[9] = {
            v0.x, v0.y, v0.z,
            v1.x, v1.y, v1.z,
            v2.x, v2.y, v2.z};
        out.write(reinterpret_cast<const char *>(vert_data), 36);

        uint16_t attr = 0;
        out.write(reinterpret_cast<const char *>(&attr), 2);
    }
}

/// @brief Write a mesh to an ASCII STL file.
///
/// ASCII STL is human-readable (you can open it in a text editor)
/// but much larger than binary STL.
///
/// @param path  Output file path.
/// @param mesh  The mesh to write.
///
/// @throws std::runtime_error if the file cannot be created.
///
/// @see write_stl_binary(), write_stl()
inline void write_stl_ascii(const std::string &path, const Mesh &mesh) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    out << "solid scimesh\n";
    for (const auto &tri : mesh.triangles) {
        Vec3 v0 = mesh.vertices[tri.v0];
        Vec3 v1 = mesh.vertices[tri.v1];
        Vec3 v2 = mesh.vertices[tri.v2];

        out << "  facet normal 0 0 1\n";
        out << "    outer loop\n";
        out << "      vertex " << v0.x << ' ' << v0.y << ' ' << v0.z << '\n';
        out << "      vertex " << v1.x << ' ' << v1.y << ' ' << v1.z << '\n';
        out << "      vertex " << v2.x << ' ' << v2.y << ' ' << v2.z << '\n';
        out << "    endloop\n";
        out << "  endfacet\n";
    }
    out << "endsolid scimesh\n";
}

/// @brief Write a mesh to an STL file, choosing the format.
///
/// @param path   Output file path.
/// @param mesh   The mesh to write.
/// @param format Either `"binary"` (compact, recommended) or `"ascii"`
///               (human-readable but large).
///
/// @throws std::runtime_error if the file cannot be created.
///
/// @par Example
/// @code{.cpp}
/// scimesh::stl_io::write_stl("output.stl", mesh, "binary");
/// scimesh::stl_io::write_stl("output_ascii.stl", mesh, "ascii");
/// @endcode
///
/// @see write_stl_binary(), write_stl_ascii(), read_stl()
inline void write_stl(const std::string &path, const Mesh &mesh,
                      const std::string &format) {
    if (format == "binary") {
        write_stl_binary(path, mesh);
    } else {
        write_stl_ascii(path, mesh);
    }
}

} // namespace stl_io
} // namespace scimesh
