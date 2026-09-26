#include <Rcpp.h>
#include <sstream>
#include <scimesh/renderer.h>
#include <scimesh/transforms.h>
#include <scimesh/normals.h>
#include <scimesh/primitives.h>
#include <scimesh/spline.h>
#include <scimesh/colormap.h>
#include <scimesh/to_string.h>
#include <scimesh/stl_io.h>
#include <scimesh/obj_io.h>
#include <scimesh/ply_io.h>
#include <scimesh/gltf_io.h>
#include <scimesh/font.h>
#include <scimesh/text.h>

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

// Nx3 point matrix -> path.  Rows are points, columns are x, y, z.
std::vector<scimesh::Vec3> vec3_path_from_r(const NumericMatrix &m) {
    std::vector<scimesh::Vec3> path;
    path.reserve(static_cast<size_t>(m.nrow()));
    for (int i = 0; i < m.nrow(); i++) {
        path.push_back(scimesh::Vec3(
            static_cast<float>(m(i, 0)),
            static_cast<float>(m(i, 1)),
            static_cast<float>(m(i, 2))));
    }
    return path;
}

// Path -> Nx3 point matrix (0 rows for an empty path, which the R layer
// treats as "no points").
NumericMatrix vec3_path_to_r(const std::vector<scimesh::Vec3> &path) {
    NumericMatrix out(static_cast<int>(path.size()), 3);
    for (size_t i = 0; i < path.size(); i++) {
        const int row = static_cast<int>(i);
        out(row, 0) = static_cast<double>(path[i].x);
        out(row, 1) = static_cast<double>(path[i].y);
        out(row, 2) = static_cast<double>(path[i].z);
    }
    return out;
}

scimesh::ShadingMode parse_shading(const std::string &s) {
    if (s == "flat") return scimesh::ShadingMode::FLAT;
    return scimesh::ShadingMode::SMOOTH;
}

scimesh::ProjectionType parse_projection(const std::string &s) {
    if (s == "orthographic") return scimesh::ProjectionType::ORTHOGRAPHIC;
    return scimesh::ProjectionType::PERSPECTIVE;
}

scimesh::PlaneSpace parse_plane_space(const std::string &s) {
    if (s == "eye") return scimesh::PlaneSpace::EYE;
    return scimesh::PlaneSpace::WORLD;
}

scimesh::FogSpace parse_fog_space(const std::string &s) {
    if (s == "ndc") return scimesh::FogSpace::NDC;
    return scimesh::FogSpace::WORLD;
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
    if (opt_desc.containsElementNamed("near_plane") &&
        !Rf_isNull(opt_desc["near_plane"])) {
        opts.near_plane = as<float>(opt_desc["near_plane"]);
    }
    if (opt_desc.containsElementNamed("far_plane") &&
        !Rf_isNull(opt_desc["far_plane"])) {
        opts.far_plane = as<float>(opt_desc["far_plane"]);
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

    if (opt_desc.containsElementNamed("contrast") &&
        !Rf_isNull(opt_desc["contrast"])) {
        opts.contrast = as<float>(opt_desc["contrast"]);
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
    if (opt_desc.containsElementNamed("fog_space") &&
        !Rf_isNull(opt_desc["fog_space"])) {
        opts.fog_space = parse_fog_space(as<std::string>(opt_desc["fog_space"]));
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
            if (cp.containsElementNamed("space") &&
                !Rf_isNull(cp["space"])) {
                plane.space = parse_plane_space(as<std::string>(cp["space"]));
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
    List out = List::create(
        Named("width") = img.width,
        Named("height") = img.height,
        Named("pixels") = pixels);
    out.attr("class") = "scimesh_image";
    return out;
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

    out.attr("class") = "scimesh_mesh";
    return out;
}

scimesh::Mat4 mat4_from_r(const NumericMatrix &m) {
    // Standard homogeneous convention: the R matrix M (displayed row-major)
    // is applied as M * p.  glm stores column-major, so the glm element at
    // (column c, row r) holds M(r, c).
    scimesh::Mat4 out;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[c][r] = static_cast<float>(m(r, c));
    return out;
}

/// Build a scimesh LineLayer from an R line layer descriptor.
///
/// The descriptor is a list with components `from` (Nx3), `to` (Nx3),
/// optional `colors` (Nx4 or a single RGBA vector), `width`, `depth_test` and
/// `lit`.  Use lines() on the R side to create one.
scimesh::LineLayer build_line_layer_from_r(List layer) {
    scimesh::LineLayer out;

    NumericMatrix from = layer["from"];
    NumericMatrix to = layer["to"];
    const int n = std::min(from.nrow(), to.nrow());
    out.from.reserve(n);
    out.to.reserve(n);
    for (int i = 0; i < n; i++) {
        out.from.push_back(scimesh::Vec3(
            static_cast<float>(from(i, 0)),
            static_cast<float>(from(i, 1)),
            static_cast<float>(from(i, 2))));
        out.to.push_back(scimesh::Vec3(
            static_cast<float>(to(i, 0)),
            static_cast<float>(to(i, 1)),
            static_cast<float>(to(i, 2))));
    }

    if (layer.containsElementNamed("colors")) {
        SEXP cs = layer["colors"];
        if (cs != R_NilValue) {
            NumericMatrix colors(cs);
            out.colors.reserve(colors.nrow());
            for (int i = 0; i < colors.nrow(); i++) {
                out.colors.push_back(scimesh::Color(
                    static_cast<float>(colors(i, 0)),
                    static_cast<float>(colors(i, 1)),
                    static_cast<float>(colors(i, 2)),
                    static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
            }
        }
    }

    if (layer.containsElementNamed("width")) {
        SEXP w = layer["width"];
        if (w != R_NilValue) {
            out.width = static_cast<float>(as<double>(w));
        }
    }
    if (layer.containsElementNamed("depth_test")) {
        SEXP d = layer["depth_test"];
        if (d != R_NilValue) {
            out.depth_test = as<bool>(d);
        }
    }
    if (layer.containsElementNamed("lit")) {
        SEXP l = layer["lit"];
        if (l != R_NilValue) {
            out.lit = as<bool>(l);
        }
    }
    if (layer.containsElementNamed("affects_bounds")) {
        SEXP a = layer["affects_bounds"];
        if (a != R_NilValue) {
            out.affects_bounds = as<bool>(a);
        }
    }

    return out;
}

/// Build a scimesh TextLayer from an R text layer descriptor.
///
/// The descriptor is a list with components `strings` (character vector),
/// `positions` (Nx3 or Nx2 numeric matrix; Nx2 is screen space), optional
/// `colors` (Nx4 or a single RGBA vector), `size` (text height in pixels),
/// `font_file` (path to a .ttf, "" = bundled font), `space` ("world" or
/// "screen"), `adj` (length 2), `offset` (length 2), `line_spacing`,
/// `rotation` (degrees), `depth_test`, `halo_color` (length 3 or 4) and
/// `halo_width`.  Use text_layer() on the R side to create one.
scimesh::TextLayer build_text_layer_from_r(List layer) {
    scimesh::TextLayer out;

    CharacterVector strings = layer["strings"];
    out.strings.reserve(strings.size());
    for (int i = 0; i < strings.size(); i++) {
        out.strings.push_back(as<std::string>(strings[i]));
    }

    NumericMatrix positions = layer["positions"];
    out.positions.reserve(positions.nrow());
    for (int i = 0; i < positions.nrow(); i++) {
        out.positions.push_back(scimesh::Vec3(
            static_cast<float>(positions(i, 0)),
            static_cast<float>(positions(i, 1)),
            positions.ncol() > 2 ? static_cast<float>(positions(i, 2)) : 0.0f));
    }

    if (layer.containsElementNamed("colors")) {
        SEXP cs = layer["colors"];
        if (cs != R_NilValue) {
            NumericMatrix colors(cs);
            out.colors.reserve(colors.nrow());
            for (int i = 0; i < colors.nrow(); i++) {
                out.colors.push_back(scimesh::Color(
                    static_cast<float>(colors(i, 0)),
                    static_cast<float>(colors(i, 1)),
                    static_cast<float>(colors(i, 2)),
                    static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
            }
        }
    }

    if (layer.containsElementNamed("size")) {
        SEXP s = layer["size"];
        if (s != R_NilValue) out.size = static_cast<float>(as<double>(s));
    }
    if (layer.containsElementNamed("font_file")) {
        SEXP f = layer["font_file"];
        if (f != R_NilValue) out.font_file = as<std::string>(f);
    }
    if (layer.containsElementNamed("space")) {
        SEXP sp = layer["space"];
        if (sp != R_NilValue) {
            out.space = (as<std::string>(sp) == "screen")
                            ? scimesh::TextSpace::SCREEN
                            : scimesh::TextSpace::WORLD;
        }
    }
    if (layer.containsElementNamed("adj")) {
        SEXP a = layer["adj"];
        if (a != R_NilValue) {
            NumericVector adj(a);
            if (adj.size() >= 2) {
                out.adj = scimesh::Vec2(static_cast<float>(adj[0]),
                                        static_cast<float>(adj[1]));
            }
        }
    }
    if (layer.containsElementNamed("offset")) {
        SEXP o = layer["offset"];
        if (o != R_NilValue) {
            NumericVector offset(o);
            if (offset.size() >= 2) {
                out.offset = scimesh::Vec2(static_cast<float>(offset[0]),
                                           static_cast<float>(offset[1]));
            }
        }
    }
    if (layer.containsElementNamed("line_spacing")) {
        SEXP ls = layer["line_spacing"];
        if (ls != R_NilValue) out.line_spacing = static_cast<float>(as<double>(ls));
    }
    if (layer.containsElementNamed("rotation")) {
        SEXP r = layer["rotation"];
        if (r != R_NilValue) out.rotation = static_cast<float>(as<double>(r));
    }
    if (layer.containsElementNamed("depth_test")) {
        SEXP d = layer["depth_test"];
        if (d != R_NilValue) out.depth_test = as<bool>(d);
    }
    if (layer.containsElementNamed("halo_color")) {
        SEXP h = layer["halo_color"];
        if (h != R_NilValue) {
            NumericVector halo(h);
            if (halo.size() >= 3) {
                out.halo_color = scimesh::Color(
                    static_cast<float>(halo[0]), static_cast<float>(halo[1]),
                    static_cast<float>(halo[2]),
                    static_cast<float>(halo.size() > 3 ? halo[3] : 1.0f));
            }
        }
    }
    if (layer.containsElementNamed("halo_width")) {
        SEXP hw = layer["halo_width"];
        if (hw != R_NilValue) out.halo_width = static_cast<float>(as<double>(hw));
    }

    return out;
}

/// Build a scimesh Scene from an R list of mesh descriptors or scene nodes.
///
/// Each entry may be a bare mesh descriptor (scimesh or rgl format, via
/// build_mesh_from_r), a scene node list with components `mesh` (a mesh
/// descriptor), optional `transform` (4x4 NumericMatrix), and optional
/// `name` (string), a line layer (class `scimesh_lines`, created by lines()),
/// or a text layer (class `scimesh_text`, created by text_layer()).  Line and
/// text layers may be wrapped into a scene node with a `transform`.
scimesh::Scene build_scene_from_r(List scene_data) {
    scimesh::Scene scene;
    for (int i = 0; i < scene_data.size(); i++) {
        List entry = scene_data[i];
        scimesh::Mat4 t(1.0f);
        std::string name;

        // Line layer, possibly wrapped into a scene node (with a transform).
        SEXP layer_sexp = R_NilValue;
        if (Rf_inherits(entry, "scimesh_lines")) {
            layer_sexp = entry;
        } else if (entry.containsElementNamed("lines")) {
            SEXP inner = entry["lines"];
            if (inner != R_NilValue && Rf_inherits(inner, "scimesh_lines")) {
                layer_sexp = inner;
                if (entry.containsElementNamed("transform")) {
                    SEXP tr = entry["transform"];
                    if (tr != R_NilValue) {
                        t = mat4_from_r(NumericMatrix(tr));
                    }
                }
                if (entry.containsElementNamed("name")) {
                    SEXP nm = entry["name"];
                    if (nm != R_NilValue) {
                        name = as<std::string>(nm);
                    }
                }
            }
        }
        if (layer_sexp != R_NilValue) {
            scene.add_lines(build_line_layer_from_r(List(layer_sexp)), t, name);
            continue;
        }

        // Text layer, possibly wrapped into a scene node (with a transform).
        SEXP text_sexp = R_NilValue;
        if (Rf_inherits(entry, "scimesh_text")) {
            text_sexp = entry;
        } else if (entry.containsElementNamed("text")) {
            SEXP inner = entry["text"];
            if (inner != R_NilValue && Rf_inherits(inner, "scimesh_text")) {
                text_sexp = inner;
                if (entry.containsElementNamed("transform")) {
                    SEXP tr = entry["transform"];
                    if (tr != R_NilValue) {
                        t = mat4_from_r(NumericMatrix(tr));
                    }
                }
                if (entry.containsElementNamed("name")) {
                    SEXP nm = entry["name"];
                    if (nm != R_NilValue) {
                        name = as<std::string>(nm);
                    }
                }
            }
        }
        if (text_sexp != R_NilValue) {
            scene.add_texts(build_text_layer_from_r(List(text_sexp)), t, name);
            continue;
        }

        scimesh::Mesh mesh;
        SEXP me = entry["mesh"];
        if (TYPEOF(me) == VECSXP) {
            // scene node form: list(mesh = ..., transform = ..., name = ...)
            mesh = build_mesh_from_r(entry["mesh"]);
            if (entry.containsElementNamed("transform")) {
                SEXP tr = entry["transform"];
                if (tr != R_NilValue) {
                    t = mat4_from_r(NumericMatrix(tr));
                }
            }
            if (entry.containsElementNamed("name")) {
                SEXP nm = entry["name"];
                if (nm != R_NilValue) {
                    name = as<std::string>(nm);
                }
            }
        } else {
            // bare mesh descriptor
            mesh = build_mesh_from_r(entry);
        }
        scene.meshes.push_back(mesh);
        scene.transforms.push_back(t);
        scene.names.push_back(name);
    }
    return scene;
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
    scimesh::Scene scene = build_scene_from_r(scene_data);

    scimesh::Camera cam = build_camera_from_r(camera_data);
    scimesh::RenderOptions opts = build_options_from_r(options_data);

    scimesh::Renderer renderer;
    scimesh::Image result = renderer.render_scene(scene, cam, opts);

    return image_to_r_list(result);
}


// ---- Cameras -----------------------------------------------------------------

/// Convert a camera to the R representation used by camera()/camera_auto().
List camera_to_r_list(const scimesh::Camera &cam) {
    List out = List::create(
        Named("eye") = NumericVector::create(cam.eye.x, cam.eye.y, cam.eye.z),
        Named("center") = NumericVector::create(
            cam.center.x, cam.center.y, cam.center.z),
        Named("up") = NumericVector::create(cam.up.x, cam.up.y, cam.up.z),
        Named("projection") = (cam.projection == scimesh::ProjectionType::ORTHOGRAPHIC
            ? "orthographic" : "perspective"),
        Named("fov") = cam.fov_degrees);
    out.attr("class") = "scimesh_camera";
    return out;
}

// [[Rcpp::export]]
List scimesh_camera_fit_mesh(List mesh_data, NumericVector direction,
                              NumericVector up, double fov_degrees = 45.0,
                              double margin = 1.1,
                              CharacterVector projection = "perspective") {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::Vec3 dir = vec3_from_r(direction);
    scimesh::Vec3 up_vec = vec3_from_r(up);

    std::string proj_str = as<std::string>(projection);
    scimesh::ProjectionType proj = scimesh::ProjectionType::PERSPECTIVE;
    if (proj_str == "orthographic") proj = scimesh::ProjectionType::ORTHOGRAPHIC;

    scimesh::Camera cam = scimesh::camera_fit_mesh(
        mesh, dir, up_vec,
        static_cast<float>(fov_degrees),
        static_cast<float>(margin),
        proj);

    return camera_to_r_list(cam);
}

// [[Rcpp::export]]
List scimesh_camera_fit_scene(List scene_data, NumericVector direction,
                               NumericVector up, double fov_degrees = 45.0,
                               double margin = 1.1,
                               CharacterVector projection = "perspective") {
    scimesh::Scene scene = build_scene_from_r(scene_data);
    scimesh::Vec3 dir = vec3_from_r(direction);
    scimesh::Vec3 up_vec = vec3_from_r(up);

    std::string proj_str = as<std::string>(projection);
    scimesh::ProjectionType proj = scimesh::ProjectionType::PERSPECTIVE;
    if (proj_str == "orthographic") proj = scimesh::ProjectionType::ORTHOGRAPHIC;

    scimesh::Camera cam = scimesh::camera_fit_scene(
        scene, dir, up_vec,
        static_cast<float>(fov_degrees),
        static_cast<float>(margin),
        proj);

    return camera_to_r_list(cam);
}

// ---- Mesh transforms -------------------------------------------------------
// [[Rcpp::export]]
List scimesh_transform_mesh(List mesh_data, NumericMatrix matrix_4x4) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    scimesh::transform_mesh(mesh, mat4_from_r(matrix_4x4));
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
                                      int segments = 12, bool caps = true) {
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
    scimesh::Mesh mesh = scimesh::generate_multi_cylinders(s, e, r, col, segments, caps);
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_generate_tube(NumericMatrix path, double radius, int segments,
                           NumericVector color, bool cap_start = true,
                           bool cap_end = true) {
    std::vector<scimesh::Vec3> pts;
    pts.reserve(path.nrow());
    for (int i = 0; i < path.nrow(); i++) {
        pts.push_back(scimesh::Vec3(
            static_cast<float>(path(i, 0)),
            static_cast<float>(path(i, 1)),
            static_cast<float>(path(i, 2))));
    }
    scimesh::Color c(
        static_cast<float>(color[0]),
        static_cast<float>(color[1]),
        static_cast<float>(color[2]),
        static_cast<float>(color.size() > 3 ? color[3] : 1.0f));
    scimesh::Mesh mesh = scimesh::generate_tube(
        pts, static_cast<float>(radius), segments, c, cap_start, cap_end);
    return mesh_to_r_list(mesh);
}

// [[Rcpp::export]]
List scimesh_generate_multi_tubes(List paths, NumericVector radii,
                                  NumericMatrix colors, int segments = 12,
                                  bool caps = false) {
    int n = paths.size();
    std::vector<std::vector<scimesh::Vec3>> all_paths;
    std::vector<float> r;
    std::vector<scimesh::Color> col;
    all_paths.reserve(n);
    for (int i = 0; i < n; i++) {
        NumericMatrix m = paths[i];
        std::vector<scimesh::Vec3> pts;
        pts.reserve(m.nrow());
        for (int j = 0; j < m.nrow(); j++) {
            pts.push_back(scimesh::Vec3(
                static_cast<float>(m(j, 0)),
                static_cast<float>(m(j, 1)),
                static_cast<float>(m(j, 2))));
        }
        all_paths.push_back(pts);
    }
    for (int i = 0; i < n; i++) {
        // Missing radii/colors are recycled from the first entry by the C++
        // generators; only pass what the caller actually provided.
        if (i < radii.size()) r.push_back(static_cast<float>(radii[i]));
        if (colors.nrow() > 0 && i < colors.nrow()) {
            col.push_back(scimesh::Color(
                static_cast<float>(colors(i, 0)),
                static_cast<float>(colors(i, 1)),
                static_cast<float>(colors(i, 2)),
                static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
        }
    }
    scimesh::Mesh mesh = scimesh::generate_multi_tubes(all_paths, r, col, segments, caps);
    return mesh_to_r_list(mesh);
}

// ---- Splines --------------------------------------------------------------
// [[Rcpp::export]]
NumericMatrix scimesh_catmull_rom_path(NumericMatrix points,
                                       int samples_per_segment = 8,
                                       bool closed = false,
                                       double alpha = 0.5) {
    std::vector<scimesh::Vec3> path = scimesh::catmull_rom_path(
        vec3_path_from_r(points), samples_per_segment, closed,
        static_cast<float>(alpha));
    return vec3_path_to_r(path);
}

// [[Rcpp::export]]
NumericMatrix scimesh_bspline_path(NumericMatrix points,
                                   int samples_per_segment = 8,
                                   bool closed = false) {
    std::vector<scimesh::Vec3> path = scimesh::bspline_path(
        vec3_path_from_r(points), samples_per_segment, closed);
    return vec3_path_to_r(path);
}

// [[Rcpp::export]]
NumericMatrix scimesh_bezier_path(NumericMatrix control_points,
                                  int samples = 64) {
    std::vector<scimesh::Vec3> path =
        scimesh::bezier_path(vec3_path_from_r(control_points), samples);
    return vec3_path_to_r(path);
}

// [[Rcpp::export]]
NumericMatrix scimesh_resample_path(NumericMatrix path, double step,
                                    bool closed = false) {
    std::vector<scimesh::Vec3> resampled = scimesh::resample_by_arclength(
        vec3_path_from_r(path), static_cast<float>(step), closed);
    return vec3_path_to_r(resampled);
}

// [[Rcpp::export]]
double scimesh_path_length(NumericMatrix path, bool closed = false) {
    return static_cast<double>(
        scimesh::path_length(vec3_path_from_r(path), closed));
}

// [[Rcpp::export]]
NumericVector scimesh_path_curvature(NumericMatrix path,
                                     bool closed = false) {
    std::vector<float> curvature =
        scimesh::path_curvature(vec3_path_from_r(path), closed);
    return NumericVector(curvature.begin(), curvature.end());
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

// [[Rcpp::export]]
List scimesh_render_lines_raw(NumericMatrix from, NumericMatrix to,
                              NumericMatrix colors, double width,
                              List camera_data, List options_data,
                              bool lit = false) {
    scimesh::Camera cam = build_camera_from_r(camera_data);
    scimesh::RenderOptions opts = build_options_from_r(options_data);

    const int n = std::min(from.nrow(), to.nrow());
    std::vector<scimesh::Vec3> starts, ends;
    std::vector<scimesh::Color> cols;
    starts.reserve(n);
    ends.reserve(n);
    cols.reserve(n);
    for (int i = 0; i < n; i++) {
        starts.push_back(scimesh::Vec3(
            static_cast<float>(from(i, 0)),
            static_cast<float>(from(i, 1)),
            static_cast<float>(from(i, 2))));
        ends.push_back(scimesh::Vec3(
            static_cast<float>(to(i, 0)),
            static_cast<float>(to(i, 1)),
            static_cast<float>(to(i, 2))));
        if (i < colors.nrow()) {
            cols.push_back(scimesh::Color(
                static_cast<float>(colors(i, 0)),
                static_cast<float>(colors(i, 1)),
                static_cast<float>(colors(i, 2)),
                static_cast<float>(colors.ncol() > 3 ? colors(i, 3) : 1.0f)));
        }
    }

    scimesh::Renderer renderer;
    scimesh::Image result;
    if (lit) {
        // The raw renderer draws flat lines; shading has to go through a
        // LineLayer with lit = TRUE, added to a scene.
        scimesh::LineLayer layer;
        layer.from = starts;
        layer.to = ends;
        layer.colors = cols;
        layer.width = static_cast<float>(width);
        layer.lit = true;
        scimesh::Scene scene;
        scene.add_lines(layer);
        result = renderer.render_scene(scene, cam, opts);
    } else {
        result = renderer.render_lines_raw(starts, ends, cols,
                                           static_cast<float>(width), cam, opts);
    }
    return image_to_r_list(result);
}

// ---- Text labels ------------------------------------------------------------

// [[Rcpp::export]]
NumericMatrix scimesh_text_extent(CharacterVector text, double size,
                                  std::string font_file = "",
                                  double line_spacing = 1.2) {
    const int n = text.size();
    NumericMatrix out(n, 5);
    CharacterVector colnames = CharacterVector::create(
        "width", "height", "ascent", "descent", "lines");
    out.attr("dimnames") = List::create(R_NilValue, colnames);
    for (int i = 0; i < n; i++) {
        const scimesh::TextExtent ext = scimesh::measure_text(
            as<std::string>(text[i]), static_cast<float>(size), font_file,
            static_cast<float>(line_spacing));
        out(i, 0) = ext.width;
        out(i, 1) = ext.height;
        out(i, 2) = ext.ascent;
        out(i, 3) = ext.descent;
        out(i, 4) = static_cast<double>(ext.line_count);
    }
    return out;
}

// [[Rcpp::export]]
std::string scimesh_default_font_path() {
    return scimesh::default_font_path();
}

// [[Rcpp::export]]
List scimesh_font_info(std::string font_file, double size) {
    const scimesh::Font font = scimesh::cached_font(font_file, static_cast<float>(size));
    const scimesh::FontMetrics m = font.metrics();
    return List::create(
        _["family"] = font.family_name(),
        _["path"] = font.source_path(),
        _["size"] = font.pixel_size(),
        _["ascent"] = m.ascent,
        _["descent"] = m.descent,
        _["line_gap"] = m.line_gap);
}

// [[Rcpp::export]]
DataFrame scimesh_world_to_screen(NumericMatrix points, List camera_data,
                                 int width, int height, List options_data) {
    scimesh::Camera cam = build_camera_from_r(camera_data);
    scimesh::RenderOptions opts = build_options_from_r(options_data);

    const int n = points.nrow();
    NumericVector x(n), y(n), depth(n);
    LogicalVector in_front(n);
    for (int i = 0; i < n; i++) {
        const scimesh::ProjectedPoint p = scimesh::world_to_screen(
            cam, scimesh::Vec3(static_cast<float>(points(i, 0)),
                               static_cast<float>(points(i, 1)),
                               static_cast<float>(points(i, 2))),
            width, height, opts.projection, opts.near_plane, opts.far_plane);
        x[i] = p.pixel.x;
        y[i] = p.pixel.y;
        depth[i] = p.depth;
        in_front[i] = p.in_front;
    }
    return DataFrame::create(_["x"] = x, _["y"] = y, _["depth"] = depth,
                             _["in_front"] = in_front);
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

// ---- glTF I/O ---------------------------------------------------------------
// [[Rcpp::export]]
void scimesh_write_gltf(List scene_data, CharacterVector path,
                        Nullable<List> camera_data = R_NilValue,
                        String format = "gltf") {
    scimesh::Scene scene = build_scene_from_r(scene_data);
    std::string p = as<std::string>(path);
    std::string fmt = format;

    if (scene.line_count() > 0) {
        Rcpp::warning("glTF does not support scene line layers: %d line layer(s) were skipped. Use render_scene() to draw them.", static_cast<int>(scene.line_count()));
    }

    scimesh::Camera cam_storage;
    scimesh::Camera *cam = nullptr;
    if (camera_data.isNotNull()) {
        List cd(camera_data);
        if (cd.size() > 0) {
            cam_storage = build_camera_from_r(cd);
            cam = &cam_storage;
        }
    }

    if (fmt == "glb") {
        scimesh::gltf_io::write_glb(p, scene, cam);
    } else {
        scimesh::gltf_io::write_gltf(p, scene, cam);
    }
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

// [[Rcpp::export]]
bool scimesh_write_tga(List image, CharacterVector filename,
                       bool use24bit = false) {
    int width = as<int>(image["width"]);
    int height = as<int>(image["height"]);
    RawVector pixels = image["pixels"];

    scimesh::Image img;
    img.width = width;
    img.height = height;
    img.pixels.assign(pixels.begin(), pixels.end());

    return img.write_tga(as<std::string>(filename), use24bit);
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

// [[Rcpp::export]]
List scimesh_image_crop_to_content(List image, CharacterVector direction,
                                    NumericVector background) {
    std::string dir = as<std::string>(direction);
    scimesh::CropContentDirection ccd;
    if (dir == "left")        ccd = scimesh::CropContentDirection::LEFT;
    else if (dir == "right")  ccd = scimesh::CropContentDirection::RIGHT;
    else if (dir == "horizontal") ccd = scimesh::CropContentDirection::HORIZONTAL;
    else if (dir == "top")    ccd = scimesh::CropContentDirection::TOP;
    else if (dir == "bottom") ccd = scimesh::CropContentDirection::BOTTOM;
    else if (dir == "vertical") ccd = scimesh::CropContentDirection::VERTICAL;
    else if (dir == "all")    ccd = scimesh::CropContentDirection::ALL;
    else Rcpp::stop("direction must be 'left', 'right', 'horizontal', 'top', 'bottom', 'vertical', or 'all'");

    if (background.size() != 4) {
        Rcpp::stop("background must be a numeric vector of length 4 (RGBA)");
    }
    scimesh::Image img = r_list_to_image(image);
    scimesh::Color bg(static_cast<float>(background[0]),
                      static_cast<float>(background[1]),
                      static_cast<float>(background[2]),
                      static_cast<float>(background[3]));
    img.crop_to_content(ccd, bg);
    return image_to_r_list(img);
}

// [[Rcpp::export]]
List scimesh_image_apply_contrast(List image, double contrast) {
    scimesh::Image img = r_list_to_image(image);
    img.apply_contrast(static_cast<float>(contrast));
    return image_to_r_list(img);
}

// ---- Print / to-string -------------------------------------------------------
// [[Rcpp::export]]
std::string scimesh_print_image(List image) {
    scimesh::Image img = r_list_to_image(image);
    std::ostringstream os;
    os << img;
    return os.str();
}

// [[Rcpp::export]]
std::string scimesh_print_camera(List camera_data) {
    scimesh::Vec3 eye = vec3_from_r(camera_data["eye"]);
    scimesh::Vec3 center = vec3_from_r(camera_data["center"]);
    scimesh::Vec3 up = vec3_from_r(camera_data["up"]);
    scimesh::Camera cam;
    cam.eye = eye;
    cam.center = center;
    cam.up = up;
    cam.fov_degrees = static_cast<float>(as<double>(camera_data["fov"]));
    std::string proj_str = as<std::string>(camera_data["projection"]);
    cam.projection = (proj_str == "orthographic")
        ? scimesh::ProjectionType::ORTHOGRAPHIC
        : scimesh::ProjectionType::PERSPECTIVE;
    std::ostringstream os;
    os << cam;
    return os.str();
}

// [[Rcpp::export]]
std::string scimesh_print_options(List options_data) {
    scimesh::RenderOptions opts = build_options_from_r(options_data);
    std::ostringstream os;
    os << opts;
    return os.str();
}

// [[Rcpp::export]]
std::string scimesh_print_mesh(List mesh_data) {
    scimesh::Mesh mesh = build_mesh_from_r(mesh_data);
    std::ostringstream os;
    os << mesh;
    return os.str();
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

// ---- Colormap ----------------------------------------------------------------

namespace {

// Convert R numeric vector to scimesh::Color
scimesh::Color colormap_color_from_r(const NumericVector &v) {
    return scimesh::Color(
        static_cast<float>(v.size() > 0 ? v[0] : 0.5),
        static_cast<float>(v.size() > 1 ? v[1] : 0.5),
        static_cast<float>(v.size() > 2 ? v[2] : 0.5),
        static_cast<float>(v.size() > 3 ? v[3] : 1.0));
}

// Convert an ApplyColormapResult to an R list with attributes
List colormap_result_to_r(const scimesh::ApplyColormapResult &r) {
    int n = static_cast<int>(r.colors.size());
    NumericMatrix cols(n, 4);
    for (int i = 0; i < n; i++) {
        cols(i, 0) = r.colors[i].r;
        cols(i, 1) = r.colors[i].g;
        cols(i, 2) = r.colors[i].b;
        cols(i, 3) = r.colors[i].a;
    }

    List out = List::create(Named("colors") = cols);
    out.attr("data_min")   = r.data_min;
    out.attr("data_max")   = r.data_max;
    out.attr("raw_min")    = r.raw_min;
    out.attr("raw_max")    = r.raw_max;
    out.attr("winsor_lo")  = r.winsor_lo;
    out.attr("winsor_hi")  = r.winsor_hi;
    out.attr("nan_count")  = static_cast<int>(r.nan_count);
    return out;
}

} // anonymous namespace

// [[Rcpp::export]]
List scimesh_apply_colormap_single(NumericVector data,
                                    NumericMatrix colormap_lut,
                                    Nullable<NumericVector> limits = R_NilValue,
                                    NumericVector nan_color = NumericVector::create(0.5, 0.5, 0.5, 1.0),
                                    Nullable<NumericVector> winsor_pct = R_NilValue) {
    // Build ColorMap from R matrix (N×3 or N×4)
    scimesh::ColorMap cmap;
    int nl = colormap_lut.nrow();
    int ncol_lut = colormap_lut.ncol();
    cmap.colors.reserve(nl);
    for (int i = 0; i < nl; i++) {
        cmap.colors.emplace_back(
            static_cast<float>(colormap_lut(i, 0)),
            static_cast<float>(ncol_lut > 1 ? colormap_lut(i, 1) : 0.0f),
            static_cast<float>(ncol_lut > 2 ? colormap_lut(i, 2) : 0.0f),
            static_cast<float>(ncol_lut > 3 ? colormap_lut(i, 3) : 1.0f));
    }

    // Convert data
    int nd = data.size();
    std::vector<float> cdata(nd);
    for (int i = 0; i < nd; i++) {
        cdata[i] = static_cast<float>(data[i]);
    }

    // Parse limits
    float vmin = NAN, vmax = NAN;
    if (limits.isNotNull()) {
        NumericVector lim(limits);
        if (lim.size() >= 2) {
            vmin = static_cast<float>(lim[0]);
            vmax = static_cast<float>(lim[1]);
        }
    }

    // Parse nan_color
    scimesh::Color nc = colormap_color_from_r(nan_color);

    // Parse winsor percentiles
    float lo_pct = 0.0f, hi_pct = 100.0f;
    if (winsor_pct.isNotNull()) {
        NumericVector wp(winsor_pct);
        if (wp.size() >= 1) lo_pct = static_cast<float>(wp[0]);
        if (wp.size() >= 2) hi_pct = static_cast<float>(wp[1]);
    }

    auto result = scimesh::apply_colormap(cdata, cmap, vmin, vmax, nc,
                                          lo_pct, hi_pct);
    return colormap_result_to_r(result);
}

// [[Rcpp::export]]
List scimesh_apply_colormap_multi(List data_list,
                                   NumericMatrix colormap_lut,
                                   bool global_range = false,
                                   Nullable<NumericVector> limits = R_NilValue,
                                   NumericVector nan_color = NumericVector::create(0.5, 0.5, 0.5, 1.0),
                                   Nullable<NumericVector> winsor_pct = R_NilValue) {
    // Build ColorMap
    scimesh::ColorMap cmap;
    int nl = colormap_lut.nrow();
    int ncol_lut = colormap_lut.ncol();
    cmap.colors.reserve(nl);
    for (int i = 0; i < nl; i++) {
        cmap.colors.emplace_back(
            static_cast<float>(colormap_lut(i, 0)),
            static_cast<float>(ncol_lut > 1 ? colormap_lut(i, 1) : 0.0f),
            static_cast<float>(ncol_lut > 2 ? colormap_lut(i, 2) : 0.0f),
            static_cast<float>(ncol_lut > 3 ? colormap_lut(i, 3) : 1.0f));
    }

    // Convert data list
    int nds = data_list.size();
    std::vector<std::vector<float>> datasets(nds);
    for (int d = 0; d < nds; d++) {
        NumericVector rv = data_list[d];
        int n = rv.size();
        datasets[d].resize(n);
        for (int i = 0; i < n; i++) {
            datasets[d][i] = static_cast<float>(rv[i]);
        }
    }

    // Parse limits
    float vmin = NAN, vmax = NAN;
    if (limits.isNotNull()) {
        NumericVector lim(limits);
        if (lim.size() >= 2) {
            vmin = static_cast<float>(lim[0]);
            vmax = static_cast<float>(lim[1]);
        }
    }

    scimesh::Color nc = colormap_color_from_r(nan_color);

    float lo_pct = 0.0f, hi_pct = 100.0f;
    if (winsor_pct.isNotNull()) {
        NumericVector wp(winsor_pct);
        if (wp.size() >= 1) lo_pct = static_cast<float>(wp[0]);
        if (wp.size() >= 2) hi_pct = static_cast<float>(wp[1]);
    }

    auto result = scimesh::apply_colormap(datasets, cmap, vmin, vmax, nc,
                                          lo_pct, hi_pct, global_range);

    // Build return list
    List out_list(static_cast<int>(result.per_dataset.size()));
    for (size_t d = 0; d < result.per_dataset.size(); d++) {
        int n = static_cast<int>(result.per_dataset[d].colors.size());
        NumericMatrix cols(n, 4);
        for (int i = 0; i < n; i++) {
            cols(i, 0) = result.per_dataset[d].colors[i].r;
            cols(i, 1) = result.per_dataset[d].colors[i].g;
            cols(i, 2) = result.per_dataset[d].colors[i].b;
            cols(i, 3) = result.per_dataset[d].colors[i].a;
        }
        out_list[static_cast<int>(d)] = cols;
    }

    // Attach pooled metadata as attributes on the list
    out_list.attr("pooled_data_min")  = result.pooled_data_min;
    out_list.attr("pooled_data_max")  = result.pooled_data_max;
    out_list.attr("pooled_raw_min")   = result.pooled_raw_min;
    out_list.attr("pooled_raw_max")   = result.pooled_raw_max;
    out_list.attr("pooled_winsor_lo") = result.pooled_winsor_lo;
    out_list.attr("pooled_winsor_hi") = result.pooled_winsor_hi;
    out_list.attr("total_nan_count")  = static_cast<int>(result.total_nan_count);

    // Attach per-dataset metadata
    NumericVector per_data_min(static_cast<int>(result.per_dataset.size()));
    NumericVector per_data_max(static_cast<int>(result.per_dataset.size()));
    NumericVector per_winsor_lo(static_cast<int>(result.per_dataset.size()));
    NumericVector per_winsor_hi(static_cast<int>(result.per_dataset.size()));
    NumericVector per_nan_count(static_cast<int>(result.per_dataset.size()));
    for (size_t d = 0; d < result.per_dataset.size(); d++) {
        per_data_min[static_cast<int>(d)]  = result.per_dataset[d].data_min;
        per_data_max[static_cast<int>(d)]  = result.per_dataset[d].data_max;
        per_winsor_lo[static_cast<int>(d)] = result.per_dataset[d].winsor_lo;
        per_winsor_hi[static_cast<int>(d)] = result.per_dataset[d].winsor_hi;
        per_nan_count[static_cast<int>(d)] = static_cast<int>(result.per_dataset[d].nan_count);
    }
    out_list.attr("data_ranges")     = List::create(per_data_min, per_data_max);
    out_list.attr("winsor_cutoffs")  = List::create(per_winsor_lo, per_winsor_hi);
    out_list.attr("nan_counts")      = per_nan_count;

    return out_list;
}
