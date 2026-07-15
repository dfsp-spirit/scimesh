/// Demo program that loads both brain hemispheres from FreeSurfer files,
/// maps sulcal depth to vertex colors using the viridis colormap, masks
/// out non-cortex (medial wall) vertices, and renders the whole brain
/// scene to an image using the scimesh software renderer.
///
/// This demonstrates the full pipeline: FreeSurfer mesh loading via libfs,
/// per-vertex data coloring with cortex masking, and headless rendering
/// with scimesh.
///
/// To compile (from the project root):
///   cd examples/cpp/whole_brain_sulc && mkdir -p build && cd build
///   cmake .. && make
///
/// Then run:
///   ./whole_brain_sulc
///
/// Or override paths:
///   ./whole_brain_sulc /path/to/subjects_dir subject1
///
/// Output: whole_brain_sulc.ppm (PPM image, open with any image viewer)

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
using scimesh::CropContentDirection;
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
    std::string lh_sulc = surf_dir + "/lh.sulc";
    std::string rh_sulc = surf_dir + "/rh.sulc";
    std::string lh_label = label_dir + "/lh.cortex.label";
    std::string rh_label = label_dir + "/rh.cortex.label";

    std::cout << "Subjects dir : " << subjects_dir << "\n";
    std::cout << "Subject      : " << subject << "\n\n";

    // Check that all input files exist.
    for (const auto &f : {lh_surf, rh_surf, lh_sulc, rh_sulc,
                          lh_label, rh_label}) {
        if (!fs::util::file_exists(f)) {
            std::cerr << "ERROR: File not found: " << f << "\n";
            std::cerr << "Run from the examples/cpp/whole_brain_sulc"
                         " directory, or pass paths as arguments.\n";
            return 1;
        }
    }

    Scene scene;

    // ---- Process each hemisphere ----
    for (int hemi = 0; hemi < 2; hemi++) {
        const char *hemi_tag = (hemi == 0) ? "lh" : "rh";
        const std::string surf_file = (hemi == 0) ? lh_surf : rh_surf;
        const std::string sulc_file = (hemi == 0) ? lh_sulc : rh_sulc;
        const std::string label_file = (hemi == 0) ? lh_label : rh_label;

        std::cout << "=== Loading " << hemi_tag << " hemisphere ===\n";

        // a) Read surface mesh.
        std::cout << "  Surface: " << surf_file << "\n";
        fs::Mesh fs_surface;
        fs::read_surf(&fs_surface, surf_file);
        size_t nv = fs_surface.num_vertices();
        size_t nf = fs_surface.num_faces();
        std::cout << "  Loaded " << nv << " vertices, " << nf
                  << " faces.\n";

        // b) Read sulcal depth data.
        std::cout << "  Sulc data: " << sulc_file << "\n";
        std::vector<float> sulc = fs::read_curv_data(sulc_file);
        if (sulc.size() != nv) {
            std::cerr << "  ERROR: sulc count (" << sulc.size()
                      << ") != vertex count (" << nv << ")\n";
            return 1;
        }
        {
            float mn = NAN, mx = NAN;
            bool have = false;
            for (auto v : sulc) {
                if (std::isnan(v)) continue;
                if (!have) { mn = mx = v; have = true; }
                else { if (v < mn) mn = v; if (v > mx) mx = v; }
            }
            std::cout << "  Sulc range: " << mn << " to " << mx << "\n";
        }

        // c) Read cortex label for masking.
        std::cout << "  Label: " << label_file << "\n";
        fs::Label cortex_label;
        fs::read_label(&cortex_label, label_file);
        size_t n_labeled = cortex_label.num_entries();
        std::vector<bool> in_cortex = cortex_label.vert_in_label(nv);
        size_t n_cortex = 0, n_medial = 0;
        for (size_t i = 0; i < nv; i++) {
            if (in_cortex[i]) n_cortex++; else n_medial++;
        }
        std::cout << "  Cortex label: " << n_labeled << " labeled vertices, "
                  << n_cortex << " in cortex, " << n_medial << " medial wall.\n";

        // d) Mask non-cortex: set sulc to NaN for medial wall vertices.
        for (size_t i = 0; i < nv; i++) {
            if (!in_cortex[i]) {
                sulc[i] = NAN;
            }
        }

        // e) Map sulcal depth to RGB colors (viridis, NaN -> white).
        std::cout << "  Mapping sulc -> viridis colors...\n";
        std::vector<uint8_t> rgb_colors = fs::util::viridis(sulc);

        // f) Convert to scimesh::Mesh.
        Mesh sc_mesh = convert_fs_mesh(fs_surface, sulc, rgb_colors);
        scene.meshes.push_back(std::move(sc_mesh));

        std::cout << "  Done with " << hemi_tag << ".\n\n";
    }

    // ---- Camera: auto-frame the whole scene ----
    // Use a right-lateral-superior view that shows both hemispheres well.
    Vec3 view_dir(-1.0f, 0.3f, 0.4f);
    view_dir = glm::normalize(view_dir);
    Vec3 up(0.0f, 0.0f, 1.0f);

    std::cout << "=== Computing camera ===\n";
    Camera cam = scimesh::camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);
    std::cout << "  eye    = (" << cam.eye.x << ", " << cam.eye.y << ", "
              << cam.eye.z << ")\n";
    std::cout << "  center = (" << cam.center.x << ", " << cam.center.y
              << ", " << cam.center.z << ")\n";

    // ---- Render options ----
    RenderOptions opts;
    opts.width = 1200;
    opts.height = 900;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = false;  // Brain surfaces are open, not closed.
    opts.background_color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    opts.near_plane = 0.1f;
    opts.far_plane = glm::length(cam.eye - cam.center) * 4.0f;

    std::cout << "  Rendering at " << opts.width << "x" << opts.height
              << "...\n";

    // ---- Render ----
    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    img.crop_to_content(CropContentDirection::ALL, opts.background_color);
    img.grow(20, 20, 200, 200, opts.background_color);

    // ---- Write output ----
    std::string out_ppm = "whole_brain_sulc.ppm";
    bool ok = img.write_ppm(out_ppm);
    if (ok) {
        std::cout << "  Wrote " << out_ppm << "\n";
    }

    std::string out_bmp = "whole_brain_sulc.bmp";
    ok = img.write_bmp(out_bmp);
    if (ok) {
        std::cout << "  Wrote " << out_bmp << "\n";
    }
    ok = img.write_png("whole_brain_sulc.png");
    if (ok) {
        std::cout << "  Wrote whole_brain_sulc.png\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}
