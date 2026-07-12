/// Demo: render the "Spot" cow mesh from Keenan Crane with its texture.
///
/// Loads the triangulated OBJ and PNG texture, builds a scimesh Mesh with
/// per-vertex UV coordinates, and renders with bilinear texture sampling.
///
/// Build:
///   cd examples/cpp/spot_cow && mkdir -p build && cd build && cmake .. && make
///
/// Run (from build/):
///   ./spot_cow
///
/// Output: spot_cow.ppm, spot_cow.bmp

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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
#include <algorithm>

using scimesh::Vec2;
using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;
using scimesh::Triangle;

static Image load_texture(const char *path) {
    int w, h, ch;
    unsigned char *data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path
                  << " (" << stbi_failure_reason() << ")\n";
        return Image();
    }
    Image tex(w, h);
    tex.pixels.assign(data, data + w * h * 4);
    stbi_image_free(data);
    std::cout << "Texture: " << w << "x" << h << "\n";
    return tex;
}

static Mesh load_spot_cow(const char *obj_path, const char *tex_path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                               obj_path, nullptr, true, false);
    if (!warn.empty()) std::cout << "Warn: " << warn << "\n";
    if (!err.empty()) std::cerr << "Error: " << err << "\n";
    if (!ok) return Mesh();

    int nv = static_cast<int>(attrib.vertices.size()) / 3;
    int nt_u = static_cast<int>(attrib.texcoords.size()) / 2;
    std::cout << "OBJ: " << nv << " verts, " << nt_u << " texcoords, "
              << shapes[0].mesh.indices.size() << " face corners\n";

    Mesh mesh;
    mesh.texture = load_texture(tex_path);

    const auto &indices = shapes[0].mesh.indices;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Triangle tri;
        for (int c = 0; c < 3; ++c) {
            const auto &idx = indices[i + c];

            Vec3 pos(attrib.vertices[idx.vertex_index * 3 + 0],
                     attrib.vertices[idx.vertex_index * 3 + 1],
                     attrib.vertices[idx.vertex_index * 3 + 2]);

            Vec2 uv(0, 0);
            if (idx.texcoord_index >= 0) {
            uv = Vec2(attrib.texcoords[idx.texcoord_index * 2 + 0],
                      1.0f - attrib.texcoords[idx.texcoord_index * 2 + 1]);
            }

            mesh.uvs.push_back(uv);
            mesh.vertices.push_back(pos);
            mesh.colors.push_back(Color(1, 1, 1, 1));

            if (c == 0) tri.v0 = static_cast<uint32_t>(mesh.vertices.size() - 1);
            if (c == 1) tri.v1 = static_cast<uint32_t>(mesh.vertices.size() - 1);
            if (c == 2) tri.v2 = static_cast<uint32_t>(mesh.vertices.size() - 1);
        }
        mesh.triangles.push_back(tri);
    }

    std::cout << "Mesh: " << mesh.vertices.size() << " verts, "
              << mesh.triangles.size() << " tris\n";
    return mesh;
}

int main() {
    const char *obj_path = "../../../../test_data/keenan_crane/spot/spot_triangulated.obj";
    const char *tex_path = "../../../../test_data/keenan_crane/spot/spot_texture.png";

    Mesh mesh = load_spot_cow(obj_path, tex_path);
    if (mesh.empty()) {
        std::cerr << "Failed to load mesh.\n";
        return 1;
    }

    Vec3 eye_dir = glm::normalize(Vec3(1.0f, 0.4f, -1.2f));
    Camera cam = scimesh::camera_fit_mesh(mesh, eye_dir,
        Vec3(0.0f, 1.0f, 0.0f), 40.0f, 1.05f);

    scimesh::Light key_light;
    key_light.position = Vec3(0.5f, 1.0f, 1.0f);
    key_light.color    = Color(1.00f, 0.97f, 0.90f);
    key_light.intensity = 1.5f;

    scimesh::Light fill_light;
    fill_light.position = Vec3(-1.0f, 0.2f, 0.5f);
    fill_light.color    = Color(0.4f, 0.5f, 0.8f);
    fill_light.intensity = 0.5f;

    scimesh::Light rim_light;
    rim_light.position = Vec3(0.0f, -0.3f, -1.0f);
    rim_light.color    = Color(0.5f, 0.5f, 0.5f);
    rim_light.intensity = 0.4f;

    RenderOptions opts;
    opts.width  = 1200;
    opts.height = 900;
    opts.background_color = Color(0.10f, 0.10f, 0.14f);
    opts.shading = ShadingMode::SMOOTH;
    opts.backface_culling = true;
    opts.ambient = 0.3f;
    opts.lights = {key_light, fill_light, rim_light};
    opts.specular_color = Color(0.2f, 0.2f, 0.2f);
    opts.shininess = 16.0f;
    opts.aa_samples = 2;
    opts.ssao_enabled = true;
    opts.ssao_radius = 12.0f;
    opts.ssao_intensity = 0.5f;

    Renderer renderer;
    Image img = renderer.render_mesh(mesh, cam, opts);

    img.write_ppm("spot_cow.ppm");
    img.write_bmp("spot_cow.bmp");
    std::cout << "Wrote spot_cow.ppm, spot_cow.bmp ("
              << img.width << "x" << img.height << ")\n";

    return 0;
}
