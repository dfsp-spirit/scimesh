/// Demo: render the Stanford Dragon at high quality.
///
/// Loads the VRIPped reconstruction from the Stanford 3D Scanning Repository
/// and renders it with smooth shading, multi-light Blinn-Phong illumination,
/// screen-space ambient occlusion, and 4x anti-aliasing.
///
/// Build:
///   cd examples/cpp/dragon && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make
///
/// Run (from build/):
///   ./dragon
///
/// Output: dragon.png (1980x1020)

#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include "normals.h"
#include "ply_io.h"
#include "types.h"

#include <iostream>
#include <chrono>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;

int main() {
    const char *ply_path = "../../../../test_data/stanford_3d_scanning_repo/dragon/dragon_vrip.ply";

    std::cout << "Loading " << ply_path << " ...\n";
    Mesh mesh = scimesh::ply_io::read_ply(ply_path);
    if (mesh.empty()) {
        std::cerr << "Failed to load mesh.\n";
        return 1;
    }

    std::cout << "Mesh: " << mesh.vertices.size() << " vertices, "
              << mesh.triangles.size() << " triangles\n";

    mesh.colors.assign(mesh.vertices.size(), Color(0.78f, 0.75f, 0.68f, 1.0f));

    std::vector<Vec3> normals;
    scimesh::compute_vertex_normals(mesh, normals);
    mesh.normals = normals;

    // Camera: slightly elevated, front-right view.
    Vec3 eye_dir = glm::normalize(Vec3(0.9f, 0.5f, 1.0f));
    Camera cam = scimesh::camera_fit_mesh(mesh, eye_dir,
        Vec3(0.0f, 1.0f, 0.0f), 40.0f, 1.01f);

    // Key light: warm, upper right.
    scimesh::Light key_light;
    key_light.position = Vec3(1.2f, 1.5f, 1.0f);
    key_light.color    = Color(1.00f, 0.95f, 0.88f);
    key_light.intensity = 1.8f;

    // Fill light: cool blue, left side.
    scimesh::Light fill_light;
    fill_light.position = Vec3(-1.5f, 0.4f, 0.5f);
    fill_light.color    = Color(0.42f, 0.50f, 0.78f);
    fill_light.intensity = 0.55f;

    // Rim light: neutral, behind and low.
    scimesh::Light rim_light;
    rim_light.position = Vec3(-0.3f, -0.5f, -1.3f);
    rim_light.color    = Color(0.55f, 0.55f, 0.55f);
    rim_light.intensity = 0.5f;

    RenderOptions opts;
    opts.width  = 1980;
    opts.height = 1020;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = true;
    opts.background_color = Color(1.0f, 1.0f, 1.0f, 0.0f);
    opts.ambient = 0.22f;
    opts.lights = {key_light, fill_light, rim_light};
    opts.specular_color = Color(0.25f, 0.25f, 0.25f);
    opts.shininess = 40.0f;
    opts.aa_samples = 4;
    opts.threads = 0;
    opts.ssao_enabled = true;
    opts.ssao_radius = 12.0f;
    opts.ssao_intensity = 0.5f;

    std::cout << "Rendering at " << opts.width << "x" << opts.height
              << " with " << opts.aa_samples << "x AA ...\n";

    Renderer renderer;
    auto t0 = std::chrono::high_resolution_clock::now();
    Image img = renderer.render_mesh(mesh, cam, opts);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    const char *out_file = "dragon.png";
    img.write_png(out_file);
    std::cout << "Wrote " << out_file << " (" << img.width << "x" << img.height << ")\n";
    std::cout << "Render time: " << elapsed << "s\n";

    return 0;
}
