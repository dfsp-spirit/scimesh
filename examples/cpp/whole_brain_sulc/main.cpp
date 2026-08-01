/// Demo program that loads both brain hemispheres from FreeSurfer files,
/// maps sulcal depth to vertex colors using the viridis colormap, masks
/// out non-cortex (medial wall) vertices, and renders the whole brain
/// scene to an image using the scimesh software renderer.
///
/// This demonstrates the full pipeline: FreeSurfer mesh loading via libfs,
/// per-vertex data coloring with the scimesh colormap API (including
/// winsorizing and NaN handling), and headless rendering with scimesh.
///
/// Key features shown:
///   - Loading FreeSurfer surface + sulc data + cortex label
///   - Masking medial wall vertices (set to NaN)
///   - apply_colormap() with global (pooled) range across both hemispheres
///   - Winsorizing to clip outliers
///   - ColorMap::from_uint8_colors() for scibar/libfs interop
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
#include "colormap.h"

#include <string>
#include <iostream>
#include <vector>
#include <cmath>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;
using scimesh::CropContentDirection;
using scimesh::convert_fs_mesh;
using scimesh::apply_colormap;
using scimesh::ColorMap;

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

    std::string lh_surf  = surf_dir  + "/lh.white";
    std::string rh_surf  = surf_dir  + "/rh.white";
    std::string lh_sulc  = surf_dir  + "/lh.sulc";
    std::string rh_sulc  = surf_dir  + "/rh.sulc";
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

    // ---- Phase 1: Load mesh geometry and sulc data for both hemispheres ----
    // We collect scimesh meshes (geometry only) and sulc vectors separately,
    // then apply the colormap to both datasets with a pooled range.

    Mesh lh_mesh, rh_mesh;
    std::vector<float> lh_sulc_data, rh_sulc_data;

    for (int hemi = 0; hemi < 2; hemi++) {
        const char *hemi_tag = (hemi == 0) ? "lh" : "rh";
        const std::string surf_file  = (hemi == 0) ? lh_surf  : rh_surf;
        const std::string sulc_file  = (hemi == 0) ? lh_sulc  : rh_sulc;
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

        // c) Read cortex label and mask medial wall vertices to NaN.
        std::cout << "  Label: " << label_file << "\n";
        fs::Label cortex_label;
        fs::read_label(&cortex_label, label_file);
        std::vector<bool> in_cortex = cortex_label.vert_in_label(nv);
        size_t n_cortex = 0, n_medial = 0;
        for (size_t i = 0; i < nv; i++) {
            if (in_cortex[i]) {
                n_cortex++;
            } else {
                sulc[i] = NAN;   // medial wall → NaN
                n_medial++;
            }
        }
        std::cout << "  Cortex label: " << n_cortex << " in cortex, "
                  << n_medial << " medial wall (→ NaN).\n";

        // d) Convert mesh geometry to scimesh (no colors yet).
        Mesh sc_mesh = convert_fs_mesh(fs_surface);

        // Store in the right slot.
        if (hemi == 0) {
            lh_mesh = std::move(sc_mesh);
            lh_sulc_data = std::move(sulc);
        } else {
            rh_mesh = std::move(sc_mesh);
            rh_sulc_data = std::move(sulc);
        }
        std::cout << "  Done with " << hemi_tag << ".\n\n";
    }

    // ---- Phase 2: Apply colormap to both hemispheres with pooled range ----
    std::cout << "=== Applying colormap ===\n";

    // Build viridis colormap (built-in, no external dependency).
    const auto& viridis_cmap = ColorMap::viridis();

    // Apply: global (pooled) range, winsorize 2nd/98th percentiles,
    // white for NaN (medial wall).
    auto result = apply_colormap({lh_sulc_data, rh_sulc_data},
                                 viridis_cmap,
                                 NAN, NAN,
                                 Color(1.0f, 1.0f, 1.0f, 1.0f), // white NaN
                                 2.0f, 98.0f,                    // winsorize
                                 true);                           // global range

    std::cout << "  Pooled data range: "
              << result.pooled_data_min << " to "
              << result.pooled_data_max << "\n";
    std::cout << "  Total NaN vertices: " << result.total_nan_count << "\n";
    if (!std::isnan(result.pooled_winsor_lo)) {
        std::cout << "  Winsor cutoffs: "
                  << result.pooled_winsor_lo << " / "
                  << result.pooled_winsor_hi << "\n";
    }

    // Assign the mapped colours to each mesh.
    lh_mesh.colors = std::move(result.per_dataset[0].colors);
    rh_mesh.colors = std::move(result.per_dataset[1].colors);

    // ---- Phase 3: Build scene and render ----
    Scene scene;
    scene.meshes.push_back(std::move(lh_mesh));
    scene.meshes.push_back(std::move(rh_mesh));

    // Camera: right-lateral-superior view.
    Vec3 view_dir(-1.0f, 0.3f, 0.4f);
    view_dir = glm::normalize(view_dir);
    Vec3 up(0.0f, 0.0f, 1.0f);

    std::cout << "\n=== Computing camera ===\n";
    Camera cam = scimesh::camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);
    std::cout << "  eye    = (" << cam.eye.x << ", " << cam.eye.y << ", "
              << cam.eye.z << ")\n";
    std::cout << "  center = (" << cam.center.x << ", " << cam.center.y
              << ", " << cam.center.z << ")\n";

    // Render options.
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

    // Render.
    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    // Trim transparent border.
    img.crop_to_content(CropContentDirection::ALL, opts.background_color);
    img.grow(20, 20, 200, 200, opts.background_color);

    // Write output.
    std::string out_ppm = "whole_brain_sulc.ppm";
    bool ok = img.write_ppm(out_ppm);
    if (ok) std::cout << "  Wrote " << out_ppm << "\n";

    ok = img.write_bmp("whole_brain_sulc.bmp");
    if (ok) std::cout << "  Wrote whole_brain_sulc.bmp\n";

    ok = img.write_png("whole_brain_sulc.png");
    if (ok) std::cout << "  Wrote whole_brain_sulc.png\n";

    std::cout << "\nDone.\n";
    return 0;
}
