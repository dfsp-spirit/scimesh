/// Demo program demonstrating semi-transparent mesh rendering.
/// Loads the lh.white (opaque) and lh.pial (semi-transparent) surfaces
/// for subject1 and renders them together in a single scene, showing
/// the inner white matter surface through the outer pial surface.
///
/// To compile (from the project root):
///   cd examples/cpp/transparency && mkdir -p build && cd build
///   cmake .. && make
///
/// Then run:
///   ./transparency_demo
///
/// Output: transparency_demo.ppm

#define LIBFS_DBG_INFO

#include "libfs.h"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"

#include <string>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Triangle;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;

static Mesh convert_fs_mesh(const fs::Mesh &fs_mesh, const Color &solid_color) {
    Mesh out;
    size_t nv = fs_mesh.num_vertices();
    out.vertices.reserve(nv);
    out.colors.reserve(nv);

    for (size_t i = 0; i < nv; i++) {
        out.vertices.push_back(Vec3(
            fs_mesh.vertices[i * 3],
            fs_mesh.vertices[i * 3 + 1],
            fs_mesh.vertices[i * 3 + 2]));
        out.colors.push_back(solid_color);
    }

    size_t nf = fs_mesh.num_faces();
    out.triangles.reserve(nf);
    for (size_t i = 0; i < nf; i++) {
        out.triangles.push_back(Triangle{
            static_cast<uint32_t>(fs_mesh.faces[i * 3]),
            static_cast<uint32_t>(fs_mesh.faces[i * 3 + 1]),
            static_cast<uint32_t>(fs_mesh.faces[i * 3 + 2])});
    }

    return out;
}


int main(int argc, char **argv) {
    std::string subjects_dir = "../../../../test_data/freesurfer/subjects_dir";
    std::string subject = "subject1";

    if (argc == 3) {
        subjects_dir = argv[1];
        subject = argv[2];
    } else if (argc != 1) {
        std::cout << "Usage: " << argv[0]
                  << " [subjects_dir subject_id]\n";
        return 1;
    }

    std::string surf_dir = subjects_dir + "/" + subject + "/surf";

    std::string white_file = surf_dir + "/lh.white";
    std::string pial_file  = surf_dir + "/lh.pial";

    for (const auto &f : {white_file, pial_file}) {
        if (!fs::util::file_exists(f)) {
            std::cerr << "ERROR: File not found: " << f << "\n";
            return 1;
        }
    }

    Scene scene;

    // ---- Load white matter surface (opaque) ----
    std::cout << "Loading white surface: " << white_file << "\n";
    fs::Mesh fs_white;
    fs::read_surf(&fs_white, white_file);
    Mesh white_mesh = convert_fs_mesh(fs_white,
        Color(0.7f, 0.7f, 0.7f, 1.0f));  // solid gray
    white_mesh.has_transparency = false;
    scene.meshes.push_back(std::move(white_mesh));
    std::cout << "  " << fs_white.num_vertices() << " vertices, "
              << fs_white.num_faces() << " faces\n";

    // ---- Load pial surface (semi-transparent) ----
    std::cout << "Loading pial surface:  " << pial_file << "\n";
    fs::Mesh fs_pial;
    fs::read_surf(&fs_pial, pial_file);
    Mesh pial_mesh = convert_fs_mesh(fs_pial,
        Color(0.9f, 0.3f, 0.2f, 0.35f));  // reddish, 35% opaque
    pial_mesh.has_transparency = true;
    scene.meshes.push_back(std::move(pial_mesh));
    std::cout << "  " << fs_pial.num_vertices() << " vertices, "
              << fs_pial.num_faces() << " faces\n";

    // ---- Camera ----
    Vec3 view_dir(-1.0f, 0.0f, 0.2f);
    view_dir = glm::normalize(view_dir);
    Vec3 up(0.0f, 0.0f, 1.0f);

    std::cout << "Computing camera...\n";
    Camera cam = scimesh::camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);

    // ---- Render ----
    RenderOptions opts;
    opts.width = 1200;
    opts.height = 900;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = false;
    opts.background_color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    opts.near_plane = 0.1f;
    opts.far_plane = glm::length(cam.eye - cam.center) * 4.0f;
    opts.specular_color = Color(0.4f, 0.4f, 0.4f, 1.0f);
    opts.shininess = 64.0f;

    std::cout << "Rendering " << opts.width << "x" << opts.height << "...\n";
    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    std::string out_ppm = "transparency_demo.ppm";
    if (img.write_ppm(out_ppm)) {
        std::cout << "  Wrote " << out_ppm << "\n";
    }
    std::string out_bmp = "transparency_demo.bmp";
    if (img.write_bmp(out_bmp)) {
        std::cout << "  Wrote " << out_bmp << "\n";
    }

    std::cout << "Done.\n";
    return 0;
}
