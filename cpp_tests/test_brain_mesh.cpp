#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "mesh.h"
#include "image.h"
#include "math_utils.h"

#include "tinyply.h"

#include <fstream>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <cmath>

using namespace scimesh;

// Load a PLY mesh (with vertex colors) into a scimesh::Mesh using tinyply.
static Mesh load_ply_mesh(const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary);
    REQUIRE(ifs.good());

    tinyply::PlyFile ply;
    ply.parse_header(ifs);

    auto vertex_elements = ply.get_elements();
    bool has_vertex_colors = false;
    for (const auto &el : vertex_elements) {
        if (el.name == "vertex") {
            for (const auto &prop : el.properties) {
                if (prop.name == "red" || prop.name == "green" || prop.name == "blue") {
                    has_vertex_colors = true;
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
        const uint8_t *cptr = reinterpret_cast<const uint8_t *>(colors_data->buffer.get_const());
        for (size_t i = 0; i < num_vertices; ++i) {
            mesh.colors[i] = Color(
                cptr[i * 3] / 255.0f,
                cptr[i * 3 + 1] / 255.0f,
                cptr[i * 3 + 2] / 255.0f,
                1.0f);
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

TEST_CASE("Render real brain hemisphere mesh from PLY") {
    const std::string ply_path = "../test_data/ply/lh_mesh_sulc_viridis.ply";
    if (!std::filesystem::exists(ply_path)) {
        WARN("Skipping brain render test: PLY file not found at " << ply_path);
        return;
    }

    auto total_start = std::chrono::high_resolution_clock::now();

    // 1. Load the mesh
    auto load_start = std::chrono::high_resolution_clock::now();
    Mesh mesh = load_ply_mesh(ply_path);
    auto load_end = std::chrono::high_resolution_clock::now();
    double load_ms = std::chrono::duration<double, std::milli>(load_end - load_start).count();

    REQUIRE(mesh.vertices.size() > 0);
    REQUIRE(mesh.triangles.size() > 0);
    REQUIRE(mesh.has_colors());

    std::cout << "Brain mesh loaded: " << mesh.vertices.size() << " vertices, "
              << mesh.triangles.size() << " triangles, "
              << mesh.colors.size() << " colors\n";
    std::cout << "Load time: " << load_ms << " ms\n";

    // 2. Compute bounding box for manual auto-framing
    Vec3 bbox_min, bbox_max;
    mesh.compute_bounding_box(bbox_min, bbox_max);
    Vec3 center = (bbox_min + bbox_max) * 0.5f;
    Vec3 extent = bbox_max - bbox_min;
    float radius = glm::length(extent) * 0.5f;

    std::cout << "Bounding box: min=(" << bbox_min.x << ", " << bbox_min.y << ", " << bbox_min.z << ")"
              << " max=(" << bbox_max.x << ", " << bbox_max.y << ", " << bbox_max.z << ")\n";
    std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")  radius=" << radius << "\n";

    // 3. Camera: lateral view (look from -X toward center in RAS)
    // Position camera at distance = radius / sin(fov/2) to fit the mesh.
    float fov_rad = glm::radians(45.0f);
    float dist = radius / std::sin(fov_rad * 0.5f);
    // Add a small margin
    dist *= 1.1f;

    Camera cam;
    cam.eye = Vec3(center.x - dist, center.y, center.z);
    cam.center = center;
    cam.up = Vec3(0, 0, 1); // RAS: Z is up (superior)
    cam.fov_degrees = 45.0f;

    RenderOptions opts;
    opts.width = 640;
    opts.height = 480;
    opts.background_color = Color(1, 1, 1, 1); // white
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = false; // brain surfaces are open (single surface)
    opts.invert_normals = false;
    opts.near_plane = 0.1f;
    opts.far_plane = dist * 4.0f;

    std::cout << "Camera: eye=(" << cam.eye.x << ", " << cam.eye.y << ", " << cam.eye.z << ")"
              << " center=(" << center.x << ", " << center.y << ", " << center.z << ")\n";
    std::cout << "Render: " << opts.width << "x" << opts.height << "\n";

    // 4. Render and time it
    Renderer renderer;
    auto render_start = std::chrono::high_resolution_clock::now();
    Image img = renderer.render_mesh(mesh, cam, opts);
    auto render_end = std::chrono::high_resolution_clock::now();
    double render_ms = std::chrono::duration<double, std::milli>(render_end - render_start).count();

    std::cout << "Render time: " << render_ms << " ms\n";

    // 5. Verify the image has content
    int colored_pixels = 0;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            uint8_t r, g, b, a;
            img.get_pixel(x, y, r, g, b, a);
            if (r < 250 || g < 250 || b < 250)
                colored_pixels++;
        }
    }
    std::cout << "Colored pixels: " << colored_pixels << " ("
              << (100.0 * colored_pixels) / (opts.width * opts.height) << "%)\n";

    REQUIRE(colored_pixels > 1000);

    // 6. Write image for visual inspection
    std::filesystem::create_directories("reference_images");
    bool ok = img.write_ppm("reference_images/brain_lh_lateral.ppm");
    REQUIRE(ok);
    std::cout << "Image written to reference_images/brain_lh_lateral.ppm\n";

    auto total_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();
    std::cout << "Total test time: " << total_ms << " ms\n";
}