#include "renderer.h"
#include "math_utils.h"
#include "normals.h"
#include "clipping.h"
#include "rasterizer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scimesh {

Image Renderer::render_mesh(const Mesh &mesh, const Camera &camera, const RenderOptions &options) {
    Image output(options.width, options.height);
    render_pipeline({&mesh}, camera, options, output);
    return output;
}

Image Renderer::render_scene(const Scene &scene, const Camera &camera, const RenderOptions &options) {
    Image output(options.width, options.height);
    std::vector<const Mesh *> mesh_ptrs;
    for (const auto &m : scene.meshes) {
        mesh_ptrs.push_back(&m);
    }
    render_pipeline(mesh_ptrs, camera, options, output);
    return output;
}

void Renderer::render_pipeline(const std::vector<const Mesh *> &meshes,
                               const Camera &camera,
                               const RenderOptions &options,
                               Image &output) {
    // 1. Clear image with background color
    output.clear_float(options.background_color.r, options.background_color.g,
                       options.background_color.b, options.background_color.a);

    // 2. Initialize rasterizer
    Rasterizer rasterizer(options.width, options.height);
    rasterizer.clear(1.0f);

    // 3. Compute view and projection matrices
    Mat4 view = camera.get_view_matrix();
    float aspect = static_cast<float>(options.width) / static_cast<float>(options.height);
    Mat4 projection = camera.get_projection_matrix(aspect, options.near_plane, options.far_plane);
    Mat4 view_projection = projection * view;

    // 4. Compute light direction in view space (camera-aligned "headlight")
    // The light points in the same direction the camera looks (forward = -Z in view space)
    Vec3 light_direction = Vec3(0.0f, 0.0f, -1.0f);

    // 5. For each mesh
    for (const auto *mesh_ptr : meshes) {
        const Mesh &mesh = *mesh_ptr;

        if (mesh.empty())
            continue;

        // 5a. Get or compute normals
        std::vector<Vec3> computed_normals;
        const std::vector<Vec3> *normals_ptr;
        if (mesh.has_normals()) {
            normals_ptr = &mesh.normals;
        } else {
            compute_vertex_normals(mesh, computed_normals);
            normals_ptr = &computed_normals;
        }

        // 5b. Transform normals to view space (rotation part of view matrix)
        // For lighting, we use view-space normals
        std::vector<Vec3> view_normals(normals_ptr->size());
        for (size_t i = 0; i < normals_ptr->size(); ++i) {
            Vec3 n = (*normals_ptr)[i];
            if (options.invert_normals)
                n = -n;
            view_normals[i] = glm::normalize(transform_direction(view, n));
        }

        // 5c. For each triangle
        for (const auto &tri : mesh.triangles) {
            // Get vertex positions
            Vec3 v0 = mesh.vertices[tri.v0];
            Vec3 v1 = mesh.vertices[tri.v1];
            Vec3 v2 = mesh.vertices[tri.v2];

            // Get colors (or use default)
            Color c0 = mesh.has_colors() ? mesh.colors[tri.v0] : options.default_color;
            Color c1 = mesh.has_colors() ? mesh.colors[tri.v1] : options.default_color;
            Color c2 = mesh.has_colors() ? mesh.colors[tri.v2] : options.default_color;

            // Get view-space normals
            Vec3 n0 = view_normals[tri.v0];
            Vec3 n1 = view_normals[tri.v1];
            Vec3 n2 = view_normals[tri.v2];

            // Transform to clip space
            ClipVertex cv0, cv1, cv2;
            cv0.position = transform_point_homogeneous(view_projection, v0);
            cv0.color = c0;
            cv0.normal = n0;

            cv1.position = transform_point_homogeneous(view_projection, v1);
            cv1.color = c1;
            cv1.normal = n1;

            cv2.position = transform_point_homogeneous(view_projection, v2);
            cv2.color = c2;
            cv2.normal = n2;

            // Near-plane clipping
            std::vector<ClipVertex> clipped_vertices;
            std::vector<Triangle> clipped_triangles;
            int num_clipped = clip_triangle_near_plane(cv0, cv1, cv2, clipped_vertices, clipped_triangles);

            if (num_clipped == 0)
                continue;

            // For each clipped triangle
            for (const auto &ct : clipped_triangles) {
                const ClipVertex &cv_a = clipped_vertices[ct.v0];
                const ClipVertex &cv_b = clipped_vertices[ct.v1];
                const ClipVertex &cv_c = clipped_vertices[ct.v2];

                // Perspective divide (clip space -> NDC)
                Vec3 ndc0 = perspective_divide(cv_a.position);
                Vec3 ndc1 = perspective_divide(cv_b.position);
                Vec3 ndc2 = perspective_divide(cv_c.position);

                // NDC -> screen coordinates
                float sx0, sy0, sz0, sx1, sy1, sz1, sx2, sy2, sz2;
                ndc_to_screen(ndc0, options.width, options.height, sx0, sy0, sz0);
                ndc_to_screen(ndc1, options.width, options.height, sx1, sy1, sz1);
                ndc_to_screen(ndc2, options.width, options.height, sx2, sy2, sz2);

                Vec3 screen_v0(sx0, sy0, sz0);
                Vec3 screen_v1(sx1, sy1, sz1);
                Vec3 screen_v2(sx2, sy2, sz2);

                bool smooth = (options.shading == ShadingMode::SMOOTH);

                // For flat shading, compute face normal in view space
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

                rasterizer.rasterize_triangle(
                    screen_v0, cv_a.color, normal_a,
                    screen_v1, cv_b.color, normal_b,
                    screen_v2, cv_c.color, normal_c,
                    options.backface_culling,
                    smooth,
                    light_direction,
                    output);
            }
        }
    }
}

} // namespace scimesh
