/// Demo: render the Stanford Bunny at high quality with all major features.
///
/// Loads the classic Stanford 3D Scanning Repository bunny mesh and renders
/// it with smooth shading, multi-light Blinn-Phong illumination, screen-space
/// ambient occlusion, and 4x anti-aliasing.
///
/// Build:
///   cd examples/cpp/bunny && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make
///
/// Run (from build/):
///   ./bunny
///
/// Output: bunny.png (1980x1020)

#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include "normals.h"
#include "ply_io.h"
#include "types.h"

#include <iostream>
#include <cmath>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;

int main() {
    const char *ply_path = "../../../../test_data/stanford_3d_scanning_repo/bunny/bun_zipper.ply";

    std::cout << "Loading " << ply_path << " ...\n";
    Mesh mesh = scimesh::ply_io::read_ply(ply_path);
    if (mesh.empty()) {
        std::cerr << "Failed to load mesh.\n";
        return 1;
    }

    std::cout << "Mesh: " << mesh.vertices.size() << " vertices, "
              << mesh.triangles.size() << " triangles\n";

    // The Stanford Bunny PLY has no vertex colours, assign a warm off-white.
    mesh.colors.assign(mesh.vertices.size(), Color(0.92f, 0.89f, 0.82f, 1.0f));

    // Compute smooth vertex normals for nice shading.
    std::vector<Vec3> normals;
    scimesh::compute_vertex_normals(mesh, normals);
    mesh.normals = normals;

    // Camera: 3/4 elevated view, from the front-right.
    Vec3 eye_dir = glm::normalize(Vec3(0.8f, 0.55f, 1.0f));
    Camera cam = scimesh::camera_fit_mesh(mesh, eye_dir,
        Vec3(0.0f, 1.0f, 0.0f), 35.0f, 1.08f);

    // Three-point lighting.
    scimesh::Light key_light;
    key_light.position = Vec3(1.0f, 1.2f, 1.0f);
    key_light.color    = Color(1.00f, 0.96f, 0.90f);
    key_light.intensity = 1.6f;

    scimesh::Light fill_light;
    fill_light.position = Vec3(-1.2f, 0.3f, 0.6f);
    fill_light.color    = Color(0.45f, 0.52f, 0.78f);
    fill_light.intensity = 0.55f;

    scimesh::Light rim_light;
    rim_light.position = Vec3(-0.2f, -0.4f, -1.2f);
    rim_light.color    = Color(0.6f, 0.6f, 0.6f);
    rim_light.intensity = 0.45f;

    // Render options: high quality.
    RenderOptions opts;
    opts.width  = 1980;
    opts.height = 1020;
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = true;
    opts.background_color = Color(0.90f, 0.91f, 0.93f, 1.0f);
    opts.ambient = 0.25f;
    opts.lights = {key_light, fill_light, rim_light};
    opts.specular_color = Color(0.30f, 0.30f, 0.30f);
    opts.shininess = 32.0f;
    opts.aa_samples = 4;
    opts.threads = 0;
    opts.ssao_enabled = false;

    std::cout << "Rendering at " << opts.width << "x" << opts.height
              << " with " << opts.aa_samples << "x AA ...\n";

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    const char *out_file = "bunny.png";
    img.write_png(out_file);
    std::cout << "Wrote " << out_file << " (" << img.width << "x" << img.height << ")\n";

    return 0;
}
