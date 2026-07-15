/// Demo: render a video of a camera orbiting around a whole-brain mesh.
///
/// Loads both brain hemispheres from FreeSurfer files, maps sulcal depth
/// to vertex colors, and renders 48 frames of a camera circling around
/// the Z axis (superior-inferior).  Frames are written to disk as PNG
/// images and can be assembled into a video with ffmpeg:
///
///   ffmpeg -framerate 24 -i frame_%04d.png -c:v libx264 -pix_fmt yuv420p brain.mp4
///
/// Build:
///   cd examples/cpp/brain_video && mkdir -p build && cd build
///   cmake .. -DCMAKE_BUILD_TYPE=Release && make
///
/// Run:
///   ./brain_video [subjects_dir subject_id]

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
#include <cstdio>
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
    std::string label_dir = subjects_dir + "/" + subject + "/label";

    std::string lh_surf = surf_dir + "/lh.white";
    std::string rh_surf = surf_dir + "/rh.white";
    std::string lh_sulc = surf_dir + "/lh.sulc";
    std::string rh_sulc = surf_dir + "/rh.sulc";
    std::string lh_label = label_dir + "/lh.cortex.label";
    std::string rh_label = label_dir + "/rh.cortex.label";

    for (const auto &f : {lh_surf, rh_surf, lh_sulc, rh_sulc,
                          lh_label, rh_label}) {
        if (!fs::util::file_exists(f)) {
            std::cerr << "ERROR: File not found: " << f << "\n";
            return 1;
        }
    }

    Scene scene;

    for (int hemi = 0; hemi < 2; hemi++) {
        const char *hemi_tag = (hemi == 0) ? "lh" : "rh";
        const std::string surf_file = (hemi == 0) ? lh_surf : rh_surf;
        const std::string sulc_file = (hemi == 0) ? lh_sulc : rh_sulc;
        const std::string label_file = (hemi == 0) ? lh_label : rh_label;

        std::cout << "Loading " << hemi_tag << " hemisphere...\n";

        fs::Mesh fs_surface;
        fs::read_surf(&fs_surface, surf_file);

        std::vector<float> sulc = fs::read_curv_data(sulc_file);

        fs::Label cortex_label;
        fs::read_label(&cortex_label, label_file);
        size_t nv = fs_surface.num_vertices();
        std::vector<bool> in_cortex = cortex_label.vert_in_label(nv);
        for (size_t i = 0; i < nv; i++) {
            if (!in_cortex[i]) {
                sulc[i] = NAN;
            }
        }

        std::vector<uint8_t> rgb_colors = fs::util::viridis(sulc);
        Mesh sc_mesh = convert_fs_mesh(fs_surface, sulc, rgb_colors);
        scene.meshes.push_back(std::move(sc_mesh));
    }

    Vec3 view_dir = glm::normalize(Vec3(-1.0f, 0.3f, 0.4f));
    Vec3 up(0.0f, 0.0f, 1.0f);

    Camera base_cam = scimesh::camera_fit_scene(scene, view_dir, up, 45.0f, 1.1f);

    RenderOptions opts;
    opts.width = 800;
    opts.height = 600;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = false;
    opts.background_color = Color(1.0f, 1.0f, 1.0f, 1.0f);
    opts.near_plane = 0.1f;
    opts.far_plane = glm::length(base_cam.eye - base_cam.center) * 4.0f;

    Renderer renderer;

    const int N = 48;
    const Vec3 orbit_axis(0.0f, 0.0f, 1.0f);

    std::cout << "\nRendering " << N << " frames at "
              << opts.width << "x" << opts.height << "...\n\n";

    auto t_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; i++) {
        Camera cam = scimesh::camera_orbit(base_cam, orbit_axis, 360.0f / N * i);

        Image img = renderer.render_scene(scene, cam, opts);

        char fname[64];
        std::snprintf(fname, sizeof(fname), "frame_%04d.png", i);
        img.write_png(fname);

        std::cout << "  [" << (i + 1) << "/" << N << "] " << fname << "\n";
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double t_sec = std::chrono::duration<double>(t_end - t_start).count();
    std::cout << "\nDone. " << N << " frames in " << t_sec << " s";
    std::cout << " (" << t_sec / N << " s/frame).\n";
    std::cout << "\nTo create a video:\n";
    std::cout << "  ffmpeg -framerate 24 -i frame_%04d.png"
              << " -c:v libx264 -pix_fmt yuv420p brain.mp4\n";

    return 0;
}
