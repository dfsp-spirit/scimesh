// scimesh — Full CLI Renderer Demo Application
//
// A self-contained command-line tool that loads a 3D mesh file, renders it
// with the scimesh software renderer, and saves the output image.  Settings
// are read from a TOML config file and can be overridden via CLI flags.
//
// Usage:
//   scimesh_render                                         # uses config.toml in cwd
//   scimesh_render --config my_config.toml                  # explicit config file
//   scimesh_render --mesh bunny.obj --output bunny.png     # quick overrides
//   scimesh_render --mesh sphere.ply --aa-samples 4 --ssao # quality boost
//   scimesh_render -h                                      # full help
//
// Supported mesh formats: Wavefront OBJ (.obj), PLY (.ply), STL (.stl),
// FreeSurfer surface (.surf)
// Supported image formats: PNG (.png), PPM (.ppm), BMP (.bmp)
//
// Dependencies (vendored alongside this file):
//   toml.hpp — toml++ single-header (MIT), TOML config parsing

#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/render_options.h>
#include <scimesh/image.h>
#include <scimesh/mesh.h>
#include <scimesh/scene.h>
#include <scimesh/obj_io.h>
#include <scimesh/ply_io.h>
#include <scimesh/stl_io.h>
#include <scimesh/fs_mesh_converter.h>
#include <scimesh/normals.h>
#include "toml.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cmath>

using scimesh::Vec3;
using scimesh::Color;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;
using scimesh::CropContentDirection;

// =========================================================================
// Application config (merged from TOML + CLI)
// =========================================================================

struct AppConfig {
    // [output]
    std::string filename = "render";
    std::string format   = "png";
    int width  = 800;
    int height = 600;

    // [mesh]
    std::string meshFile;
    float meshColorR = 0.7f;
    float meshColorG = 0.7f;
    float meshColorB = 0.7f;
    bool  computeNormals = true;

    // [camera]
    std::string projection = "perspective";
    float fov     = 45.0f;
    float nearPlane = 0.1f;
    float farPlane   = 0.0f;   // 0 = auto
    float margin   = 1.1f;
    float viewDirX = 0.0f;
    float viewDirY = 0.0f;
    float viewDirZ = 1.0f;
    float upX = 0.0f;
    float upY = 1.0f;
    float upZ = 0.0f;

    // [shading]
    std::string shadingMode     = "smooth";
    bool backfaceCulling        = true;
    bool invertNormals          = false;

    // [wireframe]
    bool  wireframe      = false;
    float wireframeColorR = 0.0f;
    float wireframeColorG = 0.0f;
    float wireframeColorB = 0.0f;

    // [antialiasing]
    int aaSamples = 1;

    // [ssao]
    bool  ssaoEnabled   = false;
    float ssaoRadius    = 16.0f;
    float ssaoIntensity = 0.8f;

    // [background]
    float bgColorR = 1.0f;
    float bgColorG = 1.0f;
    float bgColorB = 1.0f;

    // [lighting]
    float ambient = 0.3f;

    // [fog]
    bool  fogEnabled = false;
    float fogStart = 0.0f;
    float fogEnd   = 0.0f;
    float fogColorR = 0.5f;
    float fogColorG = 0.5f;
    float fogColorB = 0.5f;

    // [post]
    bool cropToContent = false;
    int  growPx = 0;
};

// =========================================================================
// Help text
// =========================================================================

static void printHelp(const char* prog) {
    printf(
        "scimesh_render — Headless CLI 3D mesh renderer\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --config FILE       TOML config file (default: config.toml in cwd)\n"
        "  --mesh FILE         Path to mesh file (.obj, .ply, .stl, .surf)\n"
        "  --output FILE       Output filename without extension (default: render)\n"
        "  --format FMT        Output format: png, ppm, bmp (default: png)\n"
        "  --width N           Image width in pixels (default: 800)\n"
        "  --height N          Image height in pixels (default: 600)\n"
        "  --mesh-color R,G,B  Default mesh color [0-1] (default: 0.7,0.7,0.7)\n"
        "  --no-normals        Skip auto-computing vertex normals\n"
        "  --projection TYPE   Projection: perspective, orthographic\n"
        "  --fov DEG           Field of view in degrees (default: 45)\n"
        "  --margin VAL        Extra margin around scene for auto-framing (default: 1.1)\n"
        "  --near N            Near clipping plane (default: 0.1)\n"
        "  --far N             Far clipping plane (0 = auto, default: 0)\n"
        "  --view-dir X,Y,Z    Camera view direction (default: 0,0,1)\n"
        "  --up X,Y,Z          Camera up vector (default: 0,1,0)\n"
        "  --shading MODE      Shading: smooth, flat (default: smooth)\n"
        "  --no-backface-cull  Disable backface culling\n"
        "  --invert-normals    Flip all surface normals\n"
        "  --wireframe         Enable wireframe overlay\n"
        "  --wire-color R,G,B  Wireframe color [0-1] (default: 0,0,0)\n"
        "  --aa-samples N      Anti-aliasing: 1 (off), 2, 4, 8 (default: 1)\n"
        "  --ssao              Enable screen-space ambient occlusion\n"
        "  --no-ssao           Disable SSAO\n"
        "  --ssao-radius N     SSAO sample radius in pixels (default: 16)\n"
        "  --ssao-intensity N  SSAO intensity [0-1] (default: 0.8)\n"
        "  --bg-color R,G,B    Background color [0-1] (default: 1,1,1)\n"
        "  --ambient N         Ambient light level [0-1] (default: 0.3)\n"
        "  --fog               Enable depth fog\n"
        "  --no-fog            Disable fog\n"
        "  --fog-start N       Fog start distance (default: 0)\n"
        "  --fog-end N         Fog end distance (0 = auto, default: 0)\n"
        "  --fog-color R,G,B   Fog color [0-1] (default: 0.5,0.5,0.5)\n"
        "  --crop              Crop transparent/background border after render\n"
        "  --grow N            Add N pixels padding after cropping (default: 0)\n"
        "  -h, --help          Show this help and exit\n"
        "\n"
        "Config file format is TOML; see config.toml for all defaults.\n"
        "CLI flags override config file values.\n"
        "\n"
        "Examples:\n"
        "  %s --mesh bunny.obj\n"
        "  %s --mesh sphere.ply --aa-samples 4 --ssao --output smooth_sphere\n"
        "  %s --mesh model.ply --shading flat --wireframe --bg-color 0.9,0.95,1.0\n",
        prog, prog, prog, prog);
}

// =========================================================================
// TOML config loading
// =========================================================================

static AppConfig loadTOMLConfig(const std::string& path) {
    AppConfig cfg;

    std::ifstream ifs(path);
    if (!ifs.is_open()) return cfg; // missing config → use all defaults

    try {
        auto tbl = toml::parse(ifs);

        // [output]
        if (auto* out = tbl["output"].as_table()) {
            cfg.filename = (*out)["filename"].value_or("render");
            cfg.format   = (*out)["format"].value_or("png");
            cfg.width    = (*out)["width"].value_or(800);
            cfg.height   = (*out)["height"].value_or(600);
        }

        // [mesh]
        if (auto* ms = tbl["mesh"].as_table()) {
            cfg.meshFile       = (*ms)["file"].value_or("");
            cfg.computeNormals = (*ms)["compute_normals"].value_or(true);
            if (auto* col = (*ms)["color"].as_array()) {
                if (col->size() >= 3) {
                    cfg.meshColorR = static_cast<float>((*col)[0].value_or(0.7));
                    cfg.meshColorG = static_cast<float>((*col)[1].value_or(0.7));
                    cfg.meshColorB = static_cast<float>((*col)[2].value_or(0.7));
                }
            }
        }

        // [camera]
        if (auto* cam = tbl["camera"].as_table()) {
            cfg.projection = (*cam)["projection"].value_or("perspective");
            cfg.fov        = static_cast<float>((*cam)["fov"].value_or(45.0));
            cfg.nearPlane  = static_cast<float>((*cam)["near"].value_or(0.1));
            cfg.farPlane   = static_cast<float>((*cam)["far"].value_or(0.0));
            cfg.margin     = static_cast<float>((*cam)["margin"].value_or(1.1));
            if (auto* vd = (*cam)["view_dir"].as_array()) {
                if (vd->size() >= 3) {
                    cfg.viewDirX = static_cast<float>((*vd)[0].value_or(0.0));
                    cfg.viewDirY = static_cast<float>((*vd)[1].value_or(0.0));
                    cfg.viewDirZ = static_cast<float>((*vd)[2].value_or(1.0));
                }
            }
            if (auto* up = (*cam)["up"].as_array()) {
                if (up->size() >= 3) {
                    cfg.upX = static_cast<float>((*up)[0].value_or(0.0));
                    cfg.upY = static_cast<float>((*up)[1].value_or(1.0));
                    cfg.upZ = static_cast<float>((*up)[2].value_or(0.0));
                }
            }
        }

        // [shading]
        if (auto* sh = tbl["shading"].as_table()) {
            cfg.shadingMode      = (*sh)["mode"].value_or("smooth");
            cfg.backfaceCulling  = (*sh)["backface_culling"].value_or(true);
            cfg.invertNormals    = (*sh)["invert_normals"].value_or(false);
        }

        // [wireframe]
        if (auto* wf = tbl["wireframe"].as_table()) {
            cfg.wireframe = (*wf)["enabled"].value_or(false);
            if (auto* col = (*wf)["color"].as_array()) {
                if (col->size() >= 3) {
                    cfg.wireframeColorR = static_cast<float>((*col)[0].value_or(0.0));
                    cfg.wireframeColorG = static_cast<float>((*col)[1].value_or(0.0));
                    cfg.wireframeColorB = static_cast<float>((*col)[2].value_or(0.0));
                }
            }
        }

        // [antialiasing]
        if (auto* aa = tbl["antialiasing"].as_table()) {
            cfg.aaSamples = (*aa)["samples"].value_or(1);
        }

        // [ssao]
        if (auto* ss = tbl["ssao"].as_table()) {
            cfg.ssaoEnabled   = (*ss)["enabled"].value_or(false);
            cfg.ssaoRadius    = static_cast<float>((*ss)["radius"].value_or(16.0));
            cfg.ssaoIntensity = static_cast<float>((*ss)["intensity"].value_or(0.8));
        }

        // [background]
        if (auto* bg = tbl["background"].as_table()) {
            if (auto* col = (*bg)["color"].as_array()) {
                if (col->size() >= 3) {
                    cfg.bgColorR = static_cast<float>((*col)[0].value_or(1.0));
                    cfg.bgColorG = static_cast<float>((*col)[1].value_or(1.0));
                    cfg.bgColorB = static_cast<float>((*col)[2].value_or(1.0));
                }
            }
        }

        // [lighting]
        if (auto* lt = tbl["lighting"].as_table()) {
            cfg.ambient = static_cast<float>((*lt)["ambient"].value_or(0.3));
        }

        // [fog]
        if (auto* fg = tbl["fog"].as_table()) {
            cfg.fogEnabled = (*fg)["enabled"].value_or(false);
            cfg.fogStart   = static_cast<float>((*fg)["start"].value_or(0.0));
            cfg.fogEnd     = static_cast<float>((*fg)["end"].value_or(0.0));
            if (auto* col = (*fg)["color"].as_array()) {
                if (col->size() >= 3) {
                    cfg.fogColorR = static_cast<float>((*col)[0].value_or(0.5));
                    cfg.fogColorG = static_cast<float>((*col)[1].value_or(0.5));
                    cfg.fogColorB = static_cast<float>((*col)[2].value_or(0.5));
                }
            }
        }

        // [post]
        if (auto* po = tbl["post"].as_table()) {
            cfg.cropToContent = (*po)["crop_to_content"].value_or(false);
            cfg.growPx = (*po)["grow_px"].value_or(0);
        }
    } catch (const toml::parse_error& e) {
        fprintf(stderr, "Warning: failed to parse '%s': %s\n", path.c_str(), e.what());
        fprintf(stderr, "Using defaults.\n");
    }

    return cfg;
}

// =========================================================================
// Helper: parse "R,G,B" string into three floats
// =========================================================================

static bool parseRGB(const std::string& s, float& r, float& g, float& b) {
    std::istringstream ss(s);
    std::string token;
    float vals[3];
    int i = 0;
    while (std::getline(ss, token, ',') && i < 3) {
        vals[i++] = static_cast<float>(std::atof(token.c_str()));
    }
    if (i != 3) return false;
    r = vals[0]; g = vals[1]; b = vals[2];
    return true;
}

// =========================================================================
// CLI argument parsing
// =========================================================================

static void applyCLIArgs(int argc, char** argv, AppConfig& cfg, std::string& configPath) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto nextStr = [&]() -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : "";
        };
        auto nextFloat = [&]() -> float {
            return (i + 1 < argc) ? static_cast<float>(std::atof(argv[++i])) : 0.0f;
        };
        auto nextInt = [&]() -> int {
            return (i + 1 < argc) ? std::atoi(argv[++i]) : 0;
        };

        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            exit(0);
        } else if (arg == "--config") {
            configPath = nextStr();
        } else if (arg == "--mesh") {
            cfg.meshFile = nextStr();
        } else if (arg == "--output") {
            cfg.filename = nextStr();
        } else if (arg == "--format") {
            cfg.format = nextStr();
        } else if (arg == "--width") {
            cfg.width = nextInt();
        } else if (arg == "--height") {
            cfg.height = nextInt();
        } else if (arg == "--mesh-color") {
            std::string val = nextStr();
            parseRGB(val, cfg.meshColorR, cfg.meshColorG, cfg.meshColorB);
        } else if (arg == "--no-normals") {
            cfg.computeNormals = false;
        } else if (arg == "--projection") {
            cfg.projection = nextStr();
        } else if (arg == "--fov") {
            cfg.fov = nextFloat();
        } else if (arg == "--margin") {
            cfg.margin = nextFloat();
        } else if (arg == "--near") {
            cfg.nearPlane = nextFloat();
        } else if (arg == "--far") {
            cfg.farPlane = nextFloat();
        } else if (arg == "--view-dir") {
            std::string val = nextStr();
            parseRGB(val, cfg.viewDirX, cfg.viewDirY, cfg.viewDirZ);
        } else if (arg == "--up") {
            std::string val = nextStr();
            parseRGB(val, cfg.upX, cfg.upY, cfg.upZ);
        } else if (arg == "--shading") {
            cfg.shadingMode = nextStr();
        } else if (arg == "--no-backface-cull") {
            cfg.backfaceCulling = false;
        } else if (arg == "--invert-normals") {
            cfg.invertNormals = true;
        } else if (arg == "--wireframe") {
            cfg.wireframe = true;
        } else if (arg == "--wire-color") {
            std::string val = nextStr();
            parseRGB(val, cfg.wireframeColorR, cfg.wireframeColorG, cfg.wireframeColorB);
        } else if (arg == "--aa-samples") {
            cfg.aaSamples = nextInt();
        } else if (arg == "--ssao") {
            cfg.ssaoEnabled = true;
        } else if (arg == "--no-ssao") {
            cfg.ssaoEnabled = false;
        } else if (arg == "--ssao-radius") {
            cfg.ssaoRadius = nextFloat();
        } else if (arg == "--ssao-intensity") {
            cfg.ssaoIntensity = nextFloat();
        } else if (arg == "--bg-color") {
            std::string val = nextStr();
            parseRGB(val, cfg.bgColorR, cfg.bgColorG, cfg.bgColorB);
        } else if (arg == "--ambient") {
            cfg.ambient = nextFloat();
        } else if (arg == "--fog") {
            cfg.fogEnabled = true;
        } else if (arg == "--no-fog") {
            cfg.fogEnabled = false;
        } else if (arg == "--fog-start") {
            cfg.fogStart = nextFloat();
        } else if (arg == "--fog-end") {
            cfg.fogEnd = nextFloat();
        } else if (arg == "--fog-color") {
            std::string val = nextStr();
            parseRGB(val, cfg.fogColorR, cfg.fogColorG, cfg.fogColorB);
        } else if (arg == "--crop") {
            cfg.cropToContent = true;
        } else if (arg == "--grow") {
            cfg.growPx = nextInt();
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
            exit(1);
        }
    }
}

// =========================================================================
// Determine file extension so we pick the right loader
// =========================================================================

static std::string lowerCase(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

static std::string fileExtension(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    return lowerCase(path.substr(dot));
}

// =========================================================================
// Load mesh from file, auto-detecting format by extension
// =========================================================================

static Mesh loadMesh(const std::string& path) {
    std::string ext = fileExtension(path);
    if (ext == ".obj") {
        return scimesh::obj_io::read_obj(path);
    } else if (ext == ".ply") {
        return scimesh::ply_io::read_ply(path);
    } else if (ext == ".stl") {
        return scimesh::stl_io::read_stl(path);
    } else if (ext == ".surf") {
        fs::Mesh fs_mesh;
        fs::read_surf(&fs_mesh, path);
        return scimesh::convert_fs_mesh(fs_mesh);
    } else {
        fprintf(stderr, "Error: unsupported file extension '%s'\n", ext.c_str());
        fprintf(stderr, "Supported formats: .obj, .ply, .stl, .surf\n");
        exit(1);
    }
}

// =========================================================================
// Build RenderOptions from AppConfig
// =========================================================================

static RenderOptions buildRenderOptions(const AppConfig& cfg, const Camera& cam) {
    RenderOptions opts;

    // Output size
    opts.width  = cfg.width;
    opts.height = cfg.height;

    // Shading
    if (cfg.shadingMode == "flat") {
        opts.shading = ShadingMode::FLAT;
    } else {
        opts.shading = ShadingMode::SMOOTH;
    }
    opts.backface_culling = cfg.backfaceCulling;
    opts.invert_normals   = cfg.invertNormals;

    // Colors
    opts.background_color = Color(cfg.bgColorR, cfg.bgColorG, cfg.bgColorB, 1.0f);
    opts.default_color    = Color(cfg.meshColorR, cfg.meshColorG, cfg.meshColorB, 1.0f);

    // Wireframe
    opts.wireframe = cfg.wireframe;
    opts.wireframe_color = Color(cfg.wireframeColorR, cfg.wireframeColorG,
                                 cfg.wireframeColorB, 1.0f);

    // Anti-aliasing
    opts.aa_samples = cfg.aaSamples;

    // SSAO
    opts.ssao_enabled   = cfg.ssaoEnabled;
    opts.ssao_radius    = cfg.ssaoRadius;
    opts.ssao_intensity = cfg.ssaoIntensity;

    // Projection
    if (cfg.projection == "orthographic") {
        opts.projection = scimesh::ProjectionType::ORTHOGRAPHIC;
    }

    // Lighting
    opts.ambient = cfg.ambient;

    // Fog
    opts.fog_enabled = cfg.fogEnabled;
    opts.fog_start   = cfg.fogStart;
    opts.fog_end     = cfg.fogEnd;
    opts.fog_color   = Color(cfg.fogColorR, cfg.fogColorG, cfg.fogColorB, 1.0f);

    // Near/far planes
    opts.near_plane = cfg.nearPlane;
    if (cfg.farPlane > 0.0f) {
        opts.far_plane = cfg.farPlane;
    } else {
        // Auto far plane: 4× the distance from camera to scene center
        opts.far_plane = glm::length(cam.eye - cam.center) * 4.0f;
    }

    return opts;
}

// =========================================================================
// Write image in the requested format
// =========================================================================

static bool writeImage(const Image& img, const std::string& filename,
                       const std::string& format) {
    std::string path = filename;
    if (format == "png") {
        path += ".png";
        return img.write_png(path);
    } else if (format == "ppm") {
        path += ".ppm";
        return img.write_ppm(path);
    } else if (format == "bmp") {
        path += ".bmp";
        return img.write_bmp(path);
    } else {
        fprintf(stderr, "Error: unsupported format '%s'. Use png, ppm, or bmp.\n",
                format.c_str());
        return false;
    }
}

// =========================================================================
// main
// =========================================================================

int main(int argc, char** argv) {
    // 1. Load defaults from TOML config
    std::string configPath = "config.toml";
    AppConfig   cfg        = loadTOMLConfig(configPath);

    // 2. Apply CLI overrides (configPath may be changed by --config)
    applyCLIArgs(argc, argv, cfg, configPath);

    // Reload config if --config was specified
    if (configPath != "config.toml") {
        cfg = loadTOMLConfig(configPath);
        // Re-apply CLI so CLI always wins
        applyCLIArgs(argc, argv, cfg, configPath);
    }

    // 3. Require a mesh file
    if (cfg.meshFile.empty()) {
        fprintf(stderr, "Error: no mesh file specified.\n");
        fprintf(stderr, "Set 'file' in [mesh] section of config.toml"
                        " or use --mesh <path>.\n");
        fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
        return 1;
    }

    // 4. Load mesh
    std::cout << "Loading mesh: " << cfg.meshFile << std::endl;
    Mesh mesh;
    try {
        mesh = loadMesh(cfg.meshFile);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error loading mesh: %s\n", e.what());
        return 1;
    }

    if (!mesh.is_valid()) {
        fprintf(stderr, "Error: loaded mesh is invalid.\n");
        return 1;
    }
    std::cout << "  Vertices: " << mesh.vertices.size()
              << ", triangles: " << mesh.triangles.size() << std::endl;

    // 5. Compute normals if needed
    if (cfg.computeNormals && mesh.normals.empty()) {
        std::cout << "  Computing vertex normals..." << std::endl;
        scimesh::compute_vertex_normals(mesh, mesh.normals);
    }

    // 6. Build scene
    Scene scene;
    scene.meshes.push_back(std::move(mesh));

    // 7. Set up camera with auto-framing
    Vec3 view_dir(cfg.viewDirX, cfg.viewDirY, cfg.viewDirZ);
    view_dir = glm::normalize(view_dir);
    Vec3 up(cfg.upX, cfg.upY, cfg.upZ);

    std::cout << "Setting up camera..." << std::endl;
    Camera cam = scimesh::camera_fit_scene(scene, view_dir, up, cfg.fov, cfg.margin);

    if (cfg.projection == "orthographic") {
        cam.projection = scimesh::ProjectionType::ORTHOGRAPHIC;
    }

    std::cout << "  eye    = (" << cam.eye.x << ", " << cam.eye.y << ", "
              << cam.eye.z << ")\n";
    std::cout << "  center = (" << cam.center.x << ", " << cam.center.y
              << ", " << cam.center.z << ")\n";
    std::cout << "  fov    = " << cam.fov_degrees << "°\n";
    std::cout << "  projection = " << cfg.projection << "\n";

    // 8. Build render options
    RenderOptions opts = buildRenderOptions(cfg, cam);

    // 9. Render
    std::cout << "Rendering at " << opts.width << "×" << opts.height;
    if (opts.aa_samples > 1) {
        std::cout << " (" << opts.aa_samples << "× AA)";
    }
    if (opts.ssao_enabled) {
        std::cout << " (SSAO)";
    }
    std::cout << "..." << std::endl;

    Renderer renderer;
    Image img = renderer.render_scene(scene, cam, opts);

    // 10. Post-processing
    if (cfg.cropToContent) {
        img.crop_to_content(CropContentDirection::ALL, opts.background_color);
    }
    if (cfg.growPx > 0) {
        img.grow(cfg.growPx, cfg.growPx, cfg.growPx, cfg.growPx,
                 opts.background_color);
    }

    // 11. Write output
    std::string outPath = cfg.filename;
    bool ok = writeImage(img, outPath, cfg.format);
    if (ok) {
        std::cout << "Wrote " << outPath << "." << cfg.format << std::endl;
    } else {
        std::cerr << "Error writing output image.\n";
        return 1;
    }

    std::cout << "Done.\n";
    return 0;
}
