#pragma once

#include "types.h"
#include "camera.h"

namespace scimesh {

enum class ShadingMode {
    SMOOTH,
    FLAT
};

struct RenderOptions {
    int width = 800;
    int height = 600;
    ShadingMode shading = ShadingMode::SMOOTH;
    bool backface_culling = true;
    Color background_color = TRANSPARENT_BLACK;
    Color default_color = DEFAULT_COLOR;
    bool invert_normals = false;
    bool wireframe = false;
    Color wireframe_color = Color(0.0f, 0.0f, 0.0f, 1.0f);
    int aa_samples = 1;
    Color specular_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
    float shininess = 0.0f;
    ProjectionType projection = ProjectionType::PERSPECTIVE;

    std::vector<Light> lights;
    float ambient = 0.3f;
    std::vector<ClipPlane> clip_planes;

    bool fog_enabled = false;
    float fog_start = 0.0f;
    float fog_end = 1.0f;
    Color fog_color = TRANSPARENT_BLACK;

    int threads = 0;

    bool ssao_enabled = false;
    float ssao_radius = 16.0f;
    float ssao_intensity = 0.8f;

    float near_plane = 0.1f;
    float far_plane = 10000.0f;
};

} // namespace scimesh
