#pragma once

#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "render_options.h"
#include "image.h"

namespace scimesh {

class Renderer {
public:
    Image render_mesh(const Mesh &mesh, const Camera &camera, const RenderOptions &options);
    Image render_scene(const Scene &scene, const Camera &camera, const RenderOptions &options);
    Image render_triangles_raw(const std::vector<Vec3> &positions,
                                const std::vector<Color> &colors,
                                const Camera &camera,
                                const RenderOptions &options);

    Image render_points_raw(const std::vector<Vec3> &positions,
                            const std::vector<Color> &colors,
                            float radius,
                            const Camera &camera,
                            const RenderOptions &options);

private:
    void render_pipeline(const std::vector<const Mesh *> &meshes,
                         const Camera &camera,
                         const RenderOptions &options,
                         Image &output);
};

} // namespace scimesh
