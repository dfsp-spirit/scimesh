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

#include "libfs.h"

#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/image.h>
#include <scimesh/primitives.h>
#include <scimesh/scene.h>

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
    Image tex = Image::read_image(path);
    if (tex.width == 0) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return Image();
    }
    std::cout << "Texture: " << tex.width << "x" << tex.height << "\n";
    return tex;
}

static Mesh load_spot_cow(const char *obj_path, const char *tex_path) {
    fs::Mesh fs_mesh;
    fs::Mesh::from_obj(&fs_mesh, obj_path);

    size_t nv = fs_mesh.num_vertices();
    size_t nf = fs_mesh.num_faces();
    size_t nt = fs_mesh.vertex_texcoords.size() / 2;
    bool has_tex = fs_mesh.has_texcoords();
    std::cout << "OBJ: " << nv << " verts, " << nt << " texcoords, "
              << nf << " faces\n";

    Mesh mesh;
    mesh.texture = load_texture(tex_path);

    // Copy vertex positions (libfs deduplicates by (pos, uv, normal) so
    // each vertex has a unique texcoord — no more texture seam artifacts).
    mesh.vertices.reserve(nv);
    for (size_t i = 0; i < nv; i++) {
        mesh.vertices.push_back(Vec3(
            fs_mesh.vertices[i * 3],
            fs_mesh.vertices[i * 3 + 1],
            fs_mesh.vertices[i * 3 + 2]));
    }

    // Copy texture coordinates (flip V: OBJ origin bottom-left → top-left).
    mesh.uvs.reserve(nv);
    if (has_tex) {
        for (size_t i = 0; i < nv; i++) {
            mesh.uvs.push_back(Vec2(
                fs_mesh.vertex_texcoords[i * 2],
                1.0f - fs_mesh.vertex_texcoords[i * 2 + 1]));
        }
    }

    // Fill per-vertex colors (white — texture provides the color).
    mesh.colors.assign(nv, Color(1, 1, 1, 1));

    // Copy face indices.
    mesh.triangles.reserve(nf);
    for (size_t i = 0; i < nf; i++) {
        mesh.triangles.push_back(Triangle{
            static_cast<uint32_t>(fs_mesh.faces[i * 3]),
            static_cast<uint32_t>(fs_mesh.faces[i * 3 + 1]),
            static_cast<uint32_t>(fs_mesh.faces[i * 3 + 2])});
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

    Vec3 cow_min, cow_max;
    mesh.compute_bounding_box(cow_min, cow_max);
    Vec3 cow_extent = cow_max - cow_min;
    float plane_half_x = cow_extent.x * 0.9f;
    float plane_half_z = cow_extent.z * 0.6f;
    Vec3 plane_center((cow_min.x + cow_max.x) * 0.5f, cow_min.y, (cow_min.z + cow_max.z) * 0.5f);

    Mesh plane = generate_plane(plane_center, Vec3(0.0f, 1.0f, 0.0f),
                                plane_half_x, plane_half_z, Color(0.1f, 0.7f, 0.2f));

    Scene scene;
    scene.meshes.push_back(std::move(mesh));
    scene.meshes.push_back(std::move(plane));

    Vec3 eye_dir = glm::normalize(Vec3(1.0f, 0.4f, -1.2f));
    Camera cam = scimesh::camera_fit_scene(scene, eye_dir,
        Vec3(0.0f, 1.0f, 0.0f), 40.0f, 1.05f);

    scimesh::Light key_light;
    key_light.position = Vec3(0.5f, 1.0f, 1.0f);
    key_light.color    = Color(1.00f, 0.97f, 0.90f);
    key_light.intensity = 3.0f;

    scimesh::Light fill_light;
    fill_light.position = Vec3(-1.0f, 0.2f, 0.5f);
    fill_light.color    = Color(0.4f, 0.5f, 0.8f);
    fill_light.intensity = 1.0f;

    scimesh::Light rim_light;
    rim_light.position = Vec3(0.0f, -0.3f, -1.0f);
    rim_light.color    = Color(0.5f, 0.5f, 0.5f);
    rim_light.intensity = 0.8f;

    RenderOptions opts;
    opts.width  = 1200;
    opts.height = 900;
    opts.background_color = Color(0.70f, 0.70f, 0.9f);
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
    Image img = renderer.render_scene(scene, cam, opts);

    img.write_ppm("spot_cow.ppm");
    img.write_bmp("spot_cow.bmp");
    img.write_png("spot_cow.png");
    std::cout << "Wrote spot_cow.ppm, spot_cow.bmp, spot_cow.png ("
              << img.width << "x" << img.height << ")\n";

    return 0;
}
