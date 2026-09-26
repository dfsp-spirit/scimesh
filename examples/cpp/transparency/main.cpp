/// Transparency demo — per-vertex and per-mesh alpha (technical reference).
///
/// Renders three scenes and writes one PNG per scene:
///
///   1. transparency_spheres.png         a translucent sphere in front of opaque
///                                       ones, another one behind them, and a
///                                       small opaque sphere *inside* the
///                                       translucent shell
///   2. transparency_alpha_gradient.png  a row of spheres with alpha 1.0 → 0.0
///   3. transparency_brain_medialwall.png a FreeSurfer hemisphere whose medial
///                                       wall is 50 % transparent, seen through
///                                       an 80 % transparent sphere, with two
///                                       opaque "voxels" inside the brain
///
/// Scene 3 is the spatial-reference use case: translucent anatomy with opaque
/// data inside it.  No `has_transparency` flag is set anywhere - the renderer
/// derives it from the colors (see `Mesh::is_transparent()`), translucent
/// triangles are sorted back-to-front, and they are depth-tested against the
/// opaque geometry, so opaque objects behind them stay crisp.
///
/// To compile (from the project root):
///   cd examples/cpp/transparency && mkdir -p build && cd build
///   cmake .. && make
///
/// Then run (from the build directory):
///   ./transparency_demo
///   ./transparency_demo /path/to/subjects_dir subject1    # custom FS data
///
/// Output: transparency_*.png in the current directory (scene 3 is skipped if
/// the FreeSurfer data is not found).

#include "libfs.h"
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/image.h>
#include <scimesh/primitives.h>
#include <scimesh/colormap.h>
#include <scimesh/fs_mesh_converter.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;

namespace {

const Color WHITE(1.0f, 1.0f, 1.0f, 1.0f);

/// Sentinel for "no value" - the medial wall - used for gaps in morphometry.
const float NO_DATA = std::numeric_limits<float>::quiet_NaN();

RenderOptions make_options(int width, int height) {
    RenderOptions opts;
    opts.width = width;
    opts.height = height;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = false;
    opts.background_color = WHITE;
    opts.specular_color = Color(0.35f, 0.35f, 0.35f, 1.0f);
    opts.shininess = 48.0f;
    opts.near_plane = 0.1f;
    return opts;
}

/// Render a scene from a fitted camera and write it as a PNG.
/// `view_dir` is the direction from the scene centre towards the camera (see
/// `camera_fit_scene()`), not the direction the camera looks in.
bool render_and_save(Renderer &renderer, const Scene &scene, const Vec3 &view_dir,
                     const Vec3 &up, RenderOptions opts, const std::string &path) {
    Camera cam = scimesh::camera_fit_scene(scene, glm::normalize(view_dir), up,
                                           45.0f, 1.15f);
    opts.far_plane = glm::length(cam.eye - cam.center) * 4.0f;
    std::cout << "Rendering " << path << " (" << opts.width << "x"
              << opts.height << ")...\n";
    Image img = renderer.render_scene(scene, cam, opts);
    if (!img.write_png(path)) {
        std::cerr << "  ERROR: could not write " << path << "\n";
        return false;
    }
    std::cout << "  Wrote " << path << "\n";
    return true;
}

/// Scene 1: translucent spheres around opaque ones.
bool scene_spheres(Renderer &renderer) {
    std::cout << "\n=== Scene 1: translucent spheres ===\n";

    Scene scene;
    // Opaque reference sphere in the middle.
    scene.add(scimesh::generate_sphere(Vec3(0.0f, 0.0f, 0.0f), 1.0f, 48,
                                       Color(0.85f, 0.2f, 0.2f, 1.0f)));
    // 80 % transparent shell overlapping it from the front, with a small opaque
    // sphere inside: the classic "is my data where I think it is?" view.  The
    // yellow sphere must appear tinted blue because it is seen through the
    // shell, and the red sphere must stay crisp behind it.
    scene.add(scimesh::generate_sphere(Vec3(-0.6f, 0.0f, 1.4f), 1.0f, 48,
                                       Color(0.2f, 0.4f, 0.95f, 0.2f)));
    scene.add(scimesh::generate_sphere(Vec3(-0.6f, 0.0f, 1.3f), 0.28f, 32,
                                       Color(1.0f, 0.85f, 0.1f, 1.0f)));
    // 60 % transparent sphere overlapping the opaque one from *behind*: it must
    // not wash the opaque sphere out.
    scene.add(scimesh::generate_sphere(Vec3(0.6f, 0.0f, -1.4f), 1.0f, 48,
                                       Color(0.2f, 0.8f, 0.3f, 0.4f)));

    std::cout << "  meshes: " << scene.meshes.size() << ", translucent: yes\n";

    return render_and_save(renderer, scene, Vec3(-0.35f, 0.6f, 0.75f),
                           Vec3(0, 0, 1), make_options(1000, 700),
                           "transparency_spheres.png");
}

/// Scene 2: the same sphere at decreasing alpha, over an opaque backdrop.
bool scene_alpha_gradient(Renderer &renderer) {
    std::cout << "\n=== Scene 2: alpha gradient ===\n";

    Scene scene;
    // Opaque backdrop so the show-through is visible.
    scene.add(scimesh::generate_cuboid(Vec3(0.0f, 0.0f, -2.0f),
                                       Vec3(5.0f, 1.6f, 0.15f),
                                       Color(0.75f, 0.75f, 0.78f, 1.0f)));

    const float alphas[] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.05f};
    const int n = static_cast<int>(sizeof(alphas) / sizeof(alphas[0]));
    for (int i = 0; i < n; ++i) {
        const float x = (i - (n - 1) * 0.5f) * 1.05f;
        scene.add(scimesh::generate_sphere(Vec3(x, 0.0f, 0.0f), 0.62f, 40,
                                           Color(0.8f, 0.25f, 0.6f, alphas[i])));
    }

    return render_and_save(renderer, scene, Vec3(0.0f, 0.55f, 0.85f),
                           Vec3(0, 0, 1), make_options(1100, 500),
                           "transparency_alpha_gradient.png");
}

/// Scene 3: FreeSurfer hemisphere with a 50 % transparent medial wall, seen
/// through a very transparent sphere, with opaque spheres inside the brain.
bool scene_brain_medial_wall(Renderer &renderer, const std::string &subjects_dir,
                             const std::string &subject) {
    std::cout << "\n=== Scene 3: brain hemisphere, medial wall 50 % transparent "
                 "===\n";

    const std::string surf_dir = subjects_dir + "/" + subject + "/surf";
    const std::string label_dir = subjects_dir + "/" + subject + "/label";
    const std::string surface_file = surf_dir + "/lh.white";
    const std::string sulc_file = surf_dir + "/lh.sulc";
    const std::string label_file = label_dir + "/lh.cortex.label";

    for (const auto &f : {surface_file, sulc_file, label_file}) {
        if (!fs::util::file_exists(f)) {
            std::cerr << "  SKIPPED: file not found: " << f << "\n";
            std::cerr << "  (run from examples/cpp/transparency/build, or pass "
                         "a subjects_dir and subject)\n";
            return true;   // not a failure: this scene is optional
        }
    }

    fs::Mesh fs_surface;
    fs::read_surf(&fs_surface, surface_file);
    const size_t nv = fs_surface.num_vertices();
    std::vector<float> sulc = fs::read_curv_data(sulc_file);
    if (sulc.size() != nv) {
        std::cerr << "  ERROR: sulc values (" << sulc.size()
                  << ") do not match vertices (" << nv << ")\n";
        return false;
    }

    // Medial wall = everything outside the cortex label.  Marking it "no data"
    // lets the colormap and the converter treat it specially.
    fs::Label cortex_label;
    fs::read_label(&cortex_label, label_file);
    std::vector<bool> in_cortex = cortex_label.vert_in_label(static_cast<int>(nv));
    size_t n_medial = 0;
    for (size_t i = 0; i < nv; ++i) {
        if (!in_cortex[i]) {
            sulc[i] = NO_DATA;
            ++n_medial;
        }
    }
    std::cout << "  " << nv << " vertices, " << n_medial
              << " on the medial wall\n";

    // Viridis coloring of the cortex, white for the medial wall.
    auto mapped = scimesh::apply_colormap(sulc, scimesh::ColorMap::viridis(),
                                          NO_DATA, NO_DATA, WHITE, 2.0f, 98.0f);
    std::vector<uint8_t> rgb(nv * 3);
    for (size_t i = 0; i < nv; ++i) {
        rgb[i * 3 + 0] = static_cast<uint8_t>(std::clamp(mapped.colors[i].r, 0.0f, 1.0f) * 255.0f);
        rgb[i * 3 + 1] = static_cast<uint8_t>(std::clamp(mapped.colors[i].g, 0.0f, 1.0f) * 255.0f);
        rgb[i * 3 + 2] = static_cast<uint8_t>(std::clamp(mapped.colors[i].b, 0.0f, 1.0f) * 255.0f);
    }

    // nan_alpha = 0.5 → the medial wall is drawn half transparent instead of
    // opaque white, so the inside of the brain (and the voxels in it) shows
    // through it.  This also switches the mesh to the blended pass - no flag.
    Mesh brain = scimesh::convert_fs_mesh(fs_surface, sulc, rgb, 0.5f);

    Vec3 bmin, bmax;
    brain.compute_bounding_box(bmin, bmax);
    const Vec3 center = (bmin + bmax) * 0.5f;
    const float extent = glm::length(bmax - bmin);

    Scene scene;
    scene.add(brain);

    // Two opaque "activation blobs" inside the hemisphere ...
    scene.add(scimesh::generate_sphere(center + Vec3(0.0f, -0.035f * extent,
                                                     0.02f * extent),
                                       0.05f * extent, 32,
                                       Color(1.0f, 0.45f, 0.05f, 1.0f)));
    scene.add(scimesh::generate_sphere(center + Vec3(-0.02f * extent,
                                                     0.045f * extent,
                                                     -0.03f * extent),
                                       0.04f * extent, 32,
                                       Color(0.95f, 0.9f, 0.1f, 1.0f)));
    // ... and an 80 % transparent sphere between the camera and the hemisphere.
    scene.add(scimesh::generate_sphere(center + Vec3(0.30f * extent, 0.0f, 0.0f),
                                       0.22f * extent, 48,
                                       Color(0.25f, 0.5f, 0.95f, 0.2f)));

    // Medial view: the medial wall faces the midline, so this is the view in
    // which the 50 % transparent medial wall matters.  `view_dir` is the offset
    // of the camera from the scene centre, so +x puts it on the medial side.
    return render_and_save(renderer, scene, Vec3(1.0f, 0.12f, 0.12f),
                           Vec3(0, 0, 1), make_options(1000, 800),
                           "transparency_brain_medialwall.png");
}

} // namespace

int main(int argc, char **argv) {
    std::string subjects_dir = "../../../../test_data/freesurfer/subjects_dir";
    std::string subject = "subject1";

    if (argc == 3) {
        subjects_dir = argv[1];
        subject = argv[2];
    } else if (argc != 1) {
        std::cout << "Usage: " << argv[0] << " [subjects_dir subject_id]\n";
        return 1;
    }

    Renderer renderer;
    int failures = 0;

    if (!scene_spheres(renderer)) ++failures;
    if (!scene_alpha_gradient(renderer)) ++failures;
    if (!scene_brain_medial_wall(renderer, subjects_dir, subject)) ++failures;

    std::cout << "\n";
    if (failures == 0) {
        std::cout << "Done.\n";
        return 0;
    }
    std::cerr << failures << " scene(s) failed.\n";
    return 1;
}
