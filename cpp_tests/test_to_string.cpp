// String formatting of scimesh types (to_string.h).
//
// The printers are used in log messages, error reports and tests, and several
// of them are the human-readable half of the public API, so every helper and
// every operator<< is exercised here.
#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/to_string.h>
#include <scimesh/types.h>
#include <scimesh/mesh.h>
#include <scimesh/scene.h>
#include <scimesh/camera.h>
#include <scimesh/image.h>
#include <scimesh/render_options.h>
#include <scimesh/transforms.h>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <string>

using namespace scimesh;
using scimesh_test::make_unit_cube;

namespace {

template <typename T>
std::string to_string_of(const T &value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("str_* helpers name every enum value", "[to_string]") {
    // Projection and shading.
    REQUIRE(std::string(str_projection(ProjectionType::PERSPECTIVE)) == "persp");
    REQUIRE(std::string(str_projection(ProjectionType::ORTHOGRAPHIC)) == "ortho");
    REQUIRE(std::string(str_shading(ShadingMode::SMOOTH)) == "smooth");
    REQUIRE(std::string(str_shading(ShadingMode::FLAT)) == "flat");

    // Coordinate spaces (the spaces of clip planes and fog distances).
    REQUIRE(std::string(str_plane_space(PlaneSpace::WORLD)) == "world");
    REQUIRE(std::string(str_plane_space(PlaneSpace::EYE)) == "eye");
    REQUIRE(std::string(str_fog_space(FogSpace::WORLD)) == "world");
    REQUIRE(std::string(str_fog_space(FogSpace::NDC)) == "ndc");

    // Image composition directions.
    REQUIRE(std::string(str_merge(MergeDirection::LEFT)) == "left");
    REQUIRE(std::string(str_merge(MergeDirection::RIGHT)) == "right");
    REQUIRE(std::string(str_merge(MergeDirection::TOP)) == "top");
    REQUIRE(std::string(str_merge(MergeDirection::BOTTOM)) == "bottom");

    REQUIRE(std::string(str_crop(CropContentDirection::LEFT)) == "left");
    REQUIRE(std::string(str_crop(CropContentDirection::RIGHT)) == "right");
    REQUIRE(std::string(str_crop(CropContentDirection::HORIZONTAL)) == "horizontal");
    REQUIRE(std::string(str_crop(CropContentDirection::TOP)) == "top");
    REQUIRE(std::string(str_crop(CropContentDirection::BOTTOM)) == "bottom");
    REQUIRE(std::string(str_crop(CropContentDirection::VERTICAL)) == "vertical");
    REQUIRE(std::string(str_crop(CropContentDirection::ALL)) == "all");
}

TEST_CASE("fmt_count and fmt_size_bytes produce readable numbers", "[to_string]") {
    REQUIRE(fmt_count(0) == "0");
    REQUIRE(fmt_count(42) == "42");
    REQUIRE(fmt_count(1234) == "1k");
    REQUIRE(fmt_count(1234567) == "1.2M");

    REQUIRE(fmt_size_bytes(0) == "0B");
    REQUIRE(fmt_size_bytes(999) == "999B");
    REQUIRE(fmt_size_bytes(2048) == "2kB");
    REQUIRE(fmt_size_bytes(static_cast<size_t>(5 * 1024 * 1024)) == "5.0MB");
}

TEST_CASE("primitive types stream in a parseable form", "[to_string]") {
    REQUIRE(contains(to_string_of(Color(1.0f, 0.5f, 0.0f, 1.0f)), "1"));
    REQUIRE(contains(to_string_of(Vec3(1.0f, 2.0f, 3.0f)), "2"));
    REQUIRE(contains(to_string_of(Triangle{0, 1, 2}), "1"));
    REQUIRE(to_string_of(ShadingMode::FLAT) == "flat");
    REQUIRE(to_string_of(ProjectionType::ORTHOGRAPHIC) == "ortho");
}

TEST_CASE("composite types stream with their key properties", "[to_string]") {
    SECTION("Light") {
        const std::string s = to_string_of(Light{Vec3(0, 0, 1), Color(1, 1, 1, 1), 2.0f, true});
        REQUIRE(contains(s, "Light"));
        REQUIRE(contains(s, "dir"));   // directional
        const std::string point = to_string_of(Light{Vec3(0, 0, 1), Color(1, 1, 1, 1), 1.0f, false});
        REQUIRE(contains(point, "point"));
    }

    SECTION("ClipPlane reports its coordinate space") {
        const std::string world = to_string_of(ClipPlane{Vec3(-1, 0, 0), 0.0f});
        REQUIRE(contains(world, "ClipPlane"));
        REQUIRE(contains(world, "world"));
        const std::string eye =
            to_string_of(ClipPlane{Vec3(0, 0, -1), -2.0f, PlaneSpace::EYE});
        REQUIRE(contains(eye, "eye"));
    }

    SECTION("Camera") {
        Camera cam;
        cam.eye = Vec3(0, 0, 10);
        cam.center = Vec3(0, 0, 0);
        const std::string s = to_string_of(cam);
        REQUIRE(contains(s, "Camera"));
        REQUIRE(contains(s, "persp"));
        REQUIRE(contains(s, "10"));

        Camera ortho = cam;
        ortho.projection = ProjectionType::ORTHOGRAPHIC;
        REQUIRE(contains(to_string_of(ortho), "ortho"));
    }

    SECTION("Mesh counts geometry and optional attributes") {
        Mesh mesh = make_unit_cube();
        const std::string plain = to_string_of(mesh);
        REQUIRE(contains(plain, "Mesh"));
        REQUIRE(contains(plain, "verts=8"));
        REQUIRE(contains(plain, "tris=12"));
        REQUIRE_FALSE(contains(plain, "cols"));

        Mesh shaded = make_unit_cube();
        shaded.colors.assign(shaded.vertices.size(), Color(1, 0, 0, 1));
        shaded.normals.assign(shaded.vertices.size(), Vec3(0, 0, 1));
        shaded.uvs.assign(shaded.vertices.size(), Vec2(0, 0));
        const std::string decorated = to_string_of(shaded);
        REQUIRE(contains(decorated, "cols"));
        REQUIRE(contains(decorated, "norms"));
        REQUIRE(contains(decorated, "uvs"));
        REQUIRE_FALSE(contains(decorated, "~norms"));
        REQUIRE(contains(decorated, "bb=["));
    }

    SECTION("Scene") {
        Scene scene;
        scene.meshes.push_back(make_unit_cube());
        const std::string s = to_string_of(scene);
        REQUIRE(contains(s, "Scene"));
    }

    SECTION("Image") {
        Image img(4, 3);
        const std::string s = to_string_of(img);
        REQUIRE(contains(s, "4x3"));
        REQUIRE(contains(s, "RGBA"));
    }

    SECTION("RenderOptions lists the enabled features") {
        RenderOptions opts;
        const std::string plain = to_string_of(opts);
        REQUIRE(contains(plain, "RenderOpts"));
        REQUIRE(contains(plain, "800x600"));
        REQUIRE_FALSE(contains(plain, "fog"));

        RenderOptions fancy = opts;
        fancy.wireframe = true;
        fancy.fog_enabled = true;
        fancy.fog_space = FogSpace::NDC;
        fancy.fog_start = 0.5f;
        fancy.fog_end = 1.0f;
        fancy.ssao_enabled = true;
        fancy.threads = 4;
        fancy.backface_culling = false;
        const std::string s = to_string_of(fancy);
        REQUIRE(contains(s, "wire"));
        REQUIRE(contains(s, "fog(ndc:"));
        REQUIRE(contains(s, "ssao"));
        REQUIRE(contains(s, "threads=4"));
        REQUIRE(contains(s, "~cull"));
    }
}
