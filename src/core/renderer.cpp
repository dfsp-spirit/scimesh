#include <scimesh/renderer.h>
#include <scimesh/math_utils.h>
#include <scimesh/normals.h>
#include <scimesh/clipping.h>
#include <scimesh/rasterizer.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace scimesh {

namespace {

struct DeferredTri {
    Vec3   screen_v0, screen_v1, screen_v2;
    Color  color0, color1, color2;
    Vec3   normal0, normal1, normal2;
    Vec2   uv0, uv1, uv2;
    bool   smooth;
    /// Centroid depth in view space: the camera looks down -Z, so a *smaller*
    /// (more negative) value is farther away.
    float  view_z;
};

/// A translucent line segment deferred to the blended pass (see DeferredTri).
struct DeferredLine {
    Vec3  screen_v0, screen_v1;
    Color color0, color1;
    float width;
    bool  lit;
    /// Midpoint depth in view space (see DeferredTri::view_z).
    float view_z;
};

/// @brief Signed distance of a view-space point to the near clipping plane.
///
/// The camera looks down -Z, so points with `z <= -near_plane` are visible and
/// the signed distance is positive inside the frustum.
inline float signed_distance_near_plane(const Vec3 &p_view, float near_plane) {
    return -near_plane - p_view.z;
}

/// @brief Signed distance of a view-space point to a (view-space) clip plane.
///
/// Matches the convention of the triangle clipper: the plane keeps the
/// half-space `dot(normal, p) + offset >= 0`.
inline float signed_distance_clip_plane(const Vec3 &p_view, const ClipPlane &plane) {
    return glm::dot(plane.normal, p_view) + plane.offset;
}

/// @brief Clip a segment against the near plane and the user clip planes.
///
/// Works on the parameter interval of the segment: returns the sub-interval
/// [t0, t1] (in [0, 1]) for which every point is inside all planes.  Returns
/// false when nothing of the segment is visible, in which case the segment has
/// to be skipped completely.
///
/// @param v0, v1      View-space endpoints of the segment.
/// @param near_plane  Near plane distance of the camera.
/// @param clip_planes User clip planes, already converted to view space.
/// @param[out] t0, t1 The visible parameter interval.
/// @return True if any part of the segment is visible.
bool clip_segment_view(const Vec3 &v0, const Vec3 &v1, float near_plane,
                       const std::vector<ClipPlane> &clip_planes,
                       float &t0, float &t1) {
    t0 = 0.0f;
    t1 = 1.0f;

    // Clip against a single plane given the signed distances of the endpoints.
    auto clip_against = [&](float d0, float d1) -> bool {
        const bool inside0 = d0 >= 0.0f;
        const bool inside1 = d1 >= 0.0f;
        if (!inside0 && !inside1) {
            return false;
        }
        if (inside0 && inside1) {
            return true;
        }
        const float denom = d0 - d1;
        if (std::abs(denom) < 1e-12f) {
            return true;
        }
        const float t = d0 / denom;
        if (inside0) {
            t1 = std::min(t1, t);
        } else {
            t0 = std::max(t0, t);
        }
        return t0 <= t1;
    };

    if (!clip_against(signed_distance_near_plane(v0, near_plane),
                      signed_distance_near_plane(v1, near_plane))) {
        return false;
    }
    for (const ClipPlane &plane : clip_planes) {
        if (!clip_against(signed_distance_clip_plane(v0, plane),
                          signed_distance_clip_plane(v1, plane))) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

Image Renderer::render_mesh(const Mesh &mesh, const Camera &camera, const RenderOptions &options) {
    if (options.width <= 0 || options.height <= 0) {
        throw std::invalid_argument("RenderOptions width and height must be > 0");
    }
    int aa = std::max(1, options.aa_samples);
    Image internal(options.width * aa, options.height * aa);
    std::vector<SceneNodeRef> nodes;
    nodes.push_back({&mesh, Mat4(1.0f), ""});
    render_pipeline(nodes, {}, camera, options, internal);
    return internal.downsample_box(aa);
}

Image Renderer::render_scene(const Scene &scene, const Camera &camera, const RenderOptions &options) {
    if (options.width <= 0 || options.height <= 0) {
        throw std::invalid_argument("RenderOptions width and height must be > 0");
    }
    int aa = std::max(1, options.aa_samples);
    Image internal(options.width * aa, options.height * aa);
    std::vector<SceneNodeRef> nodes = scene.nodes();
    render_pipeline(nodes, scene.line_nodes(), camera, options, internal);
    return internal.downsample_box(aa);
}

Image Renderer::render_triangles_raw(const std::vector<Vec3> &positions,
                                     const std::vector<Color> &colors,
                                     const Camera &camera,
                                     const RenderOptions &options) {
    Mesh mesh;
    mesh.vertices = positions;
    mesh.colors = colors;
    int nv = static_cast<int>(positions.size());
    for (int i = 0; i < nv / 3; i++) {
        mesh.triangles.push_back({
            static_cast<uint32_t>(i * 3),
            static_cast<uint32_t>(i * 3 + 1),
            static_cast<uint32_t>(i * 3 + 2)});
    }
    for (const auto &c : colors) {
        if (c.a < 1.0f - 1e-6f) {
            mesh.has_transparency = true;
            break;
        }
    }
    return render_mesh(mesh, camera, options);
}

Image Renderer::render_points_raw(const std::vector<Vec3> &positions,
                                  const std::vector<Color> &colors,
                                  float radius,
                                  const Camera &camera,
                                  const RenderOptions &options) {
    if (options.width <= 0 || options.height <= 0) {
        throw std::invalid_argument("RenderOptions width and height must be > 0");
    }
    int np = static_cast<int>(positions.size());
    if (np == 0 || colors.empty()) {
        Image img(options.width, options.height);
        img.clear_float(options.background_color.r, options.background_color.g,
                        options.background_color.b, options.background_color.a);
        return img;
    }

    int aa = std::max(1, options.aa_samples);
    Image output(options.width * aa, options.height * aa);
    output.clear_float(options.background_color.r, options.background_color.g,
                       options.background_color.b, options.background_color.a);

    Rasterizer rasterizer(output.width, output.height);
    rasterizer.clear(1.0f);
    rasterizer.specular_color = options.specular_color;
    rasterizer.shininess = options.shininess;
    rasterizer.lights = options.lights;
    rasterizer.ambient = options.ambient;
    rasterizer.fog_enabled = options.fog_enabled;
    rasterizer.fog_start = options.fog_start;
    rasterizer.fog_end = options.fog_end;
    rasterizer.fog_color = options.fog_color;
    rasterizer.fog_space = options.fog_space;
    rasterizer.z_near = options.near_plane;
    rasterizer.z_far = options.far_plane;
    rasterizer.orthographic = (options.projection == ProjectionType::ORTHOGRAPHIC);
    rasterizer.ssao_enabled = options.ssao_enabled;
    rasterizer.ssao_radius = options.ssao_radius;
    rasterizer.ssao_intensity = options.ssao_intensity;

#ifdef _OPENMP
    if (options.threads > 0) omp_set_num_threads(options.threads);
#endif

    Mat4 view = camera.get_view_matrix();
    float aspect = static_cast<float>(output.width) / static_cast<float>(output.height);
    Camera proj_cam = camera;
    proj_cam.projection = options.projection;
    Mat4 projection = proj_cam.get_projection_matrix(aspect, options.near_plane, options.far_plane);
    Mat4 view_projection = projection * view;

    Vec3 light_direction = Vec3(0.0f, 0.0f, 1.0f);
    if (!options.lights.empty()) {
        light_direction = options.lights[0].position;
        light_direction = glm::normalize(transform_direction(view, light_direction));
    }

    float aa_radius = radius * static_cast<float>(aa);

    for (int i = 0; i < np; i++) {
        Vec4 clip_pos = transform_point_homogeneous(view_projection, positions[i]);
        Vec3 ndc = perspective_divide(clip_pos);
        float sx, sy, sz;
        ndc_to_screen(ndc, output.width, output.height, sx, sy, sz);

        Vec3 normal(0.0f, 0.0f, 1.0f);
        rasterizer.rasterize_point(sx, sy, sz, aa_radius, colors[i],
                                    normal, light_direction, output);
    }

    if (rasterizer.ssao_enabled) {
        rasterizer.apply_ssao(output, options.near_plane, options.far_plane);
    }

    return output.downsample_box(aa);
}

Image Renderer::render_lines_raw(const std::vector<Vec3> &from,
                                 const std::vector<Vec3> &to,
                                 const std::vector<Color> &colors,
                                 float width,
                                 const Camera &camera,
                                 const RenderOptions &options) {
    if (options.width <= 0 || options.height <= 0) {
        throw std::invalid_argument("RenderOptions width and height must be > 0");
    }

    // A raw line render is simply a scene with a single line layer and no
    // meshes, so there is only one code path for line rendering.
    LineLayer layer;
    layer.from = from;
    layer.to = to;
    layer.colors = colors;
    layer.width = width;

    Scene scene;
    scene.add_lines(layer);

    int aa = std::max(1, options.aa_samples);
    Image internal(options.width * aa, options.height * aa);
    std::vector<SceneNodeRef> no_meshes;
    render_pipeline(no_meshes, scene.line_nodes(), camera, options, internal);
    return internal.downsample_box(aa);
}

void Renderer::render_pipeline(const std::vector<SceneNodeRef> &nodes,
                               const std::vector<LineNodeRef> &line_nodes,
                               const Camera &camera,
                               const RenderOptions &options,
                               Image &output) {
    // Validate all non-empty meshes before rendering
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto *mp = nodes[i].mesh;
        if (mp->empty()) continue;          // empty is harmless
        if (!mp->is_valid()) {
            throw std::invalid_argument(
                "Mesh " + std::to_string(i) + " failed validation: "
                "check indices, vertex data, and array sizes");
        }
    }
    output.clear_float(options.background_color.r, options.background_color.g,
                       options.background_color.b, options.background_color.a);

    Rasterizer rasterizer(output.width, output.height);
    rasterizer.clear(1.0f);
    rasterizer.specular_color = options.specular_color;
    rasterizer.shininess = options.shininess;
    rasterizer.lights = options.lights;
    rasterizer.ambient = options.ambient;
    rasterizer.contrast = options.contrast;
    rasterizer.fog_enabled = options.fog_enabled;
    rasterizer.fog_start = options.fog_start;
    rasterizer.fog_end = options.fog_end;
    rasterizer.fog_color = options.fog_color;
    rasterizer.fog_space = options.fog_space;
    rasterizer.z_near = options.near_plane;
    rasterizer.z_far = options.far_plane;
    rasterizer.orthographic = (options.projection == ProjectionType::ORTHOGRAPHIC);
    rasterizer.ssao_enabled = options.ssao_enabled;
    rasterizer.ssao_radius = options.ssao_radius;
    rasterizer.ssao_intensity = options.ssao_intensity;

#ifdef _OPENMP
    if (options.threads > 0) omp_set_num_threads(options.threads);
#endif

    Mat4 view = camera.get_view_matrix();

    for (auto &light : rasterizer.lights) {
        light.position = glm::normalize(transform_direction(view, light.position));
    }
    float aspect = static_cast<float>(output.width) / static_cast<float>(output.height);
    Camera proj_cam = camera;
    proj_cam.projection = options.projection;
    Mat4 projection = proj_cam.get_projection_matrix(aspect, options.near_plane, options.far_plane);
    Mat4 view_projection = projection * view;

    // User clip planes default to world space; clipping itself happens in
    // view space, so convert them here (see ClipPlane::space).
    std::vector<ClipPlane> view_clip_planes;
    view_clip_planes.reserve(options.clip_planes.size());
    for (const auto &cp : options.clip_planes) {
        view_clip_planes.push_back(
            clip_plane_to_view_space(cp, camera.eye, view));
    }

    Vec3 light_direction = Vec3(0.0f, 0.0f, 1.0f);

    std::vector<DeferredTri> deferred;
    std::vector<DeferredLine> deferred_lines;

    for (const auto &node : nodes) {
        const Mesh &mesh = *node.mesh;
        if (mesh.empty()) continue;

        // Translucent meshes are deferred to the blended pass.  This is derived
        // from the mesh's colors (and default_color), so callers no longer have
        // to keep Mesh::has_transparency in sync by hand.
        const bool mesh_transparent = mesh.is_transparent();

        // Placement transform for this mesh (model matrix in world space).
        const Mat4 &model = node.transform;
        const Mat4 view_model = view * model;
        const Mat4 view_proj_model = view_projection * model;

        if (mesh.has_uvs() && mesh.has_texture()) {
            rasterizer.active_texture = const_cast<Image *>(&mesh.texture);
        } else {
            rasterizer.active_texture = nullptr;
        }

        std::vector<Vec3> computed_normals;
        const std::vector<Vec3> *normals_ptr;
        if (mesh.has_normals()) {
            normals_ptr = &mesh.normals;
        } else {
            compute_vertex_normals(mesh, computed_normals);
            normals_ptr = &computed_normals;
        }

        std::vector<Vec3> view_normals(normals_ptr->size());
        for (size_t i = 0; i < normals_ptr->size(); ++i) {
            Vec3 n = (*normals_ptr)[i];
            if (options.invert_normals) n = -n;
            view_normals[i] = glm::normalize(
                transform_direction(view, transform_direction(model, n)));
        }

        for (int ti = 0; ti < static_cast<int>(mesh.triangles.size()); ++ti) {
            const auto &tri = mesh.triangles[ti];
            Vec3 v0 = mesh.vertices[tri.v0];
            Vec3 v1 = mesh.vertices[tri.v1];
            Vec3 v2 = mesh.vertices[tri.v2];

            Color c0, c1, c2;
            if (mesh.has_face_colors()) {
                c0 = c1 = c2 = mesh.face_colors[ti];
            } else if (mesh.has_colors()) {
                c0 = mesh.colors[tri.v0];
                c1 = mesh.colors[tri.v1];
                c2 = mesh.colors[tri.v2];
            } else {
                c0 = c1 = c2 = options.default_color;
            }

            Vec3 n0 = view_normals[tri.v0];
            Vec3 n1 = view_normals[tri.v1];
            Vec3 n2 = view_normals[tri.v2];

            Vec2 uv0 = mesh.has_uvs() ? mesh.uvs[tri.v0] : Vec2(0, 0);
            Vec2 uv1 = mesh.has_uvs() ? mesh.uvs[tri.v1] : Vec2(0, 0);
            Vec2 uv2 = mesh.has_uvs() ? mesh.uvs[tri.v2] : Vec2(0, 0);

            bool tri_transparent = mesh_transparent &&
                (c0.a < 1.0f - 1e-6f || c1.a < 1.0f - 1e-6f || c2.a < 1.0f - 1e-6f);

            ClipVertex cv0, cv1, cv2;
            cv0.position = transform_point_homogeneous(view_proj_model, v0);
            cv0.color = c0; cv0.normal = n0; cv0.uv = uv0;
            cv1.position = transform_point_homogeneous(view_proj_model, v1);
            cv1.color = c1; cv1.normal = n1; cv1.uv = uv1;
            cv2.position = transform_point_homogeneous(view_proj_model, v2);
            cv2.color = c2; cv2.normal = n2; cv2.uv = uv2;

            bool has_user_clips = !view_clip_planes.empty();

            std::vector<ClipVertex> clipped_vertices;
            std::vector<Triangle> clipped_triangles;

            if (has_user_clips) {
                Vec3 vv0 = transform_point(view_model, v0);
                Vec3 vv1 = transform_point(view_model, v1);
                Vec3 vv2 = transform_point(view_model, v2);

                std::vector<ClipVertex> view_clipped;
                std::vector<Triangle> view_clip_tris;
                int vc_count = clip_triangle_view_plane(
                    vv0, vv1, vv2, n0, n1, n2, c0, c1, c2,
                    uv0, uv1, uv2,
                    view_clip_planes[0], view_clipped, view_clip_tris);

                for (int ci = 1; ci < static_cast<int>(view_clip_planes.size()) && vc_count > 0; ++ci) {
                    std::vector<ClipVertex> next_vertices;
                    std::vector<Triangle> next_triangles;
                    for (const auto &vt : view_clip_tris) {
                        const ClipVertex &a = view_clipped[vt.v0];
                        const ClipVertex &b = view_clipped[vt.v1];
                        const ClipVertex &c = view_clipped[vt.v2];
                        Vec3 pa(a.position.x, a.position.y, a.position.z);
                        Vec3 pb(b.position.x, b.position.y, b.position.z);
                        Vec3 pc(c.position.x, c.position.y, c.position.z);
                        Vec3 na(a.normal), nb(b.normal), nc(c.normal);
                        Color ca(a.color), cb(b.color), cc(c.color);
                        Vec2 ua(a.uv), ub(b.uv), uc(c.uv);
                        clip_triangle_view_plane(
                            pa, pb, pc, na, nb, nc, ca, cb, cc,
                            ua, ub, uc,
                            view_clip_planes[ci], next_vertices, next_triangles);
                    }
                    view_clipped.swap(next_vertices);
                    view_clip_tris.swap(next_triangles);
                    vc_count = static_cast<int>(view_clip_tris.size());
                    if (vc_count == 0) break;
                }

                if (vc_count == 0) continue;

                clipped_vertices.clear();
                clipped_triangles.clear();
                for (const auto &vt : view_clip_tris) {
                    const ClipVertex &a = view_clipped[vt.v0];
                    const ClipVertex &b = view_clipped[vt.v1];
                    const ClipVertex &c = view_clipped[vt.v2];

                    ClipVertex cva, cvb, cvc;
                    cva.position = projection * Vec4(a.position.x, a.position.y, a.position.z, 1.0f);
                    cva.color = a.color; cva.normal = a.normal;
                    cvb.position = projection * Vec4(b.position.x, b.position.y, b.position.z, 1.0f);
                    cvb.color = b.color; cvb.normal = b.normal;
                    cvc.position = projection * Vec4(c.position.x, c.position.y, c.position.z, 1.0f);
                    cvc.color = c.color; cvc.normal = c.normal;

                    int nc = clip_triangle_near_plane(cva, cvb, cvc,
                        clipped_vertices, clipped_triangles);
                    (void)nc;
                }
                if (clipped_triangles.empty()) continue;
            } else {
                clipped_vertices.clear();
                clipped_triangles.clear();
                int num_clipped = clip_triangle_near_plane(cv0, cv1, cv2,
                    clipped_vertices, clipped_triangles);
                if (num_clipped == 0) continue;
            }

            bool smooth = (options.shading == ShadingMode::SMOOTH);

            for (const auto &ct : clipped_triangles) {
                const ClipVertex &cv_a = clipped_vertices[ct.v0];
                const ClipVertex &cv_b = clipped_vertices[ct.v1];
                const ClipVertex &cv_c = clipped_vertices[ct.v2];

                Vec3 ndc0 = perspective_divide(cv_a.position);
                Vec3 ndc1 = perspective_divide(cv_b.position);
                Vec3 ndc2 = perspective_divide(cv_c.position);

                float sx0, sy0, sz0, sx1, sy1, sz1, sx2, sy2, sz2;
                ndc_to_screen(ndc0, output.width, output.height, sx0, sy0, sz0);
                ndc_to_screen(ndc1, output.width, output.height, sx1, sy1, sz1);
                ndc_to_screen(ndc2, output.width, output.height, sx2, sy2, sz2);

                Vec3 screen_v0(sx0, sy0, sz0);
                Vec3 screen_v1(sx1, sy1, sz1);
                Vec3 screen_v2(sx2, sy2, sz2);

                Vec3 flat_normal_a, flat_normal_b, flat_normal_c;
                if (!smooth) {
                    Vec3 face_normal = compute_face_normal(
                        transform_point(view_model, mesh.vertices[tri.v0]),
                        transform_point(view_model, mesh.vertices[tri.v1]),
                        transform_point(view_model, mesh.vertices[tri.v2]));
                    flat_normal_a = flat_normal_b = flat_normal_c = face_normal;
                }

                const Vec3 &normal_a = smooth ? cv_a.normal : flat_normal_a;
                const Vec3 &normal_b = smooth ? cv_b.normal : flat_normal_b;
                const Vec3 &normal_c = smooth ? cv_c.normal : flat_normal_c;

                if (tri_transparent) {
                    Vec3 centroid_vs = (transform_point(view_model, v0) +
                                        transform_point(view_model, v1) +
                                        transform_point(view_model, v2)) * (1.0f / 3.0f);
                    deferred.push_back({
                        screen_v0, screen_v1, screen_v2,
                        cv_a.color, cv_b.color, cv_c.color,
                        normal_a, normal_b, normal_c,
                        cv_a.uv, cv_b.uv, cv_c.uv,
                        smooth, centroid_vs.z
                    });
                } else {
                    rasterizer.rasterize_triangle(
                        screen_v0, cv_a.color, normal_a, cv_a.uv,
                        screen_v1, cv_b.color, normal_b, cv_b.uv,
                        screen_v2, cv_c.color, normal_c, cv_c.uv,
                        options.backface_culling, smooth,
                        light_direction,
                        options.wireframe, options.wireframe_color,
                        output);
                }
            }
        }
    }

    // ---- Line layers -------------------------------------------------------
    // Line layers are drawn after the meshes, against the same depth buffer:
    // opaque lines are occluded by meshes in front of them and occlude meshes
    // behind them, translucent lines are deferred to the blended pass below.
    // The output image is supersampled by `aa_samples`, so the screen-space
    // line width has to be scaled by the same factor (like the point radius).
    const float line_width_scale =
        static_cast<float>(output.width) /
        static_cast<float>(std::max(1, options.width));
    for (const auto &line_node : line_nodes) {
        const LineLayer &layer = *line_node.layer;
        if (layer.empty()) {
            continue;
        }

        const Mat4 view_model = view * line_node.transform;
        const float width = std::max(0.5f, layer.width) * line_width_scale;
        const size_t num_segments = layer.size();

        for (size_t i = 0; i < num_segments; ++i) {
            const Vec3 v0 = transform_point(view_model, layer.from[i]);
            const Vec3 v1 = transform_point(view_model, layer.to[i]);

            // Clip against the near plane and the user clip planes.  A segment
            // with an endpoint behind the camera would otherwise project to
            // garbage (w <= 0).
            float t0 = 0.0f, t1 = 1.0f;
            if (!clip_segment_view(v0, v1, options.near_plane, view_clip_planes,
                                   t0, t1)) {
                continue;
            }

            const Vec3 dir = v1 - v0;
            const Vec3 p0 = v0 + t0 * dir;
            const Vec3 p1 = v0 + t1 * dir;

            float sx0, sy0, sz0, sx1, sy1, sz1;
            ndc_to_screen(perspective_divide(transform_point_homogeneous(projection, p0)),
                          output.width, output.height, sx0, sy0, sz0);
            ndc_to_screen(perspective_divide(transform_point_homogeneous(projection, p1)),
                          output.width, output.height, sx1, sy1, sz1);

            // One color per segment: clipping does not change it.
            const Color color = layer.color_or(i, options.default_color);
            const Vec3 screen_v0(sx0, sy0, sz0);
            const Vec3 screen_v1(sx1, sy1, sz1);
            const Vec3 line_normal(0.0f, 0.0f, 1.0f);

            if (color.a < 1.0f - 1e-6f) {
                deferred_lines.push_back({screen_v0, screen_v1, color, color, width,
                                          layer.lit, (p0.z + p1.z) * 0.5f});
            } else {
                rasterizer.rasterize_line(screen_v0, color, screen_v1, color,
                                          width, layer.lit, line_normal,
                                          light_direction, output);
            }
        }
    }

    if (rasterizer.ssao_enabled) {
        rasterizer.apply_ssao(output, options.near_plane, options.far_plane);
    }

    if (!deferred.empty() || !deferred_lines.empty()) {
        // Painter's algorithm: farthest primitive first, nearest last, so that
        // nearer translucent surfaces are blended *over* the ones behind them.
        // Ascending view-space z is farthest-to-nearest (the camera looks down
        // -Z); sorting the other way round blends front-to-back and makes the
        // nearest surface disappear behind the ones behind it.  Triangles and
        // lines are sorted together, so their mutual order is correct too.
        std::sort(deferred.begin(), deferred.end(),
                  [](const DeferredTri &a, const DeferredTri &b) {
                      return a.view_z < b.view_z;
                  });
        std::sort(deferred_lines.begin(), deferred_lines.end(),
                  [](const DeferredLine &a, const DeferredLine &b) {
                      return a.view_z < b.view_z;
                  });

        rasterizer.set_blend_mode(true);

        size_t tri_idx = 0;
        size_t line_idx = 0;
        while (tri_idx < deferred.size() || line_idx < deferred_lines.size()) {
            const bool take_line =
                (tri_idx >= deferred.size()) ||
                (line_idx < deferred_lines.size() &&
                 deferred_lines[line_idx].view_z < deferred[tri_idx].view_z);
            if (take_line) {
                const DeferredLine &dl = deferred_lines[line_idx++];
                rasterizer.rasterize_line(
                    dl.screen_v0, dl.color0, dl.screen_v1, dl.color1,
                    dl.width, dl.lit, Vec3(0.0f, 0.0f, 1.0f), light_direction,
                    output);
            } else {
                const DeferredTri &dt = deferred[tri_idx++];
                rasterizer.rasterize_triangle(
                    dt.screen_v0, dt.color0, dt.normal0, dt.uv0,
                    dt.screen_v1, dt.color1, dt.normal1, dt.uv1,
                    dt.screen_v2, dt.color2, dt.normal2, dt.uv2,
                    options.backface_culling, dt.smooth,
                    light_direction,
                    options.wireframe, options.wireframe_color,
                    output);
            }
        }

        rasterizer.set_blend_mode(false);
    }
}

} // namespace scimesh
