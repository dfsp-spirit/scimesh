/// Demo: render a protein (PDB file) as atom spheres with CPK coloring
/// and a C-alpha backbone trace.
///
/// Uses the tiny_pdb.hpp parser to read ATOM records and maps each element
/// to a standard CPK colour.  Spheres are sized for ball-and-stick style.
///
/// Usage:
///   ./protein_demo [path/to/file.pdb]
///
/// Default: 1CRN.pdb (in the current directory).
///
/// To compile (from the project root):
///   cd examples/cpp/protein_data_bank_pdb_file && mkdir -p build && cd build
///   cmake .. && make
///
/// Output: <basename>.ppm

#include "tiny_pdb.hpp"
#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include "primitives.h"
#include "scene.h"

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <limits>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;

static Color cpk_color(const std::string &element) {
    static const std::map<std::string, Color> map = {
        {"H",  Color(1.00f, 1.00f, 1.00f)},  {"C",  Color(0.20f, 0.20f, 0.20f)},
        {"N",  Color(0.19f, 0.31f, 0.97f)},  {"O",  Color(0.94f, 0.10f, 0.07f)},
        {"S",  Color(1.00f, 0.78f, 0.20f)},  {"P",  Color(1.00f, 0.55f, 0.00f)},
        {"F",  Color(0.27f, 0.95f, 0.27f)},  {"CL", Color(0.12f, 0.94f, 0.12f)},
        {"BR", Color(0.65f, 0.16f, 0.16f)},  {"I",  Color(0.58f, 0.00f, 0.58f)},
        {"FE", Color(0.88f, 0.40f, 0.20f)},  {"NI", Color(0.35f, 0.35f, 0.35f)},
        {"CU", Color(0.20f, 0.40f, 0.63f)},  {"ZN", Color(0.52f, 0.52f, 0.52f)},
        {"MG", Color(0.54f, 1.00f, 0.00f)},  {"CA", Color(0.24f, 0.71f, 0.10f)},
        {"K",  Color(0.56f, 0.25f, 0.83f)},  {"NA", Color(0.11f, 0.25f, 0.95f)},
        {"MN", Color(0.61f, 0.48f, 0.78f)},
    };
    auto it = map.find(element);
    if (it != map.end()) return it->second;
    return Color(1.0f, 0.08f, 0.58f);
}

static float atom_radius(const std::string &element) {
    static const std::map<std::string, float> map = {
        {"H",  0.25f},  {"C",  0.50f},  {"N",  0.45f},  {"O",  0.45f},
        {"S",  0.60f},  {"P",  0.60f},  {"F",  0.35f},  {"CL", 0.55f},
        {"BR", 0.65f},  {"I",  0.75f},  {"FE", 0.55f},  {"ZN", 0.55f},
        {"MG", 0.50f},  {"CA", 0.65f},  {"NA", 0.55f},  {"K",  0.75f},
    };
    auto it = map.find(element);
    if (it != map.end()) return it->second;
    return 0.50f;
}

int main(int argc, char *argv[]) {
    std::string pdb_path = "1CRN.pdb";
    if (argc > 1) pdb_path = argv[1];

    std::ifstream check(pdb_path);
    if (!check.good()) {
        std::cerr << "Error: file not found: " << pdb_path << "\n";
        return 1;
    }
    check.close();

    auto atoms = TinyPDB::parse(pdb_path);
    if (atoms.empty()) {
        std::cerr << "Error: no atoms loaded from " << pdb_path << "\n";
        return 1;
    }
    std::cout << "Loaded " << atoms.size() << " atoms from " << pdb_path << "\n";

    std::vector<Vec3> centers;
    std::vector<float> radii;
    std::vector<Color> colors;
    std::vector<Vec3> ca_positions;
    std::vector<char> ca_chains;

    for (const auto &a : atoms) {
        centers.push_back(Vec3(a.x, a.y, a.z));
        radii.push_back(atom_radius(a.element));
        colors.push_back(cpk_color(a.element));
        if (a.name == "CA") {
            ca_positions.push_back(Vec3(a.x, a.y, a.z));
            ca_chains.push_back(a.chain);
        }
    }

    Mesh atom_mesh = scimesh::generate_multi_spheres(centers, radii, colors, 14);
    std::cout << "Atoms: " << atom_mesh.vertices.size() << " vertices, "
              << atom_mesh.triangles.size() << " triangles\n";
    std::cout << "Backbone: " << ca_positions.size() << " CA atoms\n";

    Mesh backbone_mesh;
    if (ca_positions.size() >= 2) {
        std::vector<Vec3> bstarts, bends;
        std::vector<Color> bcolors;
        std::vector<float> bradii;
        for (size_t i = 0; i + 1 < ca_positions.size(); ++i) {
            if (ca_chains[i] != ca_chains[i + 1]) continue;
            bstarts.push_back(ca_positions[i]);
            bends.push_back(ca_positions[i + 1]);
            bradii.push_back(0.12f);
            bcolors.push_back(Color(0.1f, 0.7f, 0.8f));
        }
        backbone_mesh = scimesh::generate_multi_cylinders(
            bstarts, bends, bradii, bcolors, 8);
    }

    Scene scene;
    scene.meshes.push_back(atom_mesh);
    if (!backbone_mesh.triangles.empty()) {
        scene.meshes.push_back(backbone_mesh);
    }

    Vec3 eye_dir = glm::normalize(Vec3(0.5f, 0.4f, -1.0f));
    Camera cam = scimesh::camera_fit_mesh(atom_mesh, eye_dir,
        Vec3(0.0f, 1.0f, 0.0f), 40.0f, 1.05f);

    scimesh::Light key_light;
    key_light.position = Vec3(0.5f, 1.0f, 1.0f);
    key_light.color    = Color(1.00f, 0.97f, 0.90f);
    key_light.intensity = 2.0f;

    scimesh::Light fill_light;
    fill_light.position = Vec3(-1.0f, 0.2f, 0.3f);
    fill_light.color    = Color(0.50f, 0.60f, 0.80f);
    fill_light.intensity = 0.6f;

    scimesh::Light rim_light;
    rim_light.position = Vec3(0.0f, -0.3f, -1.0f);
    rim_light.color    = Color(0.60f, 0.55f, 0.55f);
    rim_light.intensity = 0.5f;

    RenderOptions opts;
    opts.width  = 1200;
    opts.height = 900;
    opts.background_color = Color(0.12f, 0.12f, 0.15f);
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = true;
    opts.ambient = 0.28f;
    opts.lights = {key_light, fill_light, rim_light};
    opts.specular_color = Color(1.0f, 1.0f, 1.0f);
    opts.shininess = 64.0f;
    opts.aa_samples = 2;

    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    std::string out_base;
    {
        auto slash = pdb_path.find_last_of("/\\");
        auto dot   = pdb_path.find_last_of('.');
        out_base = (slash == std::string::npos)
            ? pdb_path.substr(0, dot)
            : pdb_path.substr(slash + 1, dot - slash - 1);
    }

    std::string out_ppm = out_base + ".ppm";
    img.write_ppm(out_ppm);
    std::cout << "Wrote " << out_ppm << "\n";

    std::string out_png = out_base + ".png";
    img.write_png(out_png);
    std::cout << "Wrote " << out_png << "\n";

    std::cout << "Image size: " << img.width
              << "x" << img.height << "\n";

    return 0;
}
