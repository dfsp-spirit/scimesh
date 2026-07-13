/// Demo: render each scimesh primitive from front and back, with
/// shaded + wireframe side-by-side composites, for visual inspection.
///
/// Build:
///   cd examples/cpp/all_primitives && mkdir -p build && cd build
///   cmake .. -DCMAKE_BUILD_TYPE=Release && make
///
/// Run (from build/):
///   ./all_primitives
///
/// Output (in current directory):
///   sphere_front.png, sphere_back.png,
///   cuboid_front.png, cuboid_back.png,
///   cylinder_front.png, cylinder_back.png,
///   ... etc. (9 primitives × 2 views = 18 PNGs)

#include "renderer.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include "normals.h"
#include "primitives.h"
#include "types.h"

#include <iostream>
#include <chrono>
#include <string>
#include <cstring>
#include <cstdint>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;

static void ensure_normals(Mesh &mesh) {
    if (!mesh.has_normals()) {
        std::vector<Vec3> norms;
        scimesh::compute_vertex_normals(mesh, norms);
        mesh.normals = norms;
    }
}

static Image compose_side_by_side(const Image &left, const Image &right) {
    int lw = left.width, lh = left.height;
    int rw = right.width, rh = right.height;
    int out_w = lw + rw;
    int out_h = std::max(lh, rh);
    Image out(out_w, out_h);

    for (int y = 0; y < lh; y++) {
        std::memcpy(&out.pixels[(y * out_w) * 4],
                    &left.pixels[(y * lw) * 4],
                    lw * 4);
    }
    for (int y = 0; y < rh; y++) {
        std::memcpy(&out.pixels[(y * out_w + lw) * 4],
                    &right.pixels[(y * rw) * 4],
                    rw * 4);
    }
    return out;
}

static Vec3 opposite(const Vec3 &d) {
    return Vec3(-d.x, -d.y, -d.z);
}

static void render_mesh_front_back(const char *name, Mesh &mesh,
                                   const Vec3 &front_dir,
                                   const Vec3 &up = Vec3(0, 1, 0),
                                   float fov = 45, float margin = 1.15,
                                   int w = 400, int h = 300) {

    ensure_normals(mesh);

    Renderer renderer;

    Camera cam_front = scimesh::camera_fit_mesh(mesh, front_dir, up, fov, margin);
    Camera cam_back  = scimesh::camera_fit_mesh(mesh, opposite(front_dir), up, fov, margin);

    scimesh::Light key = {Vec3(0.5f, 1.0f, 0.8f), Color(1.0f, 0.95f, 0.9f), 1.5f};
    scimesh::Light fill = {Vec3(-0.5f, 0.2f, 0.6f), Color(0.4f, 0.5f, 0.8f), 0.5f};

    // ---- Shaded options ----
    RenderOptions opts_shaded;
    opts_shaded.width  = w;
    opts_shaded.height = h;
    opts_shaded.shading = ShadingMode::SMOOTH;
    opts_shaded.backface_culling = true;
    opts_shaded.background_color = Color(0.92f, 0.93f, 0.95f, 1.0f);
    opts_shaded.ambient = 0.2f;
    opts_shaded.lights = {key, fill};
    opts_shaded.specular_color = Color(0.3f, 0.3f, 0.3f);
    opts_shaded.shininess = 32;

    // ---- Wireframe options ----
    RenderOptions opts_wire = opts_shaded;
    opts_wire.wireframe = true;
    opts_wire.wireframe_color = Color(0.0f, 0.0f, 0.0f, 1.0f);
    opts_wire.backface_culling = false;
    opts_wire.lights.clear();

    // ---- Front ----
    {
        Image shaded = renderer.render_mesh(mesh, cam_front, opts_shaded);
        Image wire   = renderer.render_mesh(mesh, cam_front, opts_wire);
        Image comp   = compose_side_by_side(shaded, wire);

        std::string fname = std::string(name) + "_front.png";
        comp.write_png(fname);
        std::cout << "  " << fname << " (" << comp.width << "x" << comp.height << ")\n";
    }

    // ---- Back ----
    {
        Image shaded = renderer.render_mesh(mesh, cam_back, opts_shaded);
        Image wire   = renderer.render_mesh(mesh, cam_back, opts_wire);
        Image comp   = compose_side_by_side(shaded, wire);

        std::string fname = std::string(name) + "_back.png";
        comp.write_png(fname);
        std::cout << "  " << fname << " (" << comp.width << "x" << comp.height << ")\n";
    }
}

int main() {
    std::cout << "Rendering all primitives front + back ...\n\n";

    Color red(0.9f, 0.3f, 0.2f, 1.0f);
    Color blue(0.2f, 0.6f, 1.0f, 1.0f);
    Color green(0.1f, 0.7f, 0.3f, 1.0f);
    Color yellow(0.9f, 0.7f, 0.1f, 1.0f);
    Color purple(0.7f, 0.2f, 0.8f, 1.0f);
    Color cyan(0.2f, 0.8f, 0.8f, 1.0f);
    Color brown(0.6f, 0.4f, 0.2f, 1.0f);
    Color gray(0.5f, 0.5f, 0.5f, 1.0f);

    {
        Mesh m = generate_sphere(Vec3(0, 0, 0), 1.2f, 32, red);
        render_mesh_front_back("sphere", m, Vec3(1.2f, 0.8f, 1));
    }
    {
        Mesh m = generate_cuboid(Vec3(0, 0, 0), Vec3(1, 1, 1), blue);
        render_mesh_front_back("cuboid", m, Vec3(1.2f, 0.8f, 1));
    }
    {
        Mesh m = generate_cylinder(Vec3(0, -1.2f, 0), Vec3(0, 1.2f, 0), 0.6f, 32, green);
        render_mesh_front_back("cylinder", m, Vec3(1.2f, 0.8f, 0.5f));
    }
    {
        Mesh m = generate_cone(Vec3(0, -1.2f, 0), Vec3(0, 1.5f, 0), 0.8f, 32, yellow);
        render_mesh_front_back("cone", m, Vec3(1.2f, 0.5f, 0.8f));
    }
    {
        Mesh m = generate_pyramid(Vec3(0, 0, 0), Vec3(0, 1.5f, 0), 1.0f, purple);
        ensure_normals(m);
        render_mesh_front_back("pyramid", m, Vec3(1.2f, 0.8f, 1));
    }
    {
        Mesh m = generate_tetrahedron(Vec3(-1, -0.5f, -1), Vec3(1, -0.5f, -1),
                                       Vec3(0, -0.5f, 1), Vec3(0, 1.2f, 0), cyan);
        ensure_normals(m);
        render_mesh_front_back("tetrahedron", m, Vec3(1.2f, 0.8f, 0.6f));
    }
    {
        Mesh m = generate_torus(Vec3(0, 0, 0), 1.0f, 0.35f, 24, 12, brown);
        ensure_normals(m);
        render_mesh_front_back("torus", m, Vec3(0.0f, 0.5f, 1.2f));
    }
    {
        Mesh m = generate_plane(Vec3(0, 0, 0), Vec3(0, 1, 0), 1.2f, 0.8f, gray);
        render_mesh_front_back("plane", m, Vec3(0.3f, 0.8f, 1));
    }
    {
        Mesh m = generate_arrow(Vec3(0, -1.5f, 0), Vec3(0, 1.5f, 0),
                                0.12f, 0.35f, 0.7f, 32, red);
        render_mesh_front_back("arrow", m, Vec3(1.2f, 0.5f, 0.5f));
    }

    std::cout << "\nDone.\n";
    return 0;
}
