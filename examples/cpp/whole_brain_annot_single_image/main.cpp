/// Demo program that loads both brain hemispheres from FreeSurfer files,
/// obtains vertex colors directly from a cortical parcellation (aparc.annot),
/// and renders the whole brain scene to an image using the scimesh software
/// renderer.
///
/// Unlike the sulcal-depth example, this does NOT perform any data-to-color
/// mapping — the colors come directly from the annotation's built-in
/// colortable.  There is also no cortex masking because the annotation
/// assigns a color to every vertex (including the medial wall, which gets
/// the `unknown` region color).
///
/// To compile (from the project root):
///   cd examples/cpp/whole_brain_annot_single_image && mkdir -p build && cd build
///   cmake .. && make
///
/// Then run:
///   ./whole_brain_annot
///
/// Or override paths:
///   ./whole_brain_annot /path/to/subjects_dir subject1
///
/// Output: whole_brain_annot.ppm / whole_brain_annot.bmp

#define LIBFS_DBG_INFO

#include "libfs.h"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include "fs_mesh_converter.h"

#include <string>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

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
using scimesh::convert_fs_mesh;

int main(int argc, char **argv) {
    // ---- Default paths ----
    std::string subjects_dir = "../../../../test_data/freesurfer/subjects_dir";
    std::string subject = "subject1";

    if (argc == 3) {
        subjects_dir = argv[1];
        subject = argv[2];
    } else if (argc != 1) {
        std::cout << "Usage: " << argv[0]
                  << " [subjects_dir subject_id]\n";
        std::cout << "  subjects_dir : path to FreeSurfer SUBJECTS_DIR\n";
        std::cout << "  subject_id   : subject identifier\n";
        return 1;
    }

    std::string surf_dir = subjects_dir + "/" + subject + "/surf";
    std::string label_dir = subjects_dir + "/" + subject + "/label";

    std::string lh_surf = surf_dir + "/lh.white";
    std::string rh_surf = surf_dir + "/rh.white";
    std::string lh_annot = label_dir + "/lh.aparc.annot";
    std::string rh_annot = label_dir + "/rh.aparc.annot";

    std::cout << "Subjects dir : " << subjects_dir << "\n";
    std::cout << "Subject      : " << subject << "\n\n";

    for (const auto &f : {lh_surf, rh_surf, lh_annot, rh_annot}) {
        if (!fs::util::file_exists(f)) {
            std::cerr << "ERROR: File not found: " << f << "\n";
            std::cerr << "Run from the examples/cpp/whole_brain_annot_single_image"
                         " directory, or pass paths as arguments.\n";
            return 1;
        }
    }

    Scene scene;

    for (int hemi = 0; hemi < 2; hemi++) {
        const char *hemi_tag = (hemi == 0) ? "lh" : "rh";
        const std::string surf_file = (hemi == 0) ? lh_surf : rh_surf;
        const std::string annot_file = (hemi == 0) ? lh_annot : rh_annot;

        std::cout << "=== Loading " << hemi_tag << " hemisphere ===\n";

        // a) Read surface mesh.
        std::cout << "  Surface: " << surf_file << "\n";
        fs::Mesh fs_surface;
        fs::read_surf(&fs_surface, surf_file);
        size_t nv = fs_surface.num_vertices();
        size_t nf = fs_surface.num_faces();
        std::cout << "  Loaded " << nv << " vertices, " << nf
                  << " faces.\n";

        // b) Read annotation (parcellation).
        std::cout << "  Annotation: " << annot_file << "\n";
        fs::Annot annot;
        fs::read_annot(&annot, annot_file);
        size_t n_regions = annot.colortable.num_entries();
        std::cout << "  Loaded annotation with " << n_regions
                  << " regions for " << annot.num_vertices()
                  << " vertices.\n";

        // Print a few region names.
        size_t n_show = std::min(n_regions, size_t(5));
        std::cout << "  Regions: ";
        for (size_t i = 0; i < n_show; i++) {
            if (i > 0) std::cout << ", ";
            std::cout << annot.colortable.name[i];
        }
        if (n_regions > n_show) std::cout << ", ...";
        std::cout << "\n";

        // c) Get vertex colors directly from the annotation.
        std::vector<uint8_t> rgb_colors = annot.vertex_colors();
        std::cout << "  Obtained " << rgb_colors.size()
                  << " RGB values ("
                  << rgb_colors.size() / 3 << " vertices * 3 channels).\n";

        // d) Convert to scimesh::Mesh.
        Mesh sc_mesh = convert_fs_mesh(fs_surface, rgb_colors);
        scene.meshes.push_back(std::move(sc_mesh));

        std::cout << "  Done with " << hemi_tag << ".\n\n";
    }

    // ---- Camera: auto-frame the whole scene ----
    Vec3 view_dir(-1.0f, 0.3f, 0.4f);
    view_dir = glm::normalize(view_dir);
    Vec3 up(0.0f, 0.0f, 1.0f);

    std::cout << "=== Computing camera ===\n";
    Camera cam = scimesh::camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);
    std::cout << "  eye    = (" << cam.eye.x << ", " << cam.eye.y << ", "
              << cam.eye.z << ")\n";
    std::cout << "  center = (" << cam.center.x << ", " << cam.center.y
              << ", " << cam.center.z << ")\n";

    // ---- Render ----
    RenderOptions opts;
    opts.width = 1200;
    opts.height = 900;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = false;
    opts.background_color = Color(0.0f, 0.0f, 0.0f, 1.0f); // black BG for parcellation contrast
    opts.near_plane = 0.1f;
    opts.far_plane = glm::length(cam.eye - cam.center) * 4.0f;

    std::cout << "  Rendering at " << opts.width << "x" << opts.height
              << "...\n";

    Renderer renderer;
    auto t_start = std::chrono::high_resolution_clock::now();
    Image img = renderer.render_scene(scene, cam, opts);
    auto t_end = std::chrono::high_resolution_clock::now();
    double t_sec = std::chrono::duration<double>(t_end - t_start).count();
    std::cout << "  Render time: " << t_sec << " s\n";

    // ---- Write output ----
    std::string out_ppm = "whole_brain_annot.ppm";
    if (img.write_ppm(out_ppm)) {
        std::cout << "  Wrote " << out_ppm << "\n";
    }
    std::string out_bmp = "whole_brain_annot.bmp";
    if (img.write_bmp(out_bmp)) {
        std::cout << "  Wrote " << out_bmp << "\n";
    }
    std::string out_png = "whole_brain_annot.png";
    if (img.write_png(out_png)) {
        std::cout << "  Wrote " << out_png << "\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}
