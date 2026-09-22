#include <scimesh/primitives.h>
#include <scimesh/normals.h>
#include <scimesh/math_utils.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>
// <array> and <utility> must be included explicitly: this file uses
// std::array (pyramid/tetrahedron faces) and std::swap.  libstdc++ (GCC, also
// used by R on Windows) pulls both in transitively, but libc++ (clang on
// macOS) does not, which made the package fail to compile there.  Note that
// libc++ forward-declares std::array in <__tuple> (for tuple_size), so
// omitting <array> yields the confusing error "implicit instantiation of
// undefined template 'std::array<...>'" instead of "no member named array".
#include <array>
#include <utility>

namespace scimesh {

static inline void make_basis(const Vec3 &dir, Vec3 &u, Vec3 &v) {
    Vec3 arbitrary = (std::abs(glm::dot(dir, Vec3(0.0f, 1.0f, 0.0f))) < 0.999f)
                         ? Vec3(0.0f, 1.0f, 0.0f)
                         : Vec3(1.0f, 0.0f, 0.0f);
    u = glm::normalize(glm::cross(arbitrary, dir));
    v = glm::cross(dir, u);
}

/// @brief Radius of the i-th primitive of a batched generator.
///
/// The `radii` array may be shorter than the number of primitives; missing
/// entries are recycled from the first one.  A completely empty array means
/// that no radii were given at all, in which case 1.0 is used.  This keeps the
/// batched generators well-defined for any input length instead of reading out
/// of bounds.
static inline float batch_radius(const std::vector<float> &radii, size_t i) {
    if (radii.empty()) {
        return 1.0f;
    }
    return (i < radii.size()) ? radii[i] : radii[0];
}

/// @brief Color of the i-th primitive of a batched generator.
///
/// Same recycling rules as batch_radius(); an empty array means white.
static inline Color batch_color(const std::vector<Color> &colors, size_t i) {
    if (colors.empty()) {
        return Color(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return (i < colors.size()) ? colors[i] : colors[0];
}

/// @brief Vertex and triangle counts of the mesh returned by
///        generate_sphere(center, radius, segments, color).
///
/// Used by generate_multi_spheres() to reserve the exact amount of memory
/// before merging, which avoids repeated reallocation of the growing arrays.
/// The unit tests in cpp_tests/test_primitives.cpp assert that these counts
/// match the actual generator output, so the formulas cannot silently drift
/// out of sync with the generators.
static inline void sphere_geometry_counts(int segments, size_t &num_verts,
                                          size_t &num_tris) {
    const size_t s = static_cast<size_t>(std::max(3, segments));
    // North pole + (s - 1) body rings of s vertices + south pole.
    num_verts = s * (s - 1u) + 2u;
    // North cap (s) + (s - 2) body rings (2 * s each) + south cap (s).
    num_tris = 2u * s + (s - 2u) * 2u * s;
}

/// @brief Vertex and triangle counts of the mesh returned by
///        generate_cylinder(start, end, radius, segments, color, caps).
///
/// Used by generate_multi_cylinders() to reserve the exact amount of memory
/// before merging (see sphere_geometry_counts()).  Each end cap is a triangle
/// fan with a center vertex and a dedicated ring of `segments` vertices.
static inline void cylinder_geometry_counts(int segments, bool caps,
                                            size_t &num_verts,
                                            size_t &num_tris) {
    const size_t s = static_cast<size_t>(std::max(3, segments));
    num_verts = 2u * s + (caps ? 2u * (s + 1u) : 0u);
    num_tris = 2u * s + (caps ? 2u * s : 0u);
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
                       int segments, const Color &color, bool caps) {
    segments = std::max(3, segments);
    Mesh m;

    size_t expected_verts = 0, expected_tris = 0;
    cylinder_geometry_counts(segments, caps, expected_verts, expected_tris);
    m.vertices.reserve(expected_verts);
    m.normals.reserve(expected_verts);
    m.colors.reserve(expected_verts);
    m.triangles.reserve(expected_tris);

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

    if (caps) {
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
    const size_t n = centers.size();
    if (n == 0) {
        return result;
    }

    // Reserve the exact final size up front: every sphere has the same
    // geometry (same `segments`), so the total is n times the per-sphere count.
    size_t verts_per_sphere = 0, tris_per_sphere = 0;
    sphere_geometry_counts(segments, verts_per_sphere, tris_per_sphere);
    result.vertices.reserve(n * verts_per_sphere);
    result.normals.reserve(n * verts_per_sphere);
    result.colors.reserve(n * verts_per_sphere);
    result.triangles.reserve(n * tris_per_sphere);

    for (size_t i = 0; i < n; ++i) {
        float r = batch_radius(radii, i);
        Color c = batch_color(colors, i);
        Mesh sphere = generate_sphere(centers[i], r, segments, c);
        merge_mesh(result, sphere);
    }
    return result;
}

Mesh generate_multi_cylinders(const std::vector<Vec3> &starts,
                              const std::vector<Vec3> &ends,
                              const std::vector<float> &radii,
                              const std::vector<Color> &colors,
                              int segments, bool caps) {
    Mesh result;
    const size_t n = starts.size();
    if (n == 0) {
        return result;
    }

    // Reserve the exact final size up front: every cylinder has the same
    // geometry (same `segments`, same `caps`), so the total is n times the
    // per-cylinder count.
    size_t verts_per_cyl = 0, tris_per_cyl = 0;
    cylinder_geometry_counts(segments, caps, verts_per_cyl, tris_per_cyl);
    result.vertices.reserve(n * verts_per_cyl);
    result.normals.reserve(n * verts_per_cyl);
    result.colors.reserve(n * verts_per_cyl);
    result.triangles.reserve(n * tris_per_cyl);

    for (size_t i = 0; i < n; ++i) {
        float r = batch_radius(radii, i);
        Color c = batch_color(colors, i);
        Mesh cyl = generate_cylinder(starts[i], ends[i], r, segments, c, caps);
        merge_mesh(result, cyl);
    }
    return result;
}

/// @brief Vertex and triangle counts of the mesh returned by
///        generate_tube(path, radius, segments, color, cap_start, cap_end).
///
/// `num_path_points` must be the length of the *cleaned* path (see
/// clean_tube_path()), i.e. after removing consecutive duplicates.
static inline void tube_geometry_counts(size_t num_path_points, int segments,
                                        bool cap_start, bool cap_end,
                                        size_t &num_verts, size_t &num_tris) {
    const size_t s = static_cast<size_t>(std::max(3, segments));
    if (num_path_points < 2u) {
        num_verts = 0u;
        num_tris = 0u;
        return;
    }
    const size_t rings = num_path_points;
    const size_t sides = rings - 1u;
    num_verts = rings * s;
    num_tris = sides * 2u * s;
    if (cap_start) {
        num_verts += 1u + s;
        num_tris += s;
    }
    if (cap_end) {
        num_verts += 1u + s;
        num_tris += s;
    }
}

/// @brief Remove consecutive duplicate points from a tube path.
///
/// Points that coincide with their predecessor (within a small epsilon) carry
/// no direction and would make the frame construction degenerate, so they are
/// dropped.  The result has at least the first point and every point whose
/// distance to the previously kept point exceeds the epsilon.
static std::vector<Vec3> clean_tube_path(const std::vector<Vec3> &path,
                                         float epsilon = 1e-6f) {
    std::vector<Vec3> cleaned;
    cleaned.reserve(path.size());
    for (const Vec3 &p : path) {
        if (cleaned.empty() || glm::length(p - cleaned.back()) > epsilon) {
            cleaned.push_back(p);
        }
    }
    return cleaned;
}

/// @brief Rotate `v` by the minimal rotation that maps `from_dir` onto `to_dir`.
///
/// Both directions must be unit length.  Used to transport the tube frame from
/// one path point to the next (parallel transport / rotation-minimizing frame),
/// which keeps the tube from twisting around its own axis.
static inline Vec3 rotate_between(const Vec3 &v, const Vec3 &from_dir,
                                  const Vec3 &to_dir) {
    const Vec3 axis = glm::cross(from_dir, to_dir);
    const float axis_len = glm::length(axis);
    const float cos_angle = glm::dot(from_dir, to_dir);

    if (axis_len < 1e-6f) {
        if (cos_angle > 0.0f) {
            return v;   // parallel: nothing to do.
        }
        // Anti-parallel (180 degrees).  The rotation axis is arbitrary but has
        // to be perpendicular to the direction; Rodrigues reduces to a mirror.
        const Vec3 helper = (std::abs(from_dir.x) < 0.9f) ? Vec3(1.0f, 0.0f, 0.0f)
                                                         : Vec3(0.0f, 1.0f, 0.0f);
        const Vec3 perp = glm::normalize(glm::cross(from_dir, helper));
        return -v + 2.0f * glm::dot(v, perp) * perp;
    }

    // Rodrigues' rotation formula, with sin = |cross| and cos = dot for unit
    // input vectors.
    const Vec3 k = axis / axis_len;
    return v * cos_angle + glm::cross(k, v) * axis_len +
           k * (glm::dot(k, v) * (1.0f - cos_angle));
}

/// @brief Compute the unit tangent at every point of a cleaned tube path.
///
/// Interior points use the direction of the neighbouring points (a mitered
/// joint), the first and last point use the adjacent segment direction.
static std::vector<Vec3> tube_path_tangents(const std::vector<Vec3> &pts) {
    const size_t n = pts.size();
    std::vector<Vec3> tangents(n);
    for (size_t i = 0; i < n; ++i) {
        Vec3 dir;
        if (i == 0) {
            dir = pts[1] - pts[0];
        } else if (i + 1 == n) {
            dir = pts[n - 1] - pts[n - 2];
        } else {
            dir = pts[i + 1] - pts[i - 1];
        }
        const float len = glm::length(dir);
        // Note: glm::normalize() rather than dir / len, so that the geometry of
        // a 2-point tube is bit-identical to the one of generate_cylinder().
        tangents[i] = (len > 1e-8f) ? glm::normalize(dir) : tangents[i - 1];
    }
    return tangents;
}

Mesh generate_tube(const std::vector<Vec3> &path, float radius, int segments,
                   const Color &color, bool cap_start, bool cap_end) {
    segments = std::max(3, segments);
    Mesh m;

    const std::vector<Vec3> pts = clean_tube_path(path);
    if (pts.size() < 2u) {
        return m;   // a single point (or none) has no direction to sweep along.
    }

    const size_t num_rings = pts.size();
    const size_t s = static_cast<size_t>(segments);

    size_t expected_verts = 0, expected_tris = 0;
    tube_geometry_counts(num_rings, segments, cap_start, cap_end,
                         expected_verts, expected_tris);
    m.vertices.reserve(expected_verts);
    m.normals.reserve(expected_verts);
    m.colors.reserve(expected_verts);
    m.triangles.reserve(expected_tris);

    const std::vector<Vec3> tangents = tube_path_tangents(pts);
    const float step = glm::two_pi<float>() / static_cast<float>(segments);

    // Build one ring per path point.  The first frame comes from an arbitrary
    // but stable basis; all following frames are transported from the previous
    // one, which avoids the twisting that a per-point basis would introduce.
    Vec3 u, v;
    make_basis(tangents[0], u, v);

    std::vector<Vec3> ring_basis_u(num_rings), ring_basis_v(num_rings);
    ring_basis_u[0] = u;
    ring_basis_v[0] = v;
    for (size_t i = 1; i < num_rings; ++i) {
        Vec3 transported = rotate_between(ring_basis_u[i - 1], tangents[i - 1],
                                          tangents[i]);
        // Re-orthogonalize against the new tangent to fight float drift.
        transported = transported - glm::dot(transported, tangents[i]) * tangents[i];
        const float len = glm::length(transported);
        if (len < 1e-6f) {
            // The transported vector collapsed (can happen for very sharp
            // turns).  Fall back to a fresh basis for this ring.
            make_basis(tangents[i], ring_basis_u[i], ring_basis_v[i]);
            continue;
        }
        ring_basis_u[i] = transported / len;
        ring_basis_v[i] = glm::cross(tangents[i], ring_basis_u[i]);
    }

    for (size_t i = 0; i < num_rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const float a = step * static_cast<float>(j);
            const Vec3 radial = std::cos(a) * ring_basis_u[i] +
                                std::sin(a) * ring_basis_v[i];
            m.vertices.push_back(pts[i] + radius * radial);
            m.normals.push_back(radial);
            m.colors.push_back(color);
        }
    }

    // Side quads (same winding as the cylinder body of generate_cylinder()).
    for (size_t i = 0; i + 1u < num_rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const uint32_t a = static_cast<uint32_t>(i * s + static_cast<size_t>(j));
            const uint32_t b = static_cast<uint32_t>(i * s + static_cast<size_t>((j + 1) % segments));
            const uint32_t c = static_cast<uint32_t>((i + 1u) * s + static_cast<size_t>((j + 1) % segments));
            const uint32_t d = static_cast<uint32_t>((i + 1u) * s + static_cast<size_t>(j));
            m.triangles.push_back({a, b, c});
            m.triangles.push_back({a, c, d});
        }
    }

    // Start cap: outward-facing normal is -tangent of the first ring.
    if (cap_start) {
        const uint32_t cap_offset = static_cast<uint32_t>(m.vertices.size());
        const Vec3 cap_normal = -tangents[0];
        m.vertices.push_back(pts[0]);
        m.normals.push_back(cap_normal);
        m.colors.push_back(color);
        for (int j = 0; j < segments; ++j) {
            const float a = step * static_cast<float>(j);
            const Vec3 radial = std::cos(a) * ring_basis_u[0] +
                                std::sin(a) * ring_basis_v[0];
            m.vertices.push_back(pts[0] + radius * radial);
            m.normals.push_back(cap_normal);
            m.colors.push_back(color);
        }
        for (int j = 0; j < segments; ++j) {
            const uint32_t center_idx = cap_offset;
            const uint32_t edge_idx = cap_offset + 1u + static_cast<uint32_t>(j);
            const uint32_t next_edge_idx =
                cap_offset + 1u + static_cast<uint32_t>((j + 1) % segments);
            m.triangles.push_back({center_idx, next_edge_idx, edge_idx});
        }
    }

    // End cap: outward-facing normal is +tangent of the last ring.
    if (cap_end) {
        const size_t last = num_rings - 1u;
        const uint32_t cap_offset = static_cast<uint32_t>(m.vertices.size());
        const Vec3 cap_normal = tangents[last];
        m.vertices.push_back(pts[last]);
        m.normals.push_back(cap_normal);
        m.colors.push_back(color);
        for (int j = 0; j < segments; ++j) {
            const float a = step * static_cast<float>(j);
            const Vec3 radial = std::cos(a) * ring_basis_u[last] +
                                std::sin(a) * ring_basis_v[last];
            m.vertices.push_back(pts[last] + radius * radial);
            m.normals.push_back(cap_normal);
            m.colors.push_back(color);
        }
        for (int j = 0; j < segments; ++j) {
            const uint32_t center_idx = cap_offset;
            const uint32_t edge_idx = cap_offset + 1u + static_cast<uint32_t>(j);
            const uint32_t next_edge_idx =
                cap_offset + 1u + static_cast<uint32_t>((j + 1) % segments);
            m.triangles.push_back({center_idx, edge_idx, next_edge_idx});
        }
    }

    return m;
}

Mesh generate_multi_tubes(const std::vector<std::vector<Vec3>> &paths,
                          const std::vector<float> &radii,
                          const std::vector<Color> &colors,
                          int segments, bool caps) {
    Mesh result;
    const size_t n = paths.size();
    if (n == 0) {
        return result;
    }

    // Unlike spheres/cylinders, tubes can differ in length, so the exact total
    // has to be summed over the paths.  Cleaning the paths here duplicates a
    // little work in generate_tube(), but keeps the reservation exact.
    size_t total_verts = 0, total_tris = 0;
    for (const auto &path : paths) {
        size_t path_verts = 0, path_tris = 0;
        tube_geometry_counts(clean_tube_path(path).size(), segments, caps, caps,
                             path_verts, path_tris);
        total_verts += path_verts;
        total_tris += path_tris;
    }
    result.vertices.reserve(total_verts);
    result.normals.reserve(total_verts);
    result.colors.reserve(total_verts);
    result.triangles.reserve(total_tris);

    for (size_t i = 0; i < n; ++i) {
        const float r = batch_radius(radii, i);
        const Color c = batch_color(colors, i);
        merge_mesh(result, generate_tube(paths[i], r, segments, c, caps, caps));
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
