#include "renderer.h"
#include "math_utils.h"
#include "normals.h"
#include "clipping.h"
#include "rasterizer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

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

    Mat4 view = camera.get_view_matrix();
    float aspect = static_cast<float>(output.width) / static_cast<float>(output.height);
    Mat4 projection = camera.get_projection_matrix(aspect, options.near_plane, options.far_plane);
    Mat4 view_projection = projection * view;

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

        for (const auto &tri : mesh.triangles) {
            Vec3 v0 = mesh.vertices[tri.v0];
            Vec3 v1 = mesh.vertices[tri.v1];
            Vec3 v2 = mesh.vertices[tri.v2];

            Color c0 = mesh.has_colors() ? mesh.colors[tri.v0] : options.default_color;
            Color c1 = mesh.has_colors() ? mesh.colors[tri.v1] : options.default_color;
            Color c2 = mesh.has_colors() ? mesh.colors[tri.v2] : options.default_color;

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

            std::vector<ClipVertex> clipped_vertices;
            std::vector<Triangle> clipped_triangles;
            int num_clipped = clip_triangle_near_plane(cv0, cv1, cv2,
                clipped_vertices, clipped_triangles);
            if (num_clipped == 0) continue;

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
