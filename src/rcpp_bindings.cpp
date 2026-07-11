#include <Rcpp.h>
#include "core/renderer.h"

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
