#include "catch_amalgamated.hpp"
#include "test_meshes.h"
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/scene.h>
#include <scimesh/image.h>
#include <scimesh/gltf_io.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <cstdio>

using namespace scimesh;
using scimesh_test::make_unit_cube;
using scimesh_test::make_colored_cube;
using Catch::Approx;

TEST_CASE("Scene stores per-mesh transforms and names", "[scene]") {
    Mesh cube = make_unit_cube();
    Mat4 tr = glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f));

    Scene scene;
    scene.add(cube);                                    // identity, unnamed
    scene.add(cube, tr, "two");

    REQUIRE(scene.size() == 2);
    REQUIRE(scene.name(0) == "");
    REQUIRE(scene.name(1) == "two");
    REQUIRE(scene.transform(0) == Mat4(1.0f));
    REQUIRE(scene.transform(1)[3][0] == Approx(2.0f));

    // Safe reads out of range
    REQUIRE(scene.name(9) == "");
    REQUIRE(scene.transform(9) == Mat4(1.0f));

    // Direct push_back still works and defaults to identity
    scene.meshes.push_back(cube);
    REQUIRE(scene.size() == 3);
    REQUIRE(scene.transform(2) == Mat4(1.0f));
    REQUIRE(scene.node(2).mesh == &scene.meshes[2]);

    // set_transform / set_name
    scene.set_transform(2, tr);
    scene.set_name(2, "third");
    REQUIRE(scene.transform(2)[3][0] == Approx(2.0f));
    REQUIRE(scene.name(2) == "third");
}

TEST_CASE("Scene bounding box applies placement transforms", "[scene]") {
    Mesh cube = make_unit_cube();  // -1..1 in all axes

    Scene scene;
    scene.add(cube);
    Vec3 mn, mx;
    scene.compute_bounding_box(mn, mx);
    REQUIRE(mn.x == Approx(-1.0f));
    REQUIRE(mx.x == Approx(1.0f));

    scene.add(cube, glm::translate(Mat4(1.0f), Vec3(5.0f, 0.0f, 0.0f)), "moved");
    Vec3 mn2, mx2;
    scene.compute_bounding_box(mn2, mx2);
    REQUIRE(mx2.x == Approx(6.0f));  // cube shifted +5 -> right edge at 1+5
    REQUIRE(mn2.x == Approx(-1.0f)); // untransformed cube still at -1
}

TEST_CASE("Scene node transform renders like a baked transform", "[scene][rendering]") {
    Mesh cube = make_colored_cube();
    Mat4 tr = glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f));

    Scene scene;
    scene.add(cube, tr, "cube");

    Mesh baked = cube;
    for (auto &v : baked.vertices)
        v = transform_point(tr, v);
    Scene baked_scene;
    baked_scene.add(baked);

    Camera cam;
    cam.eye = Vec3(0, 0, 10);
    cam.center = Vec3(1, 0, 0);
    cam.fov_degrees = 45.0f;

    RenderOptions opts;
    opts.width = 128;
    opts.height = 128;
    opts.background_color = Color(0, 0, 0, 1);
    opts.threads = 1;

    Renderer renderer;
    Image a = renderer.render_scene(scene, cam, opts);
    Image b = renderer.render_scene(baked_scene, cam, opts);
    Scene origin_scene;
    origin_scene.add(cube);
    Image c = renderer.render_scene(origin_scene, cam, opts);

    REQUIRE(a.pixels.size() == b.pixels.size());
    REQUIRE(a.pixels.size() == c.pixels.size());

    // Declarative vs baked may differ on exact rasterization-boundary pixels
    // only (float association order); allow <= 1% of pixels to differ.
    size_t diff_baked = 0;
    for (size_t i = 0; i < a.pixels.size(); ++i)
        if (a.pixels[i] != b.pixels[i]) ++diff_baked;
    INFO("pixels differing from baked render: " << diff_baked);
    REQUIRE(diff_baked <= a.pixels.size() / 100);

    // Moving the cube must change the image substantially vs. the origin scene.
    size_t diff_origin = 0;
    for (size_t i = 0; i < a.pixels.size(); ++i)
        if (a.pixels[i] != c.pixels[i]) ++diff_origin;
    INFO("pixels differing from untransformed scene: " << diff_origin);
    REQUIRE(diff_origin > a.pixels.size() / 10);
}

TEST_CASE("gltf_io writes a valid .glb file", "[gltf]") {
    Scene scene;
    scene.add(make_colored_cube());
    scene.add(make_colored_cube(),
              glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f)), "moved");

    const std::string path = "scimesh_test_out.glb";
    gltf_io::write_glb(path, scene);

    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    char header[12];
    f.read(header, 12);
    REQUIRE(f.gcount() == 12);
    REQUIRE(std::string(header, 4) == "glTF");
    // version (little-endian uint32 at offset 4)
    uint32_t version = static_cast<uint8_t>(header[4]) |
                       (static_cast<uint8_t>(header[5]) << 8) |
                       (static_cast<uint8_t>(header[6]) << 16) |
                       (static_cast<uint8_t>(header[7]) << 24);
    REQUIRE(version == 2);
    std::remove(path.c_str());
}

TEST_CASE("gltf_io writes .gltf JSON with geometry, colors, and camera", "[gltf]") {
    Scene scene;
    scene.add(make_colored_cube(), Mat4(1.0f), "cube");
    scene.add(make_colored_cube(),
              glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f)), "moved");

    Camera cam;
    cam.eye = Vec3(0, 0, 5);
    cam.center = Vec3(0, 0, 0);
    cam.fov_degrees = 45.0f;

    const std::string json_path = "scimesh_test_out.gltf";
    const std::string bin_path = "scimesh_test_out.bin";
    gltf_io::write_gltf(json_path, scene, &cam);

    std::ifstream jf(json_path);
    REQUIRE(jf.good());
    std::string json((std::istreambuf_iterator<char>(jf)),
                     std::istreambuf_iterator<char>());

    REQUIRE(json.find("\"asset\"") != std::string::npos);
    REQUIRE(json.find("\"2.0\"") != std::string::npos);
    REQUIRE(json.find("\"COLOR_0\"") != std::string::npos);
    REQUIRE(json.find("\"POSITION\"") != std::string::npos);
    REQUIRE(json.find("\"cameras\"") != std::string::npos);
    REQUIRE(json.find("\"yfov\"") != std::string::npos);
    REQUIRE(json.find("\"matrix\"") != std::string::npos);

    // External .bin buffer written and non-empty
    std::ifstream bf(bin_path, std::ios::binary);
    REQUIRE(bf.good());
    bf.seekg(0, std::ios::end);
    REQUIRE(bf.tellg() > 0);

    std::remove(json_path.c_str());
    std::remove(bin_path.c_str());
}
