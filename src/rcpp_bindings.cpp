#include <Rcpp.h>
#include "core/renderer.h"
#include "core/transforms.h"
#include "core/primitives.h"

using namespace Rcpp;

namespace {

scimesh::Color color_from_r(const NumericVector &v) {
    int n = v.size();
    return scimesh::Color(
        static_cast<float>(n > 0 ? v[0] : 0.0),
        static_cast<float>(n > 1 ? v[1] : 0.0),
        static_cast<float>(n > 2 ? v[2] : 0.0),
        static_cast<float>(n > 3 ? v[3] : 1.0));
}

scimesh::Vec3 vec3_from_r(const NumericVector &v) {
    return scimesh::Vec3(
        static_cast<float>(v[0]),
        static_cast<float>(v[1]),
        static_cast<float>(v[2]));
}

scimesh::ShadingMode parse_shading(const std::string &s) {
    if (s == "flat") return scimesh::ShadingMode::FLAT;
    return scimesh::ShadingMode::SMOOTH;
}

scimesh::ProjectionType parse_projection(const std::string &s) {
    if (s == "orthographic") return scimesh::ProjectionType::ORTHOGRAPHIC;
    return scimesh::ProjectionType::PERSPECTIVE;
}

scimesh::Mesh build_mesh_from_r(List mesh_desc) {
    scimesh::Mesh mesh;

    NumericMatrix verts = mesh_desc["vertices"];
    IntegerMatrix tris = mesh_desc["triangles"];
    int nv = verts.nrow();

    for (int i = 0; i < nv; i++) {
        mesh.vertices.push_back(scimesh::Vec3(
            static_cast<float>(verts(i, 0)),
            static_cast<float>(verts(i, 1)),
            static_cast<float>(verts(i, 2))));
    }

    int nt = tris.nrow();
    for (int i = 0; i < nt; i++) {
        mesh.triangles.push_back(scimesh::Triangle{
            static_cast<uint32_t>(tris(i, 0) - 1),
            static_cast<uint32_t>(tris(i, 1) - 1),
            static_cast<uint32_t>(tris(i, 2) - 1)});
    }

    if (mesh_desc.containsElementNamed("colors") &&
        !Rf_isNull(mesh_desc["colors"])) {
        NumericMatrix cols = mesh_desc["colors"];
        int nc = cols.ncol();
        for (int i = 0; i < nv; i++) {
            mesh.colors.push_back(scimesh::Color(
                static_cast<float>(cols(i, 0)),
                static_cast<float>(nc > 1 ? cols(i, 1) : 0.0f),
                static_cast<float>(nc > 2 ? cols(i, 2) : 0.0f),
                static_cast<float>(nc > 3 ? cols(i, 3) : 1.0f)));
        }
        for (const auto &c : mesh.colors) {
            if (c.a < 1.0f - 1e-6f) {
                mesh.has_transparency = true;
                break;
            }
        }
    }

    if (mesh_desc.containsElementNamed("normals") &&
        !Rf_isNull(mesh_desc["normals"])) {
        NumericMatrix nrm = mesh_desc["normals"];
        for (int i = 0; i < nv; i++) {
            mesh.normals.push_back(scimesh::Vec3(
                static_cast<float>(nrm(i, 0)),
                static_cast<float>(nrm(i, 1)),
                static_cast<float>(nrm(i, 2))));
        }
    }

    if (mesh_desc.containsElementNamed("default_color") &&
        !Rf_isNull(mesh_desc["default_color"])) {
        mesh.default_color = color_from_r(mesh_desc["default_color"]);
    }

    return mesh;
}

scimesh::Camera build_camera_from_r(List cam_desc) {
    scimesh::Camera cam;

    if (cam_desc.containsElementNamed("eye") &&
        !Rf_isNull(cam_desc["eye"])) {
        cam.eye = vec3_from_r(cam_desc["eye"]);
    }
    if (cam_desc.containsElementNamed("center") &&
        !Rf_isNull(cam_desc["center"])) {
        cam.center = vec3_from_r(cam_desc["center"]);
    }
    if (cam_desc.containsElementNamed("up") &&
        !Rf_isNull(cam_desc["up"])) {
        cam.up = vec3_from_r(cam_desc["up"]);
    }
    if (cam_desc.containsElementNamed("projection") &&
        !Rf_isNull(cam_desc["projection"])) {
        cam.projection = parse_projection(
            as<std::string>(cam_desc["projection"]));
    }
    if (cam_desc.containsElementNamed("fov") &&
        !Rf_isNull(cam_desc["fov"])) {
        cam.fov_degrees = as<float>(cam_desc["fov"]);
    }

    return cam;
}

scimesh::RenderOptions build_options_from_r(List opt_desc) {
    scimesh::RenderOptions opts;

    if (opt_desc.containsElementNamed("width") &&
        !Rf_isNull(opt_desc["width"])) {
        opts.width = as<int>(opt_desc["width"]);
    }
    if (opt_desc.containsElementNamed("height") &&
        !Rf_isNull(opt_desc["height"])) {
        opts.height = as<int>(opt_desc["height"]);
    }
    if (opt_desc.containsElementNamed("shading") &&
        !Rf_isNull(opt_desc["shading"])) {
        opts.shading = parse_shading(as<std::string>(opt_desc["shading"]));
    }
    if (opt_desc.containsElementNamed("backface_culling") &&
        !Rf_isNull(opt_desc["backface_culling"])) {
        opts.backface_culling = as<bool>(opt_desc["backface_culling"]);
    }
    if (opt_desc.containsElementNamed("background_color") &&
        !Rf_isNull(opt_desc["background_color"])) {
        opts.background_color = color_from_r(opt_desc["background_color"]);
    }
    if (opt_desc.containsElementNamed("default_color") &&
        !Rf_isNull(opt_desc["default_color"])) {
        opts.default_color = color_from_r(opt_desc["default_color"]);
    }
    if (opt_desc.containsElementNamed("invert_normals") &&
        !Rf_isNull(opt_desc["invert_normals"])) {
        opts.invert_normals = as<bool>(opt_desc["invert_normals"]);
    }
    if (opt_desc.containsElementNamed("wireframe") &&
        !Rf_isNull(opt_desc["wireframe"])) {
        opts.wireframe = as<bool>(opt_desc["wireframe"]);
    }
    if (opt_desc.containsElementNamed("wireframe_color") &&
        !Rf_isNull(opt_desc["wireframe_color"])) {
        opts.wireframe_color = color_from_r(opt_desc["wireframe_color"]);
    }
    if (opt_desc.containsElementNamed("aa_samples") &&
        !Rf_isNull(opt_desc["aa_samples"])) {
        opts.aa_samples = as<int>(opt_desc["aa_samples"]);
    }
    if (opt_desc.containsElementNamed("specular_color") &&
        !Rf_isNull(opt_desc["specular_color"])) {
        opts.specular_color = color_from_r(opt_desc["specular_color"]);
    }
    if (opt_desc.containsElementNamed("shininess") &&
        !Rf_isNull(opt_desc["shininess"])) {
        opts.shininess = as<float>(opt_desc["shininess"]);
    }

    return opts;
}

List image_to_r_list(const scimesh::Image &img) {
    int npixels = img.width * img.height * 4;
    RawVector pixels(npixels);
    if (!img.pixels.empty()) {
        std::memcpy(pixels.begin(), img.pixels.data(), npixels);
    }
    return List::create(
        Named("width") = img.width,
        Named("height") = img.height,
        Named("pixels") = pixels);
}

List mesh_to_r_list(const scimesh::Mesh &mesh) {
    int nv = static_cast<int>(mesh.vertices.size());
    NumericMatrix verts(nv, 3);
    for (int i = 0; i < nv; i++) {
        verts(i, 0) = mesh.vertices[i].x;
        verts(i, 1) = mesh.vertices[i].y;
        verts(i, 2) = mesh.vertices[i].z;
    }

    int nt = static_cast<int>(mesh.triangles.size());
    IntegerMatrix tris(nt, 3);
    for (int i = 0; i < nt; i++) {
        tris(i, 0) = static_cast<int>(mesh.triangles[i].v0) + 1;
        tris(i, 1) = static_cast<int>(mesh.triangles[i].v1) + 1;
        tris(i, 2) = static_cast<int>(mesh.triangles[i].v2) + 1;
    }

    List out = List::create(
        Named("vertices") = verts,
        Named("triangles") = tris);

    if (!mesh.colors.empty()) {
        int nc = static_cast<int>(mesh.colors.size());
        NumericMatrix cols(nc, 4);
        for (int i = 0; i < nc; i++) {
            cols(i, 0) = mesh.colors[i].r;
            cols(i, 1) = mesh.colors[i].g;
            cols(i, 2) = mesh.colors[i].b;
            cols(i, 3) = mesh.colors[i].a;
        }
        out["colors"] = cols;
    }

    if (!mesh.normals.empty()) {
        int nn = static_cast<int>(mesh.normals.size());
        NumericMatrix norms(nn, 3);
        for (int i = 0; i < nn; i++) {
            norms(i, 0) = mesh.normals[i].x;
            norms(i, 1) = mesh.normals[i].y;
            norms(i, 2) = mesh.normals[i].z;
        }
        out["normals"] = norms;
    }

    return out;
}

} // anonymous namespace


// [[Rcpp::export]]
List scimesh_render_mesh(List mesh_data, List camera_data, List options_data) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::Camera cam = build_camera_from_r(camera_data);
    scimesh::RenderOptions opts = build_options_from_r(options_data);

    scimesh::Renderer renderer;
    scimesh::Image result = renderer.render_mesh(mesh, cam, opts);

    return image_to_r_list(result);
}


// [[Rcpp::export]]
List scimesh_render_scene(List scene_data, List camera_data, List options_data) {
    List meshes = scene_data;
    scimesh::Scene scene;

    for (int i = 0; i < meshes.size(); i++) {
        scene.meshes.push_back(build_mesh_from_r(meshes[i]));
    }

    scimesh::Camera cam = build_camera_from_r(camera_data);
    scimesh::RenderOptions opts = build_options_from_r(options_data);

    scimesh::Renderer renderer;
    scimesh::Image result = renderer.render_scene(scene, cam, opts);

    return image_to_r_list(result);
}


// [[Rcpp::export]]
List scimesh_camera_fit_mesh(List mesh_data, NumericVector direction,
                              NumericVector up, double fov_degrees = 45.0,
                              double margin = 1.1) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::Vec3 dir = vec3_from_r(direction);
    scimesh::Vec3 up_vec = vec3_from_r(up);

    scimesh::Camera cam = scimesh::camera_fit_mesh(
        mesh, dir, up_vec,
        static_cast<float>(fov_degrees),
        static_cast<float>(margin));

    return List::create(
        Named("eye") = NumericVector::create(cam.eye.x, cam.eye.y, cam.eye.z),
        Named("center") = NumericVector::create(
            cam.center.x, cam.center.y, cam.center.z),
        Named("up") = NumericVector::create(cam.up.x, cam.up.y, cam.up.z),
        Named("projection") = (cam.projection == scimesh::ProjectionType::ORTHOGRAPHIC
            ? "orthographic" : "perspective"),
        Named("fov") = cam.fov_degrees);
}

// ---- Mesh transforms -------------------------------------------------------
// [[Rcpp::export]]
List scimesh_transform_mesh(List mesh_data, NumericMatrix matrix_4x4) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::Mat4 m;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            m[r][c] = static_cast<float>(matrix_4x4(r, c));
    scimesh::transform_mesh(mesh, m);
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_translate_mesh(List mesh_data, NumericVector translation) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::translate_mesh(mesh, vec3_from_r(translation));
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_scale_mesh(List mesh_data, double scale) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::scale_mesh(mesh, static_cast<float>(scale));
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_rotate_mesh(List mesh_data, double angle_rad, NumericVector axis) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::rotate_mesh(mesh, static_cast<float>(angle_rad), vec3_from_r(axis));
    return mesh_to_r_list(mesh);
}

// ---- Multi primitives ------------------------------------------------------
// [[Rcpp::export]]
List scimesh_generate_multi_spheres(NumericMatrix centers, NumericVector radii,
                                    NumericMatrix colors, int segments = 16) {
    int n = centers.nrow();
    std::vector<scimesh::Vec3> c;
    std::vector<float> r;
    std::vector<scimesh::Color> col;
    for (int i = 0; i < n; i++) {
        c.push_back(scimesh::Vec3(
            static_cast<float>(centers(i, 0)),
            static_cast<float>(centers(i, 1)),
            static_cast<float>(centers(i, 2))));
        r.push_back(static_cast<float>(radii[i]));
        col.push_back(scimesh::Color(
            static_cast<float>(colors(i, 0)),
            static_cast<float>(colors(i, 1)),
            static_cast<float>(colors(i, 2)),
            static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
    }
    scimesh::Mesh mesh = scimesh::generate_multi_spheres(c, r, col, segments);
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_generate_multi_cylinders(NumericMatrix starts, NumericMatrix ends,
                                      NumericVector radii, NumericMatrix colors,
                                      int segments = 12) {
    int n = starts.nrow();
    std::vector<scimesh::Vec3> s, e;
    std::vector<float> r;
    std::vector<scimesh::Color> col;
    for (int i = 0; i < n; i++) {
        s.push_back(scimesh::Vec3(
            static_cast<float>(starts(i, 0)),
            static_cast<float>(starts(i, 1)),
            static_cast<float>(starts(i, 2))));
        e.push_back(scimesh::Vec3(
            static_cast<float>(ends(i, 0)),
            static_cast<float>(ends(i, 1)),
            static_cast<float>(ends(i, 2))));
        r.push_back(static_cast<float>(radii[i]));
        col.push_back(scimesh::Color(
            static_cast<float>(colors(i, 0)),
            static_cast<float>(colors(i, 1)),
            static_cast<float>(colors(i, 2)),
            static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
    }
    scimesh::Mesh mesh = scimesh::generate_multi_cylinders(s, e, r, col, segments);
    return mesh_to_r_list(mesh);
}

// ---- Raw triangles ----------------------------------------------------------
// [[Rcpp::export]]
List scimesh_render_triangles_raw(NumericMatrix positions, NumericMatrix colors,
                                  List camera_data, List options_data) {
    int n = positions.nrow();
    if (n % 3 != 0) stop("positions must have a multiple of 3 rows (3 per triangle)");
    std::vector<scimesh::Vec3> verts;
    std::vector<scimesh::Color> cols;
    for (int i = 0; i < n; i++) {
        verts.push_back(scimesh::Vec3(
            static_cast<float>(positions(i, 0)),
            static_cast<float>(positions(i, 1)),
            static_cast<float>(positions(i, 2))));
        cols.push_back(scimesh::Color(
            static_cast<float>(colors(i, 0)),
            static_cast<float>(colors(i, 1)),
            static_cast<float>(colors(i, 2)),
            static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
    }
    scimesh::Camera cam = build_camera_from_r(camera_data);
    scimesh::RenderOptions opts = build_options_from_r(options_data);
    scimesh::Renderer renderer;
    scimesh::Image img = renderer.render_triangles_raw(verts, cols, cam, opts);
    return image_to_r_list(img);
}

// ---- Procedural geometry ----------------------------------------------------
// [[Rcpp::export]]
List scimesh_generate_cuboid(NumericVector center, NumericVector half_extents,
                             NumericVector color) {
    auto m = scimesh::generate_cuboid(vec3_from_r(center), vec3_from_r(half_extents),
                                      color_from_r(color));
    return mesh_to_r_list(m);
}

// [[Rcpp::export]]
List scimesh_generate_pyramid(NumericVector base_center, NumericVector apex,
                              double half_width, NumericVector color) {
    auto m = scimesh::generate_pyramid(vec3_from_r(base_center), vec3_from_r(apex),
                                       static_cast<float>(half_width), color_from_r(color));
    return mesh_to_r_list(m);
}

// [[Rcpp::export]]
List scimesh_generate_tetrahedron(NumericVector p0, NumericVector p1,
                                  NumericVector p2, NumericVector p3,
                                  NumericVector color) {
    auto m = scimesh::generate_tetrahedron(vec3_from_r(p0), vec3_from_r(p1),
                                           vec3_from_r(p2), vec3_from_r(p3),
                                           color_from_r(color));
    return mesh_to_r_list(m);
}

// [[Rcpp::export]]
List scimesh_generate_torus(NumericVector center, double major_radius,
                            double minor_radius, int major_segments,
                            int minor_segments, NumericVector color) {
    auto m = scimesh::generate_torus(vec3_from_r(center),
                                     static_cast<float>(major_radius),
                                     static_cast<float>(minor_radius),
                                     major_segments, minor_segments,
                                     color_from_r(color));
    return mesh_to_r_list(m);
}

// [[Rcpp::export]]
List scimesh_generate_plane(NumericVector center, NumericVector normal,
                            double half_size_x, double half_size_y,
                            NumericVector color) {
    auto m = scimesh::generate_plane(vec3_from_r(center), vec3_from_r(normal),
                                     static_cast<float>(half_size_x),
                                     static_cast<float>(half_size_y),
                                     color_from_r(color));
    return mesh_to_r_list(m);
}
