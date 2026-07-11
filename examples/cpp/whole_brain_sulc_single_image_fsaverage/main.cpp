/// Demo program that loads the fsaverage template brain meshes, reads
/// subject sulcal depth data mapped to fsaverage (MGH format), masks
/// out non-cortex (medial wall) vertices, and renders the whole brain
/// scene to an image using the scimesh software renderer.
///
/// This demonstrates the full pipeline: FreeSurfer mesh loading via
/// libfs, MGH morphometry data reading with cortex masking, and
/// headless rendering with scimesh — all on the standard fsaverage
/// template.
///
/// To compile (from the project root):
///   cd examples/cpp/whole_brain_sulc_single_image_fsaverage && mkdir -p build && cd build
///   cmake .. && make
///
/// Then run:
///   ./whole_brain_sulc_fsaverage
///
/// Or override paths:
///   ./whole_brain_sulc_fsaverage /path/to/subjects_dir subject1 fsaverage
///
/// Output: whole_brain_sulc_fsaverage.ppm (PPM image, open with any image viewer)

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

static Mesh convert_fs_mesh(const fs::Mesh &fs_mesh,
                            const std::vector<float> &morph_data,
                            const std::vector<uint8_t> &rgb_colors) {
    Mesh out;
    size_t nv = fs_mesh.num_vertices();
    out.vertices.reserve(nv);
    out.colors.reserve(nv);

    for (size_t i = 0; i < nv; i++) {
        out.vertices.push_back(Vec3(
            fs_mesh.vertices[i * 3],
            fs_mesh.vertices[i * 3 + 1],
            fs_mesh.vertices[i * 3 + 2]));

        if (std::isnan(morph_data[i])) {
            out.colors.push_back(Color(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            out.colors.push_back(Color(
                rgb_colors[i * 3] / 255.0f,
                rgb_colors[i * 3 + 1] / 255.0f,
                rgb_colors[i * 3 + 2] / 255.0f,
                1.0f));
        }
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
    // ---- Default paths ----
    std::string subjects_dir = "../../../../test_data/freesurfer/subjects_dir";
    std::string data_subject = "subject1";
    std::string mesh_subject = "fsaverage";

    if (argc == 4) {
        subjects_dir = argv[1];
        data_subject = argv[2];
        mesh_subject = argv[3];
    } else if (argc != 1) {
        std::cout << "Usage: " << argv[0]
                  << " [subjects_dir data_subject mesh_subject]\n";
        std::cout << "  subjects_dir  : path to FreeSurfer SUBJECTS_DIR\n";
        std::cout << "  data_subject  : subject with MGH morphometry data\n";
        std::cout << "  mesh_subject  : subject with mesh and label files\n";
        return 1;
    }

    std::string data_surf_dir = subjects_dir + "/" + data_subject + "/surf";
    std::string mesh_surf_dir = subjects_dir + "/" + mesh_subject + "/surf";
    std::string mesh_label_dir = subjects_dir + "/" + mesh_subject + "/label";

    // Meshes and labels from fsaverage
    std::string lh_surf = mesh_surf_dir + "/lh.white";
    std::string rh_surf = mesh_surf_dir + "/rh.white";
    std::string lh_label = mesh_label_dir + "/lh.cortex.label";
    std::string rh_label = mesh_label_dir + "/rh.cortex.label";

    // Sulc data from subject1 (mapped to fsaverage, MGH format)
    std::string lh_sulc = data_surf_dir + "/lh.sulc.fwhm10.fsaverage.mgh";
    std::string rh_sulc = data_surf_dir + "/rh.sulc.fwhm10.fsaverage.mgh";

    std::cout << "Subjects dir   : " << subjects_dir << "\n";
    std::cout << "Data subject   : " << data_subject << "\n";
    std::cout << "Mesh subject   : " << mesh_subject << "\n\n";

    // Check that all input files exist.
    for (const auto &f : {lh_surf, rh_surf, lh_sulc, rh_sulc,
                          lh_label, rh_label}) {
        if (!fs::util::file_exists(f)) {
            std::cerr << "ERROR: File not found: " << f << "\n";
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

        // a) Read fsaverage surface mesh.
        std::cout << "  Surface: " << surf_file << "\n";
        fs::Mesh fs_surface;
        fs::read_surf(&fs_surface, surf_file);
        size_t nv = fs_surface.num_vertices();
        size_t nf = fs_surface.num_faces();
        std::cout << "  Loaded " << nv << " vertices, " << nf
                  << " faces.\n";

        // b) Read sulcal depth data from MGH file.
        //    MGH is a 4D volume format, but for surface-mapped data
        //    the file contains a flat 1D float array (one frame).
        std::cout << "  Sulc data (MGH): " << sulc_file << "\n";
        fs::Mgh mgh;
        fs::read_mgh(&mgh, sulc_file);
        if (mgh.data.data_mri_float.empty()) {
            std::cerr << "  ERROR: MGH file has no float data\n";
            return 1;
        }
        std::vector<float> sulc = mgh.data.data_mri_float;
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
        std::cout << "  MGH header: dtype=" << mgh.header.dtype
                  << " dims=" << mgh.header.dim1length
                  << "x" << mgh.header.dim2length
                  << "x" << mgh.header.dim3length
                  << "x" << mgh.header.dim4length << "\n";

        // c) Read fsaverage cortex label for masking.
        std::cout << "  Label: " << label_file << "\n";
        fs::Label cortex_label;
        fs::read_label(&cortex_label, label_file);
        size_t n_labeled = cortex_label.num_entries();
        std::vector<bool> in_cortex = cortex_label.vert_in_label(nv);
        size_t n_cortex = 0, n_medial = 0;
        for (size_t i = 0; i < nv; i++) {
            if (in_cortex[i]) n_cortex++; else n_medial++;
        }
        std::cout << "  Cortex label: " << n_labeled
                  << " labeled vertices, "
                  << n_cortex << " in cortex, "
                  << n_medial << " medial wall.\n";

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
    opts.backface_culling = false;
    opts.background_color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    opts.near_plane = 0.1f;
    opts.far_plane = glm::length(cam.eye - cam.center) * 4.0f;

    std::cout << "  Rendering at " << opts.width << "x" << opts.height
              << "...\n";

    // ---- Render ----
    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    // ---- Write output ----
    std::string out_ppm = "whole_brain_sulc_fsaverage.ppm";
    bool ok = img.write_ppm(out_ppm);
    if (ok) {
        std::cout << "  Wrote " << out_ppm << "\n";
    }

    std::string out_bmp = "whole_brain_sulc_fsaverage.bmp";
    ok = img.write_bmp(out_bmp);
    if (ok) {
        std::cout << "  Wrote " << out_bmp << "\n";
    }

    std::cout << "\nDone.\n";
    return 0;
}
