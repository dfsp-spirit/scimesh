#pragma once

#include "types.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"
#include <ostream>
#include <iomanip>
#include <cmath>

namespace scimesh {

// ---- helpers ----

inline const char* str_projection(ProjectionType p) {
    return p == ProjectionType::ORTHOGRAPHIC ? "ortho" : "persp";
}

inline const char* str_shading(ShadingMode s) {
    return s == ShadingMode::FLAT ? "flat" : "smooth";
}

inline const char* str_merge(MergeDirection d) {
    switch (d) {
        case MergeDirection::LEFT:   return "left";
        case MergeDirection::RIGHT:  return "right";
        case MergeDirection::TOP:    return "top";
        case MergeDirection::BOTTOM: return "bottom";
    }
    return "?";
}

inline const char* str_crop(CropContentDirection d) {
    switch (d) {
        case CropContentDirection::LEFT:       return "left";
        case CropContentDirection::RIGHT:      return "right";
        case CropContentDirection::HORIZONTAL: return "horizontal";
        case CropContentDirection::TOP:        return "top";
        case CropContentDirection::BOTTOM:     return "bottom";
        case CropContentDirection::VERTICAL:   return "vertical";
        case CropContentDirection::ALL:        return "all";
    }
    return "?";
}

inline std::string fmt_count(size_t n) {
    if (n >= 1000000) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fM", n / 1000000.0);
        return buf;
    }
    if (n >= 1000) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%uk", static_cast<unsigned>(n / 1000));
        return buf;
    }
    return std::to_string(n);
}

inline std::string fmt_size_bytes(size_t bytes) {
    if (bytes >= 1024 * 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fMB", bytes / (1024.0 * 1024.0));
        return buf;
    }
    if (bytes >= 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%ukB", static_cast<unsigned>(bytes / 1024));
        return buf;
    }
    return std::to_string(bytes) + "B";
}

// ---- primitive types ----

inline std::ostream& operator<<(std::ostream &os, const Color &c) {
    os << "rgba(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const Vec3 &v) {
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const Triangle &t) {
    os << "Tri(" << t.v0 << ", " << t.v1 << ", " << t.v2 << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, ShadingMode s) {
    os << str_shading(s);
    return os;
}

inline std::ostream& operator<<(std::ostream &os, ProjectionType p) {
    os << str_projection(p);
    return os;
}

// ---- composite types ----

inline std::ostream& operator<<(std::ostream &os, const Light &l) {
    os << "Light(pos=" << l.position << ", color=" << l.color
       << ", I=" << l.intensity
       << ", " << (l.is_directional ? "dir" : "point") << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const ClipPlane &cp) {
    os << "ClipPlane(n=" << cp.normal << ", off=" << cp.offset << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const Camera &cam) {
    float dist = glm::length(cam.eye - cam.center);
    os << "Camera(eye=" << cam.eye << ", ctr=" << cam.center
       << ", up=" << cam.up
       << ", " << str_projection(cam.projection)
       << ", fov=" << std::fixed << std::setprecision(1) << cam.fov_degrees << "°"
       << ", dist=" << std::fixed << std::setprecision(1) << dist << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const Mesh &m) {
    os << "Mesh(verts=" << fmt_count(m.vertices.size())
       << ", tris=" << fmt_count(m.triangles.size());
    if (m.has_colors())      os << ", cols";
    if (m.has_normals())     os << ", norms";
    else                     os << ", ~norms";
    if (m.has_uvs())         os << ", uvs";
    if (m.has_texture())     os << ", tex";
    if (m.has_transparency)  os << ", transp";
    if (!m.vertices.empty()) {
        Vec3 bmin, bmax;
        m.compute_bounding_box(bmin, bmax);
        os << ", bb=[" << bmin << ", " << bmax << "]";
    }
    os << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const Scene &s) {
    os << "Scene(meshes=" << s.meshes.size();
    if (!s.meshes.empty()) {
        Vec3 bmin, bmax;
        s.compute_bounding_box(bmin, bmax);
        os << ", bb=[" << bmin << ", " << bmax << "]";
    }
    os << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const Image &img) {
    size_t bytes = img.pixels.size();
    os << "Image(" << img.width << "x" << img.height
       << ", RGBA, " << fmt_size_bytes(bytes) << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream &os, const RenderOptions &opts) {
    os << "RenderOpts(" << opts.width << "x" << opts.height
       << ", " << str_shading(opts.shading);
    if (opts.backface_culling) os << ", cull";
    else                       os << ", ~cull";
    os << ", bg=" << opts.background_color
       << ", lights=" << opts.lights.size()
       << ", amb=" << opts.ambient
       << ", aa=" << opts.aa_samples;
    if (opts.wireframe)    os << ", wire";
    if (opts.fog_enabled)  os << ", fog";
    if (opts.ssao_enabled) os << ", ssao(r=" << opts.ssao_radius
                              << ", I=" << opts.ssao_intensity << ")";
    if (opts.threads > 0)  os << ", threads=" << opts.threads;
    os << ")";
    return os;
}

} // namespace scimesh
