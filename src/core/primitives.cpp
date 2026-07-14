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

    int n_rings = std::max(2, segments);
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

    // North cap — outward-facing triangles (CCW from above)
    for (int j = 0; j < verts_per_ring; ++j) {
        uint32_t j_next = 1 + static_cast<uint32_t>((j + 1) % verts_per_ring);
        m.triangles.push_back({north_idx, j_next, 1 + static_cast<uint32_t>(j)});
    }

    // Body rings
    for (int ring = 0; ring < n_rings - 2; ++ring) {
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

    // South cap — outward-facing triangles (CCW from below)
    uint32_t last_start =
        1 + static_cast<uint32_t>(n_rings - 2) * verts_per_ring;
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
    Vec3 uu, vv;
    make_basis(dir, uu, vv);

    // Pre-calculate the radial vectors and positions for the rings
    std::vector<Vec3> radials(segments);
    std::vector<Vec3> bottom_ring(segments);
    std::vector<Vec3> top_ring(segments);

    float step = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float a = step * static_cast<float>(i);
        radials[i] = std::cos(a) * uu + std::sin(a) * vv;
        bottom_ring[i] = start + radius * radials[i];
        top_ring[i] = end + radius * radials[i];
    }

    // --- 1. THE BODY (Smooth shading radially) ---
    for (int i = 0; i < segments; ++i) {
        m.vertices.push_back(bottom_ring[i]);
        m.normals.push_back(radials[i]);
        m.colors.push_back(color);
    }
    for (int i = 0; i < segments; ++i) {
        m.vertices.push_back(top_ring[i]);
        m.normals.push_back(radials[i]);
        m.colors.push_back(color);
    }

    for (int i = 0; i < segments; ++i) {
        uint32_t a = i;
        uint32_t b = (i + 1) % segments;
        uint32_t c = segments + (i + 1) % segments;
        uint32_t d = segments + i;

        // FIXED: Swapped from {a, c, b} to {a, b, c} for CCW outward facing
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    }

    // --- 2. BOTTOM CAP (Flat shading, normal = -dir) ---
    uint32_t bottom_cap_offset = static_cast<uint32_t>(m.vertices.size());

    // Add the bottom center vertex
    m.vertices.push_back(start);
    m.normals.push_back(-dir);
    m.colors.push_back(color);

    // Add dedicated edge vertices for the bottom cap
    for (int i = 0; i < segments; ++i) {
        m.vertices.push_back(bottom_ring[i]);
        m.normals.push_back(-dir); // Shared flat normal
        m.colors.push_back(color);
    }

    for (int i = 0; i < segments; ++i) {
        uint32_t center_idx = bottom_cap_offset;
        uint32_t edge_idx = bottom_cap_offset + 1 + i;
        uint32_t next_edge_idx = bottom_cap_offset + 1 + ((i + 1) % segments);

        // Reversed CCW winding to face outward from the bottom
        m.triangles.push_back({center_idx, next_edge_idx, edge_idx});
    }

    // --- 3. TOP CAP (Flat shading, normal = +dir) ---
    uint32_t top_cap_offset = static_cast<uint32_t>(m.vertices.size());

    // Add the top center vertex
    m.vertices.push_back(end);
    m.normals.push_back(dir);
    m.colors.push_back(color);

    // Add dedicated edge vertices for the top cap
    for (int i = 0; i < segments; ++i) {
        m.vertices.push_back(top_ring[i]);
        m.normals.push_back(dir); // Shared flat normal
        m.colors.push_back(color);
    }

    for (int i = 0; i < segments; ++i) {
        uint32_t center_idx = top_cap_offset;
        uint32_t edge_idx = top_cap_offset + 1 + i;
        uint32_t next_edge_idx = top_cap_offset + 1 + ((i + 1) % segments);

        // Standard CCW winding to face outward from the top
        m.triangles.push_back({center_idx, edge_idx, next_edge_idx});
    }

    return m;
}


Mesh generate_cone(const Vec3 &base, const Vec3 &tip, float radius,
                   int segments, const Color &color) {
    segments = std::max(3, segments);
    Mesh m;

    Vec3 dir_vec = tip - base;
    float height = glm::length(dir_vec);

    // Prevent division by zero if base and tip are identical
    Vec3 dir = (height > 1e-8f) ? (dir_vec / height) : Vec3(0, 1, 0);

    Vec3 uu, vv;
    make_basis(dir, uu, vv);

    float step = glm::two_pi<float>() / static_cast<float>(segments);

    // --- 1. THE BODY (Smooth shading radially, duplicated tip) ---
    uint32_t body_offset = 0;
    for (int i = 0; i < segments; ++i) {
        float a = step * static_cast<float>(i);
        Vec3 radial = std::cos(a) * uu + std::sin(a) * vv;

        // Calculate the mathematically perfect sloped normal for the cone surface
        Vec3 slope_normal = glm::normalize(radial * height + dir * radius);

        // Base ring vertex
        m.vertices.push_back(base + radius * radial);
        m.normals.push_back(slope_normal);
        m.colors.push_back(color);

        // Tip vertex (Duplicated for this specific slice to maintain the sloped normal)
        m.vertices.push_back(tip);
        m.normals.push_back(slope_normal);
        m.colors.push_back(color);
    }

    for (int i = 0; i < segments; ++i) {
        uint32_t base_idx = body_offset + i * 2;
        uint32_t tip_idx = body_offset + i * 2 + 1;
        uint32_t next_base_idx = body_offset + ((i + 1) % segments) * 2;

        // Outward facing CCW
        m.triangles.push_back({base_idx, next_base_idx, tip_idx});
    }

    // --- 2. BOTTOM CAP (Flat shading, normal = -dir) ---
    uint32_t cap_offset = static_cast<uint32_t>(m.vertices.size());

    // Add the bottom center vertex
    m.vertices.push_back(base);
    m.normals.push_back(-dir);
    m.colors.push_back(color);

    // Add dedicated edge vertices for the bottom cap
    for (int i = 0; i < segments; ++i) {
        float a = step * static_cast<float>(i);
        Vec3 radial = std::cos(a) * uu + std::sin(a) * vv;

        m.vertices.push_back(base + radius * radial);
        m.normals.push_back(-dir); // Shared flat normal pointing down
        m.colors.push_back(color);
    }

    for (int i = 0; i < segments; ++i) {
        uint32_t center_idx = cap_offset;
        uint32_t edge_idx = cap_offset + 1 + i;
        uint32_t next_edge_idx = cap_offset + 1 + ((i + 1) % segments);

        // Reversed CCW winding to face outward from the bottom
        m.triangles.push_back({center_idx, next_edge_idx, edge_idx});
    }

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

    // 6 faces * 4 vertices = 24 distinct vertices
    m.vertices.reserve(24);
    m.normals.reserve(24);
    m.colors.assign(24, color);
    m.triangles.reserve(12);

    float x = half.x, y = half.y, z = half.z;

    // The 8 unique spatial positions (kept exactly as you had them)
    Vec3 v[8] = {
        center + Vec3(-x, -y, -z), // 0: Bottom-Left-Back
        center + Vec3( x, -y, -z), // 1: Bottom-Right-Back
        center + Vec3( x,  y, -z), // 2: Top-Right-Back
        center + Vec3(-x,  y, -z), // 3: Top-Left-Back
        center + Vec3(-x, -y,  z), // 4: Bottom-Left-Front
        center + Vec3( x, -y,  z), // 5: Bottom-Right-Front
        center + Vec3( x,  y,  z), // 6: Top-Right-Front
        center + Vec3(-x,  y,  z), // 7: Top-Left-Front
    };

    // Define the 6 faces using the 8 positions, plus the perfect normal for that face
    struct Face {
        int v0, v1, v2, v3;
        Vec3 normal;
    };

    Face faces[6] = {
        {4, 5, 6, 7, Vec3( 0,  0,  1)}, // Front
        {1, 0, 3, 2, Vec3( 0,  0, -1)}, // Back
        {0, 1, 5, 4, Vec3( 0, -1,  0)}, // Bottom
        {7, 6, 2, 3, Vec3( 0,  1,  0)}, // Top
        {1, 2, 6, 5, Vec3( 1,  0,  0)}, // Right
        {0, 4, 7, 3, Vec3(-1,  0,  0)}  // Left
    };

    uint32_t index = 0;
    for (int i = 0; i < 6; i++) {
        // Push the 4 distinct vertices for this face
        m.vertices.push_back(v[faces[i].v0]);
        m.vertices.push_back(v[faces[i].v1]);
        m.vertices.push_back(v[faces[i].v2]);
        m.vertices.push_back(v[faces[i].v3]);

        // Push the perfectly flat normal 4 times
        for (int j = 0; j < 4; j++) {
            m.normals.push_back(faces[i].normal);
        }

        // Create the two triangles for this quad using our new unrolled indices
        m.triangles.push_back({index, index + 1, index + 2});
        m.triangles.push_back({index, index + 2, index + 3});

        index += 4;
    }

    return m;
}


Mesh generate_pyramid(const Vec3 &base_center, const Vec3 &apex,
                           float hw, const Color &color) {
    Mesh m;

    // 4 sides (3 verts each) + 1 square base (4 verts) = 16 distinct vertices
    m.vertices.reserve(16);
    m.normals.reserve(16);
    m.colors.assign(16, color);
    m.triangles.reserve(6);

    // Define the base corner positions
    Vec3 p0 = base_center + Vec3(-hw, 0, -hw); // Back-Left
    Vec3 p1 = base_center + Vec3( hw, 0, -hw); // Back-Right
    Vec3 p2 = base_center + Vec3( hw, 0,  hw); // Front-Right
    Vec3 p3 = base_center + Vec3(-hw, 0,  hw); // Front-Left

    // 1. GENERATE THE 4 SIDE FACES (Unrolled for sharp edges)
    std::vector<std::array<Vec3, 3>> side_faces = {
        {p1, p0, apex}, // Back side
        {p2, p1, apex}, // Right side
        {p3, p2, apex}, // Front side
        {p0, p3, apex}  // Left side
    };

    uint32_t index = 0;
    for (const auto& face : side_faces) {
        Vec3 A = face[0], B = face[1], C = face[2];

        // Calculate exact perpendicular normal for this side
        Vec3 normal = glm::normalize(glm::cross(B - A, C - A));

        // Push 3 dedicated vertices for this face
        m.vertices.push_back(A);
        m.vertices.push_back(B);
        m.vertices.push_back(C);

        m.normals.push_back(normal);
        m.normals.push_back(normal);
        m.normals.push_back(normal);

        m.triangles.push_back({index, index + 1, index + 2});
        index += 3;
    }

    // 2. GENERATE THE BASE QUAD (4 shared vertices, identical straight-down normal)
    Vec3 base_normal = Vec3(0.0f, -1.0f, 0.0f);

    m.vertices.push_back(p0); // index 12
    m.vertices.push_back(p1); // index 13
    m.vertices.push_back(p2); // index 14
    m.vertices.push_back(p3); // index 15

    for (int i = 0; i < 4; i++) {
        m.normals.push_back(base_normal);
    }

    // Two CCW triangles forming the square base (viewed from below)
    m.triangles.push_back({index,     index + 2, index + 3}); // {p0, p2, p3}
    m.triangles.push_back({index,     index + 1, index + 2}); // {p0, p1, p2}

    return m;
}

Mesh generate_tetrahedron(const Vec3 &p0, const Vec3 &p1,
                               const Vec3 &p2, const Vec3 &p3,
                               const Color &color) {
    Mesh m;

    // 4 faces * 3 vertices per face = 12 distinct vertices
    m.vertices.reserve(12);
    m.normals.reserve(12);
    m.colors.assign(12, color);
    m.triangles.reserve(4);

    Vec3 centroid = (p0 + p1 + p2 + p3) * 0.25f;

    std::vector<std::array<Vec3, 3>> faces = {
        {p0, p1, p2}, {p0, p2, p3}, {p0, p3, p1}, {p1, p3, p2}
    };

    uint32_t index = 0;
    for (const auto& face : faces) {
        Vec3 A = face[0], B = face[1], C = face[2];
        Vec3 normal = glm::normalize(glm::cross(B - A, C - A));

        // Flip normal and vertex order if facing inward
        if (glm::dot(normal, A - centroid) < 0.0f) {
            normal = -normal;
            std::swap(B, C);
        }

        // Push 3 distinct vertices for this specific face
        m.vertices.push_back(A);
        m.vertices.push_back(B);
        m.vertices.push_back(C);

        // All 3 vertices share the exact same perpendicular face normal
        m.normals.push_back(normal);
        m.normals.push_back(normal);
        m.normals.push_back(normal);

        m.triangles.push_back({index, index + 1, index + 2});
        index += 3;
    }

    return m;
}


Mesh generate_torus(const Vec3 &center, float R, float r,
                    int seg_major, int seg_minor, const Color &color) {
    Mesh m;

    // We can pre-allocate memory for slight performance gains
    int total_vertices = seg_major * seg_minor;
    m.vertices.reserve(total_vertices);
    m.normals.reserve(total_vertices);
    m.colors.reserve(total_vertices);
    m.triangles.reserve(total_vertices * 2);

    // 1. Generate Vertices, Colors, and Normals
    for (int i = 0; i < seg_major; i++) {
        float phi = glm::two_pi<float>() * float(i) / float(seg_major);
        float cp = std::cos(phi), sp = std::sin(phi);

        // Calculate the center of the current tube ring (minor center)
        Vec3 minor_center = center + Vec3(R * cp, 0.0f, R * sp);

        for (int j = 0; j < seg_minor; j++) {
            float theta = glm::two_pi<float>() * float(j) / float(seg_minor);
            float ct = std::cos(theta), st = std::sin(theta);

            float px = (R + r * ct) * cp;
            float py = r * st;
            float pz = (R + r * ct) * sp;

            Vec3 pos = center + Vec3(px, py, pz);
            m.vertices.push_back(pos);
            m.colors.push_back(color);

            // The normal is simply the direction from the tube's center to the vertex
            m.normals.push_back(glm::normalize(pos - minor_center));
        }
    }

    // 2. Generate Triangles (with fixed, outward-facing winding order)
    for (int i = 0; i < seg_major; i++) {
        int ni = (i + 1) % seg_major;

        for (int j = 0; j < seg_minor; j++) {
            int nj = (j + 1) % seg_minor;

            uint32_t a = i * seg_minor + j;      // Current ring, current slice
            uint32_t b = ni * seg_minor + j;     // Next ring, current slice
            uint32_t c = ni * seg_minor + nj;    // Next ring, next slice
            uint32_t d = i * seg_minor + nj;     // Current ring, next slice

            // FIXED: Flipped winding order so the torus renders from the outside
            m.triangles.push_back({a, d, c});
            m.triangles.push_back({a, c, b});
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

    // Calculate the 4 corner positions
    Vec3 p0 = center - u * hx - v * hy; // Bottom-Left
    Vec3 p1 = center + u * hx - v * hy; // Bottom-Right
    Vec3 p2 = center + u * hx + v * hy; // Top-Right
    Vec3 p3 = center - u * hx + v * hy; // Top-Left

    m.vertices.reserve(8);
    m.normals.reserve(8);
    m.colors.reserve(8);
    m.triangles.reserve(4);

    // --- 1. FRONT FACE (Normal = +n) ---
    m.vertices.insert(m.vertices.end(), {p0, p1, p2, p3});
    for (int i = 0; i < 4; i++) {
        m.normals.push_back(n);
        m.colors.push_back(color);
    }
    // Standard CCW winding for the front
    m.triangles.push_back({0, 1, 2});
    m.triangles.push_back({0, 2, 3});

    // --- 2. BACK FACE (Normal = -n) ---
    m.vertices.insert(m.vertices.end(), {p0, p1, p2, p3});
    for (int i = 0; i < 4; i++) {
        m.normals.push_back(-n);
        m.colors.push_back(color);
    }
    // Reversed CCW winding so the triangles face outward from the back
    m.triangles.push_back({4, 6, 5});
    m.triangles.push_back({4, 7, 6});

    return m;
}

} // namespace scimesh
