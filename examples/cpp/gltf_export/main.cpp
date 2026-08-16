/// Demo: export both brain hemisphere meshes to glTF 2.0 (.glb and .gltf).
///
/// Loads the two example hemisphere meshes from test_data/ply (sulcal-depth
/// viridis coloring), places them in a Scene, and writes them as glTF:
///
///   brain.glb              — single self-contained binary glTF file
///   brain.gltf + brain.bin — JSON glTF with an external binary buffer
///
/// The glTF file can be viewed in any glTF-capable viewer.  To get a
/// double-clickable offline WebGL page, use the R example
/// (examples/R/export_gltf.R), which does the same export and additionally
/// generates examples/web/brain_viewer.html (three.js based, fully offline).
///
/// Build:
///   cd examples/cpp/gltf_export && mkdir -p build && cd build
///   cmake .. -DCMAKE_BUILD_TYPE=Release && make
///
/// Run (from the build directory; paths are relative to the repo root):
///   ./gltf_export

#include <scimesh/ply_io.h>
#include <scimesh/scene.h>
#include <scimesh/gltf_io.h>

#include <string>
#include <iostream>
#include <vector>
#include <cstdio>

using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Mat4;

static bool file_exists(const std::string &path) {
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

int main() {
    // Relative to the build directory (examples/cpp/gltf_export/build), which
    // is 4 levels below the repo root.
    const std::string lh_ply = "../../../../test_data/ply/lh_mesh_sulc_viridis.ply";
    const std::string rh_ply = "../../../../test_data/ply/rh_mesh_sulc_viridis.ply";
    for (const auto &f : {lh_ply, rh_ply}) {
        if (!file_exists(f)) {
            std::cerr << "ERROR: PLY file not found: " << f
                      << " (run the example from examples/cpp/gltf_export/build)\n";
            return 1;
        }
    }

    Mesh lh = scimesh::ply_io::read_ply(lh_ply);
    Mesh rh = scimesh::ply_io::read_ply(rh_ply);
    std::cout << "Loaded " << lh.vertices.size() << " vertices (lh), "
              << rh.vertices.size() << " vertices (rh)\n";

    Scene scene;
    scene.add(lh, Mat4(1.0f), "lh");
    scene.add(rh, Mat4(1.0f), "rh");

    // Binary glTF (single file).
    scimesh::gltf_io::write_glb("brain.glb", scene);
    std::cout << "Wrote brain.glb\n";

    // JSON glTF + external .bin.
    scimesh::gltf_io::write_gltf("brain.gltf", scene);
    std::cout << "Wrote brain.gltf + brain.bin\n";

    return 0;
}
