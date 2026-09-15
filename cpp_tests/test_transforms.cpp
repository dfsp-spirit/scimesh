// Mesh transformations (translate, scale, rotate, matrix transform) and the
// conversion from raw FreeSurfer vertex/face arrays.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/transforms.h>
#include <scimesh/normals.h>
#include <scimesh/mesh.h>
#include <scimesh/math_utils.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using Catch::Approx;

namespace {

Vec3 centroid(const Mesh &mesh) {
    Vec3 sum(0.0f);
    for (const auto &v : mesh.vertices) sum += v;
    return sum / static_cast<float>(mesh.vertices.size());
}

// Diagonal of the axis-aligned bounding box - a rotation/translation invariant
// measure of a mesh's size.
float mesh_bbox_extent_size(const Mesh &mesh) {
    if (mesh.vertices.empty()) return 0.0f;
    Vec3 lower = mesh.vertices[0];
    Vec3 upper = mesh.vertices[0];
    for (const auto &v : mesh.vertices) {
        lower = Vec3(std::min(lower.x, v.x), std::min(lower.y, v.y), std::min(lower.z, v.z));
        upper = Vec3(std::max(upper.x, v.x), std::max(upper.y, v.y), std::max(upper.z, v.z));
    }
    return glm::length(upper - lower);
}

float max_coordinate(const Mesh &mesh, int axis) {
    float m = mesh.vertices[0][axis];
    for (const auto &v : mesh.vertices) m = std::max(m, v[axis]);
    return m;
}

float min_coordinate(const Mesh &mesh, int axis) {
    float m = mesh.vertices[0][axis];
    for (const auto &v : mesh.vertices) m = std::min(m, v[axis]);
    return m;
}

bool all_unit_length(const std::vector<Vec3> &normals) {
    for (const auto &n : normals) {
        if (std::abs(glm::length(n) - 1.0f) > 1e-4f) return false;
    }
    return true;
}

} // namespace

TEST_CASE("translate_mesh moves all vertices", "[transforms]") {
    Mesh mesh = make_unit_cube();
    const Vec3 before = centroid(mesh);
    REQUIRE(before.x == Approx(0.0f).margin(1e-6f));

    translate_mesh(mesh, Vec3(2.0f, -3.0f, 0.5f));

    const Vec3 after = centroid(mesh);
    REQUIRE(after.x == Approx(2.0f));
    REQUIRE(after.y == Approx(-3.0f));
    REQUIRE(after.z == Approx(0.5f));

    // A rigid translation must not change the extent of the mesh.
    REQUIRE(mesh_bbox_extent_size(mesh) == Approx(2.0f * std::sqrt(3.0f)).margin(1e-4f));
}

TEST_CASE("scale_mesh scales around the origin, uniformly or per axis",
          "[transforms]") {
    Mesh uniform = make_unit_cube();
    scale_mesh(uniform, 2.0f);
    REQUIRE(uniform.vertices[0].x == Approx(2.0f * make_unit_cube().vertices[0].x));
    REQUIRE(mesh_bbox_extent_size(uniform) == Approx(2.0f * mesh_bbox_extent_size(make_unit_cube())));

    Mesh per_axis = make_unit_cube();
    scale_mesh(per_axis, Vec3(1.0f, 2.0f, 3.0f));
    // The unit cube spans [-1, 1] on every axis.
    REQUIRE(mesh_bbox_extent_size(per_axis) ==
            Approx(std::sqrt(4.0f + 16.0f + 36.0f)).margin(1e-3f));
    REQUIRE(max_coordinate(per_axis, 0) == Approx(1.0f));
    REQUIRE(max_coordinate(per_axis, 1) == Approx(2.0f));
    REQUIRE(max_coordinate(per_axis, 2) == Approx(3.0f));
}

TEST_CASE("rotate_mesh rotates around the given axis", "[transforms]") {
    Mesh mesh = make_unit_cube();
    const float quarter_turn = 3.14159265358979f / 2.0f;

    // 90 degrees about Z maps +X to +Y: the bounding box stays [-1, 1] on both
    // axes (the corners are just permuted), so check the extremes per axis.
    rotate_mesh(mesh, quarter_turn, Vec3(0.0f, 0.0f, 1.0f));

    REQUIRE(max_coordinate(mesh, 0) == Approx(1.0f).margin(1e-5f));
    REQUIRE(max_coordinate(mesh, 1) == Approx(1.0f).margin(1e-5f));
    REQUIRE(min_coordinate(mesh, 0) == Approx(-1.0f).margin(1e-5f));
    REQUIRE(min_coordinate(mesh, 1) == Approx(-1.0f).margin(1e-5f));

    // Rotation preserves the bounding-box diagonal.
    REQUIRE(mesh_bbox_extent_size(mesh) == Approx(2.0f * std::sqrt(3.0f)).margin(1e-4f));

    // Rotating twice by the same angle gives the same result as rotating once
    // by the doubled angle.
    Mesh twice = make_unit_cube();
    rotate_mesh(twice, quarter_turn, Vec3(0.0f, 1.0f, 0.0f));
    rotate_mesh(twice, quarter_turn, Vec3(0.0f, 1.0f, 0.0f));
    Mesh once = make_unit_cube();
    rotate_mesh(once, 2.0f * quarter_turn, Vec3(0.0f, 1.0f, 0.0f));
    for (size_t i = 0; i < once.vertices.size(); ++i) {
        REQUIRE(twice.vertices[i].x == Approx(once.vertices[i].x).margin(1e-5f));
        REQUIRE(twice.vertices[i].z == Approx(once.vertices[i].z).margin(1e-5f));
    }
}

TEST_CASE("transform_mesh applies an arbitrary 4x4 matrix", "[transforms]") {
    Mesh mesh = make_unit_cube();

    // Row-major convention: translation lives in the last column (M * p).
    Mat4 m = glm::translate(Mat4(1.0f), Vec3(1.0f, 2.0f, 3.0f));
    transform_mesh(mesh, m);
    const Vec3 center = centroid(mesh);
    REQUIRE(center.x == Approx(1.0f));
    REQUIRE(center.y == Approx(2.0f));
    REQUIRE(center.z == Approx(3.0f));

    // A combined rotate + scale matrix (the case that used to be applied
    // transposed).
    Mesh combined = make_unit_cube();
    Mat4 rs = glm::rotate(Mat4(1.0f), 3.14159265358979f, Vec3(0.0f, 1.0f, 0.0f));
    rs = glm::scale(rs, Vec3(1.0f, 1.0f, 2.0f));
    transform_mesh(combined, rs);
    // Rotating the unit cube by 180 degrees about Y leaves the bounding box
    // axis-aligned, with the Z extent doubled by the scale.
    REQUIRE(mesh_bbox_extent_size(combined) ==
            Approx(std::sqrt(4.0f + 4.0f + 16.0f)).margin(1e-3f));

    // Normals follow the rotation (they are transformed with the inverse
    // transpose, which for a rotation is the rotation itself).
    Mesh shaded = make_unit_cube();
    shaded.normals.assign(shaded.vertices.size(), Vec3(1.0f, 0.0f, 0.0f));
    rotate_mesh(shaded, 3.14159265358979f / 2.0f, Vec3(0.0f, 0.0f, 1.0f));
    for (const auto &n : shaded.normals) {
        REQUIRE(n.x == Approx(0.0f).margin(1e-5f));
        REQUIRE(n.y == Approx(1.0f).margin(1e-5f));
        REQUIRE(n.z == Approx(0.0f).margin(1e-5f));
    }
    REQUIRE(all_unit_length(shaded.normals));
}

TEST_CASE("scale_mesh keeps normals perpendicular to the surface", "[transforms]") {
    // A non-uniform scale needs the inverse transpose: scaling the normal by the
    // same factors would tilt it away from the surface.
    Mesh mesh = make_unit_cube();
    mesh.normals.assign(mesh.vertices.size(), Vec3(1.0f, 1.0f, 0.0f));

    scale_mesh(mesh, Vec3(1.0f, 2.0f, 4.0f));

    // n' = normalize(n / s) = normalize(1, 0.5, 0) = (0.894, 0.447, 0)
    const Vec3 expected = glm::normalize(Vec3(1.0f, 0.5f, 0.0f));
    for (const auto &n : mesh.normals) {
        REQUIRE(n.x == Approx(expected.x).margin(1e-5f));
        REQUIRE(n.y == Approx(expected.y).margin(1e-5f));
        REQUIRE(n.z == Approx(0.0f).margin(1e-5f));
    }
    REQUIRE(all_unit_length(mesh.normals));

    // A uniform scale leaves directions unchanged ...
    Mesh uniform = make_unit_cube();
    uniform.normals.assign(uniform.vertices.size(), Vec3(0.0f, 0.0f, 1.0f));
    scale_mesh(uniform, 3.0f);
    REQUIRE(uniform.normals[0].z == Approx(1.0f));

    // ... and mirroring flips the normals (inverse of a negative scale).
    Mesh mirrored = make_unit_cube();
    mirrored.normals.assign(mirrored.vertices.size(), Vec3(1.0f, 0.0f, 0.0f));
    scale_mesh(mirrored, Vec3(-1.0f, 1.0f, 1.0f));
    REQUIRE(mirrored.normals[0].x == Approx(-1.0f));
}

TEST_CASE("translate_mesh leaves normals alone", "[transforms]") {
    Mesh mesh = make_unit_cube();
    const Vec3 normal(0.0f, 1.0f, 0.0f);
    mesh.normals.assign(mesh.vertices.size(), normal);

    translate_mesh(mesh, Vec3(5.0f, -2.0f, 7.0f));

    REQUIRE(mesh.normals[0].x == Approx(normal.x));
    REQUIRE(mesh.normals[0].y == Approx(normal.y));
    REQUIRE(mesh.normals[0].z == Approx(normal.z));
}

TEST_CASE("transform_mesh updates normals and survives singular matrices",
          "[transforms]") {
    Mesh mesh = make_unit_cube();
    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f, 0.0f, 1.0f));

    // Uniform scale + translation: the normal direction is unchanged.
    transform_mesh(mesh, glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f)) *
                            glm::scale(Mat4(1.0f), Vec3(3.0f)));
    REQUIRE(mesh.normals[0].z == Approx(1.0f));
    REQUIRE(all_unit_length(mesh.normals));

    // Non-uniform scale: the normal tilts to stay perpendicular.
    Mesh squashed = make_unit_cube();
    squashed.normals.assign(squashed.vertices.size(), Vec3(1.0f, 1.0f, 0.0f));
    transform_mesh(squashed, glm::scale(Mat4(1.0f), Vec3(1.0f, 4.0f, 1.0f)));
    const Vec3 expected = glm::normalize(Vec3(1.0f, 0.25f, 0.0f));
    REQUIRE(squashed.normals[0].x == Approx(expected.x).margin(1e-5f));
    REQUIRE(squashed.normals[0].y == Approx(expected.y).margin(1e-5f));

    // A singular matrix (zero scale) must not produce NaNs.
    Mesh flattened = make_unit_cube();
    flattened.normals.assign(flattened.vertices.size(), Vec3(0.0f, 0.0f, 1.0f));
    transform_mesh(flattened, glm::scale(Mat4(1.0f), Vec3(1.0f, 0.0f, 1.0f)));
    for (const auto &n : flattened.normals) {
        REQUIRE(std::isfinite(n.x));
        REQUIRE(std::isfinite(n.y));
        REQUIRE(std::isfinite(n.z));
    }

    // A mesh without normals is transformed without complaints.
    Mesh bare = make_unit_cube();
    transform_mesh(bare, glm::scale(Mat4(1.0f), Vec3(2.0f)));
    REQUIRE_FALSE(bare.has_normals());
}

TEST_CASE("mesh_from_fs converts raw FreeSurfer arrays", "[transforms][freesurfer]") {
    // Two triangles forming a quad, in the flat array layout FreeSurfer uses.
    const std::vector<float> vertices = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f};
    const std::vector<uint32_t> faces = {0, 1, 2, 0, 2, 3};

    Mesh mesh = mesh_from_fs(vertices, faces);
    REQUIRE(mesh.vertices.size() == 4);
    REQUIRE(mesh.triangles.size() == 2);
    REQUIRE(mesh.vertices[2].y == Approx(1.0f));
    REQUIRE(mesh.triangles[1].v0 == 0);
    REQUIRE(mesh.triangles[1].v2 == 3);
    REQUIRE_FALSE(mesh.has_colors());

    // Per-vertex morphometry values: NaN marks the medial wall and must become
    // white, other vertices keep the color from the RGB bytes.
    const std::vector<float> morph = {1.0f, std::nanf(""), 2.0f, 3.0f};
    const std::vector<uint8_t> rgb = {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        10, 20, 30};
    Mesh colored = mesh_from_fs(vertices, faces, morph, rgb);
    REQUIRE(colored.has_colors());
    REQUIRE(colored.colors.size() == 4);
    REQUIRE(colored.colors[0].r == Approx(1.0f));  // from RGB bytes
    REQUIRE(colored.colors[1].r == Approx(1.0f));  // NaN -> white
    REQUIRE(colored.colors[1].g == Approx(1.0f));
    REQUIRE(colored.colors[1].b == Approx(1.0f));
    REQUIRE(colored.colors[2].b == Approx(1.0f));  // from RGB bytes

    // RGB bytes alone also work.
    Mesh rgb_only = mesh_from_fs(vertices, faces, {}, rgb);
    REQUIRE(rgb_only.has_colors());
    REQUIRE(rgb_only.colors[3].r == Approx(10.0f / 255.0f));
    REQUIRE(rgb_only.colors[3].g == Approx(20.0f / 255.0f));
    REQUIRE(rgb_only.colors[3].b == Approx(30.0f / 255.0f));
}

TEST_CASE("mesh_from_fs marks NaN data as transparent on request",
          "[transforms][freesurfer]") {
    const std::vector<float> vertices = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f};
    const std::vector<uint32_t> faces = {0, 1, 2, 0, 2, 3};
    const std::vector<float> morph = {1.0f, std::nanf(""), 2.0f, 3.0f};
    const std::vector<uint8_t> rgb(4 * 3, 200);

    // Default: NaN vertices are opaque white and nothing is marked transparent.
    Mesh opaque = mesh_from_fs(vertices, faces, morph, rgb, false);
    REQUIRE(opaque.colors[1].a == Approx(1.0f));
    REQUIRE_FALSE(opaque.has_transparency);

    // detect_transparency: NaN vertices become holes (alpha 0) and the mesh is
    // flagged so the renderer blends it.
    Mesh transparent = mesh_from_fs(vertices, faces, morph, rgb, true);
    REQUIRE(transparent.colors[1].a == Approx(0.0f));
    REQUIRE(transparent.has_transparency);
    REQUIRE(transparent.colors[0].a == Approx(1.0f));  // valid data stays opaque
    REQUIRE(transparent.colors[0].r == Approx(200.0f / 255.0f));

    // Without per-vertex values there is nothing to detect.
    Mesh no_values = mesh_from_fs(vertices, faces, {}, rgb, true);
    REQUIRE_FALSE(no_values.has_transparency);
}
