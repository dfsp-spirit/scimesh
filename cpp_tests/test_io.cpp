// Mesh file format I/O: PLY, OBJ, STL and the FreeSurfer mesh converter.
//
// These readers/writers run for every mesh a user loads or saves, so the
// malformed-input paths matter as much as the happy path.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/ply_io.h>
#include <scimesh/obj_io.h>
#include <scimesh/stl_io.h>
#include <scimesh/fs_mesh_converter.h>
#include <scimesh/mesh.h>
#include <scimesh/math_utils.h>
#include "libfs.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using Catch::Approx;

namespace {

// The test binary runs from cpp_tests/build (or cpp_tests/build-<variant>), so
// the repository's test_data/ is two levels up.  A few other prefixes are tried
// so the tests also work when the binary is started from the repository root.
std::string test_data(const std::string &relative) {
    for (const char *prefix : {"../../test_data/", "../test_data/", "test_data/"}) {
        std::string candidate = std::string(prefix) + relative;
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return "";
}

void require_valid_mesh(const Mesh &mesh) {
    REQUIRE_FALSE(mesh.vertices.empty());
    REQUIRE_FALSE(mesh.triangles.empty());
    for (const auto &v : mesh.vertices) {
        REQUIRE(std::isfinite(v.x));
        REQUIRE(std::isfinite(v.y));
        REQUIRE(std::isfinite(v.z));
    }
    for (const auto &t : mesh.triangles) {
        REQUIRE(t.v0 < mesh.vertices.size());
        REQUIRE(t.v1 < mesh.vertices.size());
        REQUIRE(t.v2 < mesh.vertices.size());
    }
}

Vec3 bounding_box_extent(const Mesh &mesh) {
    Vec3 lower = mesh.vertices[0];
    Vec3 upper = mesh.vertices[0];
    for (const auto &v : mesh.vertices) {
        lower = Vec3(std::min(lower.x, v.x), std::min(lower.y, v.y), std::min(lower.z, v.z));
        upper = Vec3(std::max(upper.x, v.x), std::max(upper.y, v.y), std::max(upper.z, v.z));
    }
    return upper - lower;
}

// Rounded, sorted triangle centroids.  Meshes that describe the same surface
// with duplicated/reordered vertices (as STL does) compare equal.
std::vector<std::string> triangle_centroids(const Mesh &mesh) {
    std::vector<std::string> centroids;
    centroids.reserve(mesh.triangles.size());
    for (const auto &t : mesh.triangles) {
        Vec3 c = (mesh.vertices[t.v0] + mesh.vertices[t.v1] + mesh.vertices[t.v2]) / 3.0f;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.2f %.2f %.2f", c.x, c.y, c.z);
        centroids.push_back(buf);
    }
    std::sort(centroids.begin(), centroids.end());
    return centroids;
}

void write_text_file(const std::string &path, const std::string &content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

bool contains_file_text(const std::string &path, const std::string &needle) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str().find(needle) != std::string::npos;
}

// Number of facets recorded in the header of a binary STL file.
uint32_t binary_file_facet_count(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    in.seekg(80);
    uint32_t count = 0;
    in.read(reinterpret_cast<char *>(&count), sizeof(count));
    return count;
}

} // namespace

// ---------------------------------------------------------------------------
//  PLY
// ---------------------------------------------------------------------------

TEST_CASE("ply_io::read_ply loads a real FreeSurfer PLY export", "[io][ply]") {
    const std::string path = test_data("ply/lh_mesh_sulc_viridis.ply");
    REQUIRE_FALSE(path.empty());

    Mesh mesh = ply_io::read_ply(path);
    require_valid_mesh(mesh);

    // A left hemisphere surface: thousands of vertices/faces ...
    REQUIRE(mesh.vertices.size() > 1000);
    REQUIRE(mesh.triangles.size() > 1000);
    // ... spanning a real-world coordinate range (not a unit primitive).
    Vec3 extent = bounding_box_extent(mesh);
    REQUIRE(extent.x > 10.0f);
    REQUIRE(extent.y > 10.0f);
    REQUIRE(extent.z > 10.0f);
}

TEST_CASE("ply_io::read_ply reports unusable input", "[io][ply][errors]") {
    REQUIRE_THROWS_AS(ply_io::read_ply("scimesh_no_such_file.ply"), std::runtime_error);

    // A file that exists but is not a PLY must not silently return an empty mesh.
    const std::string junk = "scimesh_test_junk.ply";
    write_text_file(junk, "this is not a ply file\n");
    REQUIRE_THROWS(ply_io::read_ply(junk));
    std::remove(junk.c_str());
}

// ---------------------------------------------------------------------------
//  OBJ
// ---------------------------------------------------------------------------

TEST_CASE("obj_io::read_obj loads a Blender-exported OBJ", "[io][obj]") {
    const std::string path = test_data("wavefront_obj/blender_export_lh_white.wf_obj");
    REQUIRE_FALSE(path.empty());

    Mesh mesh = obj_io::read_obj(path);
    require_valid_mesh(mesh);
    REQUIRE(mesh.vertices.size() > 1000);
    REQUIRE(mesh.triangles.size() > 1000);
}

TEST_CASE("obj_io::read_obj parses comments, vt/vn lines and quads", "[io][obj]") {
    // OBJ indexes are 1-based; the file mixes a quad (fan-triangulated) with
    // triangles, plus the line types that must be ignored.
    const std::string path = "scimesh_test_quad.obj";
    write_text_file(path,
        "# a comment line\n"
        "mtllib ignored.mtl\n"
        "o test_object\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vn 0 0 1\n"
        "usemtl ignored\n"
        "s off\n"
        "f 1 2 3 4\n");        // quad -> 2 triangles

    Mesh mesh = obj_io::read_obj(path);
    require_valid_mesh(mesh);
    REQUIRE(mesh.vertices.size() == 4);
    REQUIRE(mesh.triangles.size() == 2);

    // All four corners must be present, and the two triangles must cover the quad.
    Vec3 extent = bounding_box_extent(mesh);
    REQUIRE(extent.x == Approx(1.0f));
    REQUIRE(extent.y == Approx(1.0f));
    REQUIRE(extent.z == Approx(0.0f).margin(1e-6f));

    std::remove(path.c_str());
}

TEST_CASE("obj_io::read_obj reports unusable input", "[io][obj][errors]") {
    REQUIRE_THROWS(obj_io::read_obj("scimesh_no_such_file.obj"));
}

// ---------------------------------------------------------------------------
//  STL (read + write)
// ---------------------------------------------------------------------------

TEST_CASE("stl_io round-trips preserve the geometry (ascii and binary)",
          "[io][stl]") {
    const Mesh cube = make_unit_cube();
    const std::vector<std::string> original = triangle_centroids(cube);
    REQUIRE(cube.triangles.size() == 12);

    const std::string ascii_path = "scimesh_test_io_ascii.stl";
    const std::string binary_path = "scimesh_test_io_binary.stl";
    stl_io::write_stl_ascii(ascii_path, cube);
    stl_io::write_stl_binary(binary_path, cube);
    REQUIRE(std::filesystem::file_size(ascii_path) > 0);
    REQUIRE(std::filesystem::file_size(binary_path) > 0);

    // The two writers must produce the formats their names promise, because
    // read_stl() auto-detects them.
    {
        std::ifstream ascii_in(ascii_path);
        std::string first_token;
        ascii_in >> first_token;
        REQUIRE(first_token == "solid");
        REQUIRE(contains_file_text(ascii_path, "facet normal"));
    }
    {
        // Binary STL: 80-byte header (legacy writers still write "solid" in
        // it) followed by a uint32 facet count - that is what distinguishes it
        // from the ASCII flavour.
        REQUIRE(binary_file_facet_count(binary_path) == cube.triangles.size());
    }

    const Mesh ascii_mesh = stl_io::read_stl(ascii_path);
    const Mesh binary_mesh = stl_io::read_stl(binary_path);
    require_valid_mesh(ascii_mesh);
    require_valid_mesh(binary_mesh);
    REQUIRE(ascii_mesh.triangles.size() == cube.triangles.size());
    REQUIRE(binary_mesh.triangles.size() == cube.triangles.size());
    REQUIRE(triangle_centroids(ascii_mesh) == original);
    REQUIRE(triangle_centroids(binary_mesh) == original);

    // The reader merges identical vertices, so the cube comes back with its 8
    // distinct corners (and never with more vertices than STL facets have
    // corners).
    REQUIRE(ascii_mesh.vertices.size() == cube.vertices.size());
    REQUIRE(binary_mesh.vertices.size() == cube.vertices.size());
    REQUIRE(ascii_mesh.vertices.size() <= 3 * cube.triangles.size());

    std::remove(ascii_path.c_str());
    std::remove(binary_path.c_str());
}

TEST_CASE("stl_io::write_stl dispatches on the format string", "[io][stl]") {
    const Mesh cube = make_unit_cube();

    const std::string ascii_path = "scimesh_test_dispatch_ascii.stl";
    const std::string binary_path = "scimesh_test_dispatch_binary.stl";
    stl_io::write_stl(ascii_path, cube, "binary");
    stl_io::write_stl(binary_path, cube, "");  // anything else -> ascii

    {
        // "binary" is written as a binary file: facet count at offset 80.
        REQUIRE(binary_file_facet_count(ascii_path) == cube.triangles.size());
    }
    {
        // Anything else falls back to the ASCII writer.
        std::ifstream in(binary_path);
        std::string first_token;
        in >> first_token;
        REQUIRE(first_token == "solid");
        REQUIRE(contains_file_text(binary_path, "facet normal"));
    }
    REQUIRE(stl_io::read_stl(ascii_path).triangles.size() == cube.triangles.size());
    REQUIRE(stl_io::read_stl(binary_path).triangles.size() == cube.triangles.size());

    std::remove(ascii_path.c_str());
    std::remove(binary_path.c_str());
}

TEST_CASE("stl_io::read_stl reports unusable input", "[io][stl][errors]") {
    REQUIRE_THROWS_AS(stl_io::read_stl("scimesh_no_such_file.stl"), std::runtime_error);
}

// ---------------------------------------------------------------------------
//  FreeSurfer (via libfs)
// ---------------------------------------------------------------------------

TEST_CASE("FreeSurfer surfaces convert to scimesh meshes", "[io][freesurfer]") {
    const std::string path =
        test_data("freesurfer/subjects_dir/subject1/surf/rh.pial");
    REQUIRE_FALSE(path.empty());

    fs::Mesh fs_mesh;
    fs::read_surf(&fs_mesh, path);
    REQUIRE(fs_mesh.num_vertices() > 1000);
    REQUIRE(fs_mesh.num_faces() > 1000);

    // Basic converter.
    const Mesh plain = convert_fs_mesh(fs_mesh);
    require_valid_mesh(plain);
    REQUIRE(plain.vertices.size() == fs_mesh.num_vertices());
    REQUIRE(plain.triangles.size() == fs_mesh.num_faces());

    // Solid color overload.
    const Mesh solid = convert_fs_mesh(fs_mesh, Color(0.2f, 0.4f, 0.8f, 1.0f));
    REQUIRE(solid.colors.size() == solid.vertices.size());
    REQUIRE(solid.colors[0].b == Approx(0.8f));

    // Per-vertex RGB overload.
    std::vector<uint8_t> rgb(plain.vertices.size() * 3, 128);
    for (size_t i = 0; i < plain.vertices.size(); ++i) {
        rgb[i * 3 + 0] = 255;  // red
        rgb[i * 3 + 1] = 0;
        rgb[i * 3 + 2] = 0;
    }
    const Mesh colored = convert_fs_mesh(fs_mesh, rgb);
    REQUIRE(colored.colors.size() == colored.vertices.size());
    REQUIRE(colored.colors[0].r == Approx(1.0f));
    REQUIRE(colored.colors[0].g == Approx(0.0f));

    // Morphometry overload: NaN values mark the medial wall / unknown regions
    // and must come out white, all other vertices keep their RGB color.
    std::vector<float> morph(plain.vertices.size(), 1.5f);
    morph[0] = std::nanf("");
    const Mesh wrapped = convert_fs_mesh(fs_mesh, morph, rgb);
    REQUIRE(wrapped.colors.size() == wrapped.vertices.size());
    REQUIRE(wrapped.colors[0].r == Approx(1.0f));
    REQUIRE(wrapped.colors[0].g == Approx(1.0f));
    REQUIRE(wrapped.colors[0].b == Approx(1.0f));
    REQUIRE(wrapped.colors[1].r == Approx(1.0f));
    REQUIRE(wrapped.colors[1].g == Approx(0.0f));
}
