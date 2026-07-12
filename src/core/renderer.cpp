#include "renderer.h"
#include "math_utils.h"
#include "normals.h"
#include "clipping.h"
#include "rasterizer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace scimesh {

namespace {

struct DeferredTri {
    Vec3   screen_v0, screen_v1, screen_v2;
    Color  color0, color1, color2;
    Vec3   normal0, normal1, normal2;
    bool   smooth;
    float  view_z;  // centroid depth in view space, for back-to-front sort
};

} // anonymous namespace

Image Renderer::render_mesh(const Mesh &mesh, const Camera &camera, const RenderOptions &options) {
    int aa = std::max(1, options.aa_samples);
    Image internal(options.width * aa, options.height * aa);
    render_pipeline({&mesh}, camera, options, internal);
    return internal.downsample_box(aa);
}

Image Renderer::render_scene(const Scene &scene, const Camera &camera, const RenderOptions &options) {
    int aa = std::max(1, options.aa_samples);
    Image internal(options.width * aa, options.height * aa);
    std::vector<const Mesh *> mesh_ptrs;
    for (const auto &m : scene.meshes) {
        mesh_ptrs.push_back(&m);
    }
    render_pipeline(mesh_ptrs, camera, options, internal);
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

    return output.downsample_box(aa);
}

void Renderer::render_pipeline(const std::vector<const Mesh *> &meshes,
                               const Camera &camera,
                               const RenderOptions &options,
                               Image &output) {
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

    std::vector<ClipPlane> view_clip_planes = options.clip_planes;
    for (auto &cp : view_clip_planes) {
        cp.normal = glm::normalize(transform_direction(view, cp.normal));
    }

    Vec3 light_direction = Vec3(0.0f, 0.0f, 1.0f);

    bool scene_has_transparency = false;
    for (const auto *mp : meshes) {
        if (mp->has_transparency) { scene_has_transparency = true; break; }
    }

    std::vector<DeferredTri> deferred;

    for (const auto *mesh_ptr : meshes) {
        const Mesh &mesh = *mesh_ptr;
        if (mesh.empty()) continue;

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
            view_normals[i] = glm::normalize(transform_direction(view, n));
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

            bool tri_transparent = scene_has_transparency &&
                (c0.a < 1.0f - 1e-6f || c1.a < 1.0f - 1e-6f || c2.a < 1.0f - 1e-6f);

            ClipVertex cv0, cv1, cv2;
            cv0.position = transform_point_homogeneous(view_projection, v0);
            cv0.color = c0; cv0.normal = n0;
            cv1.position = transform_point_homogeneous(view_projection, v1);
            cv1.color = c1; cv1.normal = n1;
            cv2.position = transform_point_homogeneous(view_projection, v2);
            cv2.color = c2; cv2.normal = n2;

            bool has_user_clips = !view_clip_planes.empty();

            std::vector<ClipVertex> clipped_vertices;
            std::vector<Triangle> clipped_triangles;

            if (has_user_clips) {
                Vec3 vv0 = transform_point(view, v0);
                Vec3 vv1 = transform_point(view, v1);
                Vec3 vv2 = transform_point(view, v2);

                std::vector<ClipVertex> view_clipped;
                std::vector<Triangle> view_clip_tris;
                int vc_count = clip_triangle_view_plane(
                    vv0, vv1, vv2, n0, n1, n2, c0, c1, c2,
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
                        clip_triangle_view_plane(
                            pa, pb, pc, na, nb, nc, ca, cb, cc,
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
                        transform_point(view, mesh.vertices[tri.v0]),
                        transform_point(view, mesh.vertices[tri.v1]),
                        transform_point(view, mesh.vertices[tri.v2]));
                    flat_normal_a = flat_normal_b = flat_normal_c = face_normal;
                }

                const Vec3 &normal_a = smooth ? cv_a.normal : flat_normal_a;
                const Vec3 &normal_b = smooth ? cv_b.normal : flat_normal_b;
                const Vec3 &normal_c = smooth ? cv_c.normal : flat_normal_c;

                if (tri_transparent) {
                    Vec3 centroid_vs = (transform_point(view, v0) +
                                        transform_point(view, v1) +
                                        transform_point(view, v2)) * (1.0f / 3.0f);
                    deferred.push_back({
                        screen_v0, screen_v1, screen_v2,
                        cv_a.color, cv_b.color, cv_c.color,
                        normal_a, normal_b, normal_c,
                        smooth, centroid_vs.z
                    });
                } else {
                    rasterizer.rasterize_triangle(
                        screen_v0, cv_a.color, normal_a,
                        screen_v1, cv_b.color, normal_b,
                        screen_v2, cv_c.color, normal_c,
                        options.backface_culling, smooth,
                        light_direction,
                        options.wireframe, options.wireframe_color,
                        output);
                }
            }
        }
    }

    if (!deferred.empty()) {
        std::sort(deferred.begin(), deferred.end(),
                  [](const DeferredTri &a, const DeferredTri &b) {
                      return a.view_z > b.view_z;
                  });

        rasterizer.set_blend_mode(true);

        for (const auto &dt : deferred) {
            rasterizer.rasterize_triangle(
                dt.screen_v0, dt.color0, dt.normal0,
                dt.screen_v1, dt.color1, dt.normal1,
                dt.screen_v2, dt.color2, dt.normal2,
                options.backface_culling, dt.smooth,
                light_direction,
                options.wireframe, options.wireframe_color,
                output);
        }

        rasterizer.set_blend_mode(false);
    }
}

} // namespace scimesh
