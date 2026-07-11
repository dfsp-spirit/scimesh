#include "primitives.h"
#include "normals.h"
#include "math_utils.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

namespace scimesh {

static inline void make_basis(const Vec3 &dir, Vec3 &u, Vec3 &v) {
    Vec3 arbitrary = (std::abs(glm::dot(dir, Vec3(0.0f, 1.0f, 0.0f))) < 0.999f)
                         ? Vec3(0.0f, 1.0f, 0.0f)
                         : Vec3(1.0f, 0.0f, 0.0f);
    u = glm::normalize(glm::cross(arbitrary, dir));
    v = glm::cross(dir, u);
}

static std::vector<Vec3> make_ring(const Vec3 &center, float radius,
                                   const Vec3 &dir, int segments) {
    Vec3 uu, vv;
    make_basis(dir, uu, vv);
    std::vector<Vec3> result(segments);
    float step = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a = step * static_cast<float>(i);
        result[i] = center + radius * (std::cos(a) * uu + std::sin(a) * vv);
    }
    return result;
}

Mesh generate_sphere(const Vec3 &center, float radius, int segments,
                     const Color &color) {
    segments = std::max(3, segments);
    Mesh m;

    if (segments < 2) {
        return m;
    }

    int n_rings = segments + 1;
    int verts_per_ring = segments;

    uint32_t north_idx = 0;
    m.vertices.push_back(center + Vec3(0.0f, radius, 0.0f));
    m.colors.push_back(color);

    for (int ring = 1; ring < n_rings; ++ring) {
        float phi = glm::pi<float>() * static_cast<float>(ring) /
                    static_cast<float>(segments);
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);

        float step = glm::two_pi<float>() / static_cast<float>(verts_per_ring);
        for (int j = 0; j < verts_per_ring; ++j) {
            float theta = step * static_cast<float>(j);
            m.vertices.push_back(
                center + Vec3(r * std::cos(theta), y, r * std::sin(theta)));
            m.colors.push_back(color);
        }
    }

    uint32_t south_idx = static_cast<uint32_t>(m.vertices.size());
    m.vertices.push_back(center + Vec3(0.0f, -radius, 0.0f));
    m.colors.push_back(color);

    m.normals.resize(m.vertices.size());
    for (size_t i = 0; i < m.vertices.size(); ++i) {
        m.normals[i] = glm::normalize(m.vertices[i] - center);
    }

    for (int j = 0; j < verts_per_ring; ++j) {
        uint32_t j_next = 1 + static_cast<uint32_t>((j + 1) % verts_per_ring);
        m.triangles.push_back({north_idx, 1 + static_cast<uint32_t>(j), j_next});
    }

    for (int ring = 0; ring < segments - 1; ++ring) {
        uint32_t ring_start = 1 + static_cast<uint32_t>(ring) * verts_per_ring;
        uint32_t next_start =
            1 + static_cast<uint32_t>(ring + 1) * verts_per_ring;

        for (int j = 0; j < verts_per_ring; ++j) {
            uint32_t a = ring_start + j;
            uint32_t b = ring_start + (j + 1) % verts_per_ring;
            uint32_t c = next_start + (j + 1) % verts_per_ring;
            uint32_t d = next_start + j;

            m.triangles.push_back({a, b, c});
            m.triangles.push_back({a, c, d});
        }
    }

    uint32_t last_start =
        1 + static_cast<uint32_t>(segments - 1) * verts_per_ring;
    for (int j = 0; j < verts_per_ring; ++j) {
        uint32_t j_next = last_start + (j + 1) % verts_per_ring;
        m.triangles.push_back({south_idx, last_start + j, j_next});
    }

    return m;
}

Mesh generate_cylinder(const Vec3 &start, const Vec3 &end, float radius,
                       int segments, const Color &color) {
    segments = std::max(3, segments);
    Mesh m;

    Vec3 dir = glm::normalize(end - start);

    std::vector<Vec3> ring0 = make_ring(start, radius, dir, segments);
    std::vector<Vec3> ring1 = make_ring(end, radius, dir, segments);

    uint32_t start_c = static_cast<uint32_t>(ring0.size()) * 2;
    m.vertices.insert(m.vertices.end(), ring0.begin(), ring0.end());
    m.vertices.insert(m.vertices.end(), ring1.begin(), ring1.end());
    m.vertices.push_back(start);
    m.vertices.push_back(end);

    m.colors.resize(m.vertices.size(), color);

    for (int j = 0; j < segments; ++j) {
        uint32_t a = j;
        uint32_t b = (j + 1) % segments;
        uint32_t c = segments + (j + 1) % segments;
        uint32_t d = segments + j;

        m.triangles.push_back({a, c, b});
        m.triangles.push_back({a, d, c});
    }

    uint32_t start_center = start_c;
    uint32_t end_center = start_c + 1;
    uint32_t n_segs = static_cast<uint32_t>(segments);
    for (uint32_t j = 0; j < n_segs; ++j) {
        uint32_t j_next = (j + 1) % n_segs;
        m.triangles.push_back({start_center, j, j_next});
    }
    for (uint32_t j = 0; j < n_segs; ++j) {
        uint32_t j_next = (j + 1) % n_segs;
        m.triangles.push_back({end_center, n_segs + j_next, n_segs + j});
    }

    compute_vertex_normals(m, m.normals);
    return m;
}

Mesh generate_cone(const Vec3 &base, const Vec3 &tip, float radius,
                   int segments, const Color &color) {
    segments = std::max(3, segments);
    Mesh m;

    Vec3 dir = glm::normalize(tip - base);
    std::vector<Vec3> ring = make_ring(base, radius, dir, segments);

    uint32_t tip_idx = static_cast<uint32_t>(ring.size());
    uint32_t base_center = tip_idx + 1;

    m.vertices = ring;
    m.vertices.push_back(tip);
    m.vertices.push_back(base);

    m.colors.resize(m.vertices.size(), color);

    uint32_t n_segs = static_cast<uint32_t>(segments);
    for (uint32_t j = 0; j < n_segs; ++j) {
        uint32_t j_next = (j + 1) % n_segs;
        m.triangles.push_back({j, tip_idx, j_next});
    }

    for (uint32_t j = 0; j < n_segs; ++j) {
        uint32_t j_next = (j + 1) % n_segs;
        m.triangles.push_back({base_center, j, j_next});
    }

    compute_vertex_normals(m, m.normals);
    return m;
}

Mesh generate_arrow(const Vec3 &from, const Vec3 &to, float shaft_radius,
                    float head_radius, float head_length, int segments,
                    const Color &color) {
    Vec3 dir_vec = to - from;
    float total_len = glm::length(dir_vec);
    if (total_len < 1e-8f)
        return Mesh();

    Vec3 dir = dir_vec / total_len;
    float hl = std::min(head_length, total_len * 0.8f);

    Vec3 head_base = to - hl * dir;

    Mesh shaft = generate_cylinder(from, head_base, shaft_radius, segments, color);
    Mesh head = generate_cone(head_base, to, head_radius, segments, color);

    merge_mesh(shaft, head);
    return shaft;
}

void merge_mesh(Mesh &dst, const Mesh &src) {
    uint32_t offset = static_cast<uint32_t>(dst.vertices.size());
    dst.vertices.insert(dst.vertices.end(), src.vertices.begin(),
                        src.vertices.end());
    dst.colors.insert(dst.colors.end(), src.colors.begin(), src.colors.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());

    for (const auto &tri : src.triangles) {
        dst.triangles.push_back(
            {tri.v0 + offset, tri.v1 + offset, tri.v2 + offset});
    }
}

Mesh generate_multi_spheres(const std::vector<Vec3> &centers,
                            const std::vector<float> &radii,
                            const std::vector<Color> &colors,
                            int segments) {
    Mesh result;
    size_t n = centers.size();
    for (size_t i = 0; i < n; ++i) {
        float r = (i < radii.size()) ? radii[i] : radii[0];
        Color c = (i < colors.size()) ? colors[i] : colors[0];
        Mesh sphere = generate_sphere(centers[i], r, segments, c);
        merge_mesh(result, sphere);
    }
    return result;
}

Mesh generate_multi_cylinders(const std::vector<Vec3> &starts,
                              const std::vector<Vec3> &ends,
                              const std::vector<float> &radii,
                              const std::vector<Color> &colors,
                              int segments) {
    Mesh result;
    size_t n = starts.size();
    for (size_t i = 0; i < n; ++i) {
        float r = (i < radii.size()) ? radii[i] : radii[0];
        Color c = (i < colors.size()) ? colors[i] : colors[0];
        Mesh cyl = generate_cylinder(starts[i], ends[i], r, segments, c);
        merge_mesh(result, cyl);
    }
    return result;
}

Mesh generate_cuboid(const Vec3 &center, const Vec3 &half,
                     const Color &color) {
    Mesh m;
    float x = half.x, y = half.y, z = half.z;
    Vec3 v[8] = {
        center + Vec3(-x, -y, -z), center + Vec3( x, -y, -z),
        center + Vec3( x,  y, -z), center + Vec3(-x,  y, -z),
        center + Vec3(-x, -y,  z), center + Vec3( x, -y,  z),
        center + Vec3( x,  y,  z), center + Vec3(-x,  y,  z),
    };
    for (int i = 0; i < 8; i++) {
        m.vertices.push_back(v[i]);
        m.colors.push_back(color);
    }
    uint32_t f[12][3] = {
        {0,1,2}, {0,2,3}, {4,6,5}, {4,7,6},
        {0,5,1}, {0,4,5}, {1,6,2}, {1,5,6},
        {2,7,3}, {2,6,7}, {3,4,0}, {3,7,4},
    };
    for (int i = 0; i < 12; i++)
        m.triangles.push_back({f[i][0], f[i][1], f[i][2]});
    return m;
}

Mesh generate_pyramid(const Vec3 &base_center, const Vec3 &apex,
                      float hw, const Color &color) {
    Mesh m;
    m.vertices = {
        base_center + Vec3(-hw, 0, -hw), base_center + Vec3( hw, 0, -hw),
        base_center + Vec3( hw, 0,  hw), base_center + Vec3(-hw, 0,  hw),
        apex,
    };
    for (int i = 0; i < 5; i++) m.colors.push_back(color);
    m.triangles = {
        {0,1,4}, {1,2,4}, {2,3,4}, {3,0,4},
        {0,3,2}, {0,2,1},
    };
    return m;
}

Mesh generate_tetrahedron(const Vec3 &p0, const Vec3 &p1,
                          const Vec3 &p2, const Vec3 &p3,
                          const Color &color) {
    Mesh m;
    m.vertices = {p0, p1, p2, p3};
    for (int i = 0; i < 4; i++) m.colors.push_back(color);
    m.triangles = {{0,1,2}, {0,2,3}, {0,3,1}, {1,3,2}};
    return m;
}

Mesh generate_torus(const Vec3 &center, float R, float r,
                    int seg_major, int seg_minor, const Color &color) {
    Mesh m;
    for (int i = 0; i < seg_major; i++) {
        float phi = glm::two_pi<float>() * float(i) / float(seg_major);
        float cp = std::cos(phi), sp = std::sin(phi);
        for (int j = 0; j < seg_minor; j++) {
            float theta = glm::two_pi<float>() * float(j) / float(seg_minor);
            float ct = std::cos(theta), st = std::sin(theta);
            float px = (R + r * ct) * cp;
            float py = r * st;
            float pz = (R + r * ct) * sp;
            m.vertices.push_back(center + Vec3(px, py, pz));
            m.colors.push_back(color);
        }
    }
    for (int i = 0; i < seg_major; i++) {
        int ni = (i + 1) % seg_major;
        for (int j = 0; j < seg_minor; j++) {
            int nj = (j + 1) % seg_minor;
            uint32_t a = i * seg_minor + j;
            uint32_t b = ni * seg_minor + j;
            uint32_t c = ni * seg_minor + nj;
            uint32_t d = i * seg_minor + nj;
            m.triangles.push_back({a, b, c});
            m.triangles.push_back({a, c, d});
        }
    }
    return m;
}

Mesh generate_plane(const Vec3 &center, const Vec3 &normal,
                    float hx, float hy, const Color &color) {
    Mesh m;
    Vec3 n = glm::length(normal) > 1e-9f ? glm::normalize(normal) : Vec3(0,0,1);
    Vec3 u, v;
    if (std::abs(n.x) < 0.9f) u = glm::normalize(glm::cross(n, Vec3(1,0,0)));
    else u = glm::normalize(glm::cross(n, Vec3(0,1,0)));
    v = glm::cross(n, u);
    m.vertices = {
        center - u * hx - v * hy,
        center + u * hx - v * hy,
        center + u * hx + v * hy,
        center - u * hx + v * hy,
    };
    for (int i = 0; i < 4; i++) m.colors.push_back(color);
    m.triangles = {{0,1,2}, {0,2,3}};
    return m;
}

} // namespace scimesh
