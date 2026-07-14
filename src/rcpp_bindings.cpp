#include <Rcpp.h>
#include "core/renderer.h"
#include "core/transforms.h"
#include "core/normals.h"
#include "core/primitives.h"
#include "core/stl_io.h"
#include "core/obj_io.h"
#include "core/ply_io.h"

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

    if (mesh_desc.containsElementNamed("face_colors") &&
        !Rf_isNull(mesh_desc["face_colors"])) {
        NumericMatrix fcols = mesh_desc["face_colors"];
        int nfc = fcols.nrow();
        int ncc = fcols.ncol();
        for (int i = 0; i < nfc; i++) {
            mesh.face_colors.push_back(scimesh::Color(
                static_cast<float>(fcols(i, 0)),
                static_cast<float>(ncc > 1 ? fcols(i, 1) : 0.0f),
                static_cast<float>(ncc > 2 ? fcols(i, 2) : 0.0f),
                static_cast<float>(ncc > 3 ? fcols(i, 3) : 1.0f)));
        }
        for (const auto &c : mesh.face_colors) {
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

    if (mesh_desc.containsElementNamed("uv") &&
        !Rf_isNull(mesh_desc["uv"])) {
        NumericMatrix uv_mat = mesh_desc["uv"];
        int nuv = uv_mat.nrow();
        for (int i = 0; i < nuv; i++) {
            mesh.uvs.push_back(scimesh::Vec2(
                static_cast<float>(uv_mat(i, 0)),
                static_cast<float>(uv_mat.ncol() > 1 ? uv_mat(i, 1) : 0.0f)));
        }
    }

    if (mesh_desc.containsElementNamed("texture") &&
        !Rf_isNull(mesh_desc["texture"])) {
        List tex = mesh_desc["texture"];
        mesh.texture.width = as<int>(tex["width"]);
        mesh.texture.height = as<int>(tex["height"]);
        RawVector pix = tex["pixels"];
        mesh.texture.pixels.assign(pix.begin(), pix.end());
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
    if (opt_desc.containsElementNamed("projection") &&
        !Rf_isNull(opt_desc["projection"])) {
        opts.projection = parse_projection(as<std::string>(opt_desc["projection"]));
    }
    if (opt_desc.containsElementNamed("lights") &&
        !Rf_isNull(opt_desc["lights"])) {
        List lights_list = opt_desc["lights"];
        for (int i = 0; i < lights_list.size(); i++) {
            List ldesc = lights_list[i];
            scimesh::Light light;
            if (ldesc.containsElementNamed("position") &&
                !Rf_isNull(ldesc["position"])) {
                light.position = vec3_from_r(ldesc["position"]);
            }
            if (ldesc.containsElementNamed("color") &&
                !Rf_isNull(ldesc["color"])) {
                light.color = color_from_r(ldesc["color"]);
            }
            if (ldesc.containsElementNamed("intensity") &&
                !Rf_isNull(ldesc["intensity"])) {
                light.intensity = as<float>(ldesc["intensity"]);
            }
            if (ldesc.containsElementNamed("directional") &&
                !Rf_isNull(ldesc["directional"])) {
                light.is_directional = as<bool>(ldesc["directional"]);
            }
            opts.lights.push_back(light);
        }
    }
    if (opt_desc.containsElementNamed("ambient") &&
        !Rf_isNull(opt_desc["ambient"])) {
        opts.ambient = as<float>(opt_desc["ambient"]);
    }

    if (opt_desc.containsElementNamed("fog_enabled") &&
        !Rf_isNull(opt_desc["fog_enabled"])) {
        opts.fog_enabled = as<bool>(opt_desc["fog_enabled"]);
    }
    if (opt_desc.containsElementNamed("fog_start") &&
        !Rf_isNull(opt_desc["fog_start"])) {
        opts.fog_start = as<float>(opt_desc["fog_start"]);
    }
    if (opt_desc.containsElementNamed("fog_end") &&
        !Rf_isNull(opt_desc["fog_end"])) {
        opts.fog_end = as<float>(opt_desc["fog_end"]);
    }
    if (opt_desc.containsElementNamed("fog_color") &&
        !Rf_isNull(opt_desc["fog_color"])) {
        opts.fog_color = color_from_r(opt_desc["fog_color"]);
    }

    if (opt_desc.containsElementNamed("threads") &&
        !Rf_isNull(opt_desc["threads"])) {
        opts.threads = as<int>(opt_desc["threads"]);
    }

    if (opt_desc.containsElementNamed("ssao_enabled") &&
        !Rf_isNull(opt_desc["ssao_enabled"])) {
        opts.ssao_enabled = as<bool>(opt_desc["ssao_enabled"]);
    }
    if (opt_desc.containsElementNamed("ssao_radius") &&
        !Rf_isNull(opt_desc["ssao_radius"])) {
        opts.ssao_radius = as<float>(opt_desc["ssao_radius"]);
    }
    if (opt_desc.containsElementNamed("ssao_intensity") &&
        !Rf_isNull(opt_desc["ssao_intensity"])) {
        opts.ssao_intensity = as<float>(opt_desc["ssao_intensity"]);
    }

    if (opt_desc.containsElementNamed("clip_planes") &&
        !Rf_isNull(opt_desc["clip_planes"])) {
        List cplanes = opt_desc["clip_planes"];
        for (int i = 0; i < cplanes.size(); i++) {
            List cp = cplanes[i];
            scimesh::ClipPlane plane;
            if (cp.containsElementNamed("normal") &&
                !Rf_isNull(cp["normal"])) {
                plane.normal = vec3_from_r(cp["normal"]);
            }
            if (cp.containsElementNamed("offset") &&
                !Rf_isNull(cp["offset"])) {
                plane.offset = as<float>(cp["offset"]);
            }
            opts.clip_planes.push_back(plane);
        }
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

scimesh::Image r_list_to_image(List r_img) {
    scimesh::Image img;
    img.width = as<int>(r_img["width"]);
    img.height = as<int>(r_img["height"]);
    RawVector r_pixels = r_img["pixels"];
    img.pixels.assign(r_pixels.begin(), r_pixels.end());
    return img;
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

    if (!mesh.face_colors.empty()) {
        int nfc = static_cast<int>(mesh.face_colors.size());
        NumericMatrix fcols(nfc, 4);
        for (int i = 0; i < nfc; i++) {
            fcols(i, 0) = mesh.face_colors[i].r;
            fcols(i, 1) = mesh.face_colors[i].g;
            fcols(i, 2) = mesh.face_colors[i].b;
            fcols(i, 3) = mesh.face_colors[i].a;
        }
        out["face_colors"] = fcols;
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

    if (!mesh.uvs.empty()) {
        int nu = static_cast<int>(mesh.uvs.size());
        NumericMatrix uvs_mat(nu, 2);
        for (int i = 0; i < nu; i++) {
            uvs_mat(i, 0) = mesh.uvs[i].x;
            uvs_mat(i, 1) = mesh.uvs[i].y;
        }
        out["uv"] = uvs_mat;
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
List scimesh_scale_mesh_nonuniform(List mesh_data, NumericVector scale) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::scale_mesh(mesh, vec3_from_r(scale));
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

// ---- Procedural primitives (single) -----------------------------------------
// [[Rcpp::export]]
List scimesh_generate_cone(NumericVector base, NumericVector tip,
                           double radius, int segments,
                           NumericVector color) {
    scimesh::Vec3 b = vec3_from_r(base);
    scimesh::Vec3 t = vec3_from_r(tip);
    scimesh::Color c = color_from_r(color);
    scimesh::Mesh mesh = scimesh::generate_cone(b, t, static_cast<float>(radius),
                                                segments, c);
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_generate_arrow(NumericVector from, NumericVector to,
                            double shaft_radius, double head_radius,
                            double head_length, int segments,
                            NumericVector color) {
    scimesh::Vec3 f = vec3_from_r(from);
    scimesh::Vec3 t = vec3_from_r(to);
    scimesh::Color c = color_from_r(color);
    scimesh::Mesh mesh = scimesh::generate_arrow(f, t,
        static_cast<float>(shaft_radius), static_cast<float>(head_radius),
        static_cast<float>(head_length), segments, c);
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

// ---- Points ------------------------------------------------------------------
// [[Rcpp::export]]
List scimesh_render_points_raw(NumericMatrix positions, NumericMatrix colors,
                               double radius,
                               List camera_data, List options_data) {
    int n = positions.nrow();
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
    scimesh::Image img = renderer.render_points_raw(verts, cols,
        static_cast<float>(radius), cam, opts);
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

// ---- STL I/O ----------------------------------------------------------------
// [[Rcpp::export]]
List scimesh_read_stl(CharacterVector path) {
    std::string p = as<std::string>(path);
    scimesh::Mesh mesh = scimesh::stl_io::read_stl(p);
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
void scimesh_write_stl(List mesh_data, CharacterVector path,
                       String format = "binary") {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    std::string p = as<std::string>(path);
    std::string fmt = format;
    scimesh::stl_io::write_stl(p, mesh, fmt);
}

// ---- OBJ I/O ----------------------------------------------------------------
// [[Rcpp::export]]
List scimesh_read_obj(CharacterVector path) {
    std::string p = as<std::string>(path);
    scimesh::Mesh mesh = scimesh::obj_io::read_obj(p);
    return mesh_to_r_list(mesh);
}

// ---- PLY I/O ----------------------------------------------------------------
// [[Rcpp::export]]
List scimesh_read_ply(CharacterVector path) {
    std::string p = as<std::string>(path);
    scimesh::Mesh mesh = scimesh::ply_io::read_ply(p);
    return mesh_to_r_list(mesh);
}

// ---- Image I/O --------------------------------------------------------------
// [[Rcpp::export]]
bool scimesh_write_png(List image, CharacterVector filename) {
    int width = as<int>(image["width"]);
    int height = as<int>(image["height"]);
    RawVector pixels = image["pixels"];

    scimesh::Image img;
    img.width = width;
    img.height = height;
    img.pixels.assign(pixels.begin(), pixels.end());

    return img.write_png(as<std::string>(filename));
}

// ---- Image manipulation -------------------------------------------------------
// [[Rcpp::export]]
List scimesh_image_crop(List image, int x, int y, int w, int h) {
    scimesh::Image img = r_list_to_image(image);
    img.crop(x, y, w, h);
    return image_to_r_list(img);
}

// [[Rcpp::export]]
List scimesh_image_merge(List image, List other, CharacterVector direction) {
    std::string dir = as<std::string>(direction);
    scimesh::MergeDirection md;
    if (dir == "left")       md = scimesh::MergeDirection::LEFT;
    else if (dir == "right") md = scimesh::MergeDirection::RIGHT;
    else if (dir == "top")   md = scimesh::MergeDirection::TOP;
    else if (dir == "bottom") md = scimesh::MergeDirection::BOTTOM;
    else {
        Rcpp::stop("direction must be 'left', 'right', 'top', or 'bottom'");
    }
    scimesh::Image img = r_list_to_image(image);
    scimesh::Image oth = r_list_to_image(other);
    img.merge(oth, md);
    return image_to_r_list(img);
}

// [[Rcpp::export]]
List scimesh_image_grow(List image, int top, int bottom, int left, int right,
                         NumericVector background) {
    if (background.size() != 4) {
        Rcpp::stop("background must be a numeric vector of length 4 (RGBA)");
    }
    scimesh::Image img = r_list_to_image(image);
    scimesh::Color bg(static_cast<float>(background[0]),
                      static_cast<float>(background[1]),
                      static_cast<float>(background[2]),
                      static_cast<float>(background[3]));
    img.grow(top, bottom, left, right, bg);
    return image_to_r_list(img);
}

// [[Rcpp::export]]
List scimesh_image_rotate_90(List image, bool clockwise) {
    scimesh::Image img = r_list_to_image(image);
    img.rotate_90(clockwise);
    return image_to_r_list(img);
}

// [[Rcpp::export]]
List scimesh_image_scale(List image, int new_width, int new_height) {
    scimesh::Image img = r_list_to_image(image);
    img.scale(new_width, new_height);
    return image_to_r_list(img);
}

// ---- Normals -----------------------------------------------------------------
// [[Rcpp::export]]
List scimesh_compute_vertex_normals(List mesh_data) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    std::vector<scimesh::Vec3> normals;
    scimesh::compute_vertex_normals(mesh, normals);
    mesh.normals = normals;
    return mesh_to_r_list(mesh);
}
