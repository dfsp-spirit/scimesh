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
// FreeSurfer surface (auto: .white; use --mesh-format fs for .pial, .sphere, etc.)
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
#include <scimesh/text.h>
#include "toml.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cmath>

using scimesh::Vec3;
using scimesh::Vec2;
using scimesh::Color;
using scimesh::Mat4;
using scimesh::Mesh;
using scimesh::Scene;
using scimesh::Camera;
using scimesh::RenderOptions;
using scimesh::ShadingMode;
using scimesh::Renderer;
using scimesh::Image;
using scimesh::CropContentDirection;
using scimesh::FitMode;
using scimesh::TextLayer;
using scimesh::TextSpace;
using scimesh::grid_arrange;

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
    std::vector<std::string> meshFiles;   // one or more mesh files
    std::string meshFormat;   // empty = auto-detect; "obj", "ply", "stl", or "fs"
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

    // [text] (labels; see TextLayer)
    std::vector<std::string> textStrings;  // one per label (--text, repeatable)
    std::vector<Vec3> textPositions;       // parallel to textStrings (--text-at)
    float textSize = 18.0f;                // height in output pixels
    Color textColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    Color textHaloColor = Color(1.0f, 1.0f, 1.0f, 0.0f);  // alpha 0 = no halo
    float textHaloWidth = 1.5f;
    std::string textFont;      // empty = bundled Inter-Regular.ttf
    bool textScreenSpace = false;  // positions are output pixels (--text-screen)
    bool textDepthTest = true;     // --text-no-depth disables the depth test
    Vec2 textAdj = Vec2(0.5f, 0.5f);      // where the position sits on the text box
    Vec2 textOffset = Vec2(0.0f, 0.0f);   // extra pixel offset

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
    std::string fogSpace = "world";   // "world" (world units) or "ndc"
    float fogStart = 0.0f;
    float fogEnd   = 0.0f;
    float fogColorR = 0.5f;
    float fogColorG = 0.5f;
    float fogColorB = 0.5f;

    // [post]
    bool cropToContent = false;
    int  growPx = 0;
    int  rotateDeg = 0;        // 0, 90, 180, or 270

    // [composite]
    bool compositeMode = false;
    std::vector<std::string> compositeImages;
    int  compositeCols = 0;   // 0 = auto
    int  compositeRows = 0;   // 0 = auto
    std::string fitMode = "pad";

    // [info]
    bool infoMode = false;    // --info: print image dimensions, no rendering
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
        "  --mesh FILE         Path to mesh file (.obj, .ply, .stl, .white).\n"
        "                      Repeatable for multiple meshes in one scene.\n"
        "  --mesh-format FMT    Force mesh format: obj, ply, stl, fs\n"
        "  --output FILE       Output filename, extension optional (default: render)\n"
        "                      A supported extension (.png, .ppm, .bmp) selects\n"
        "                      the format and overrides --format.\n"
        "  --format FMT        Output format for names without extension:\n"
        "                      png, ppm, bmp (default: png)\n"
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
        "  --fog-space S       Fog distance unit: world (default) or ndc\n"
        "  --fog-color R,G,B   Fog color [0-1] (default: 0.5,0.5,0.5)\n"
        "  --crop              Crop transparent/background border after render\n"
        "                      (also applied to each image in composite mode)\n"
        "  --grow N            Add N pixels padding after cropping (default: 0)\n"
        "                      (also applied to each image in composite mode)\n"
        "  --rotate DEG        Rotate output 0/90/180/270 degrees CW (default: 0)\n"
        "  -h, --help          Show this help and exit\n"
        "\n"
        "Text labels (optional, drawn on top of the meshes):\n"
        "  --text STRING       Label text (repeatable; use \\n for line breaks)\n"
        "  --text-at X,Y,Z     Position of the matching --text label. In world\n"
        "                      space (default) or in output pixels with\n"
        "                      --text-screen (origin: top left).\n"
        "  --text-size N       Text height in pixels (default: 18)\n"
        "  --text-color R,G,B[,A]  Text color, 0-1 (default: 0,0,0)\n"
        "  --text-halo R,G,B[,A]   Halo (outline) color, alpha 0 = off\n"
        "  --text-halo-width N Halo thickness in pixels (default: 1.5)\n"
        "  --text-font FILE    .ttf file to use (default: bundled Inter font)\n"
        "  --text-screen       Positions are output pixels instead of world coords\n"
        "  --text-no-depth     Always draw labels, even behind the geometry\n"
        "  --text-adj X,Y      Where the position sits on the text box, 0-1\n"
        "                      (default: 0.5,0.5 = centred)\n"
        "  --text-offset DX,DY Extra label offset in pixels (default: 0,0)\n"
        "\n"
        "Composite mode (image layout, no rendering):\n"
        "  --composite         Enter composite mode; remaining non-flag arguments\n"
        "                      are image files to arrange into a grid.\n"
        "  --cols N            Grid columns (0 = auto, default: 0)\n"
        "  --rows N            Grid rows (0 = auto, default: 0)\n"
        "  --fit-mode MODE     Size handling: pad (default) or scale\n"
        "                      (--crop and --grow N also apply to each tile)\n"
        "\n"
        "Image info mode (print dimensions, no rendering):\n"
        "  --info              Print '<path> WxH' for each given image file,\n"
        "                      one per line, then exit. Useful for scripts\n"
        "                      that must avoid external tools like 'identify'.\n"
        "\n"
        "Config file format is TOML; see config.toml for all defaults.\n"
        "CLI flags override config file values.\n"
        "\n"
        "Examples:\n"
        "  %s --mesh bunny.obj\n"
        "  %s --mesh sphere.ply --aa-samples 4 --ssao --output smooth_sphere\n"
        "  %s --mesh model.ply --shading flat --wireframe --bg-color 0.9,0.95,1.0\n"
        "  %s --composite view1.png view2.png --cols 2 --output grid.png\n"
        "\n"
        "Text label workflow:\n"
        "  %s --mesh brain.ply --text anterior --text-at 3,0,0 --text-size 22\n"
        "  %s --mesh atom.ply --text C --text-at 0,0,0 --text-halo 1,1,1,0.9\n"
        "  %s --mesh brain.ply --text 'my figure' --text-at 20,20,0 --text-screen --text-adj 0,1\n"
        "\n"
        "Composite workflow (render multiple views + combine):\n"
        "  %s --mesh brain.ply --view-dir 0,0,1 --output lat\n"
        "  %s --mesh brain.ply --view-dir 0,1,0 --output med\n"
        "  %s --composite lat.png med.png colorbar.png --cols 3 --output final\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

// =========================================================================
// TOML config loading
// =========================================================================

static std::string lowerCase(const std::string& s);  // defined below

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
            cfg.meshFormat     = (*ms)["format"].value_or("");
            cfg.computeNormals = (*ms)["compute_normals"].value_or(true);
            // Support both single file (string) and multiple files (array)
            if (auto* fileArr = (*ms)["file"].as_array()) {
                for (const auto& f : *fileArr) {
                    if (auto s = f.value<std::string>())
                        cfg.meshFiles.push_back(*s);
                }
            } else if (auto s = (*ms)["file"].value<std::string>()) {
                if (!s->empty()) cfg.meshFiles.push_back(*s);
            }
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

        // [text]
        if (auto* tx = tbl["text"].as_table()) {
            // Support both a single label (string) and multiple ones (array).
            if (auto* arr = (*tx)["string"].as_array()) {
                for (const auto& s : *arr) {
                    if (auto v = s.value<std::string>()) cfg.textStrings.push_back(*v);
                }
            } else if (auto s = (*tx)["string"].value<std::string>()) {
                if (!s->empty()) cfg.textStrings.push_back(*s);
            }
            // Positions: one [x, y, z] per label.
            if (auto* arr = (*tx)["at"].as_array()) {
                for (const auto& p : *arr) {
                    if (auto* pos = p.as_array()) {
                        if (pos->size() >= 3) {
                            cfg.textPositions.push_back(Vec3(
                                static_cast<float>((*pos)[0].value_or(0.0)),
                                static_cast<float>((*pos)[1].value_or(0.0)),
                                static_cast<float>((*pos)[2].value_or(0.0))));
                        }
                    }
                }
            }
            cfg.textSize = static_cast<float>((*tx)["size"].value_or(18.0));
            if (auto* col = (*tx)["color"].as_array()) {
                if (col->size() >= 3) {
                    cfg.textColor = Color(static_cast<float>((*col)[0].value_or(0.0)),
                                          static_cast<float>((*col)[1].value_or(0.0)),
                                          static_cast<float>((*col)[2].value_or(0.0)),
                                          static_cast<float>(col->size() > 3
                                              ? (*col)[3].value_or(1.0) : 1.0));
                }
            }
            if (auto* col = (*tx)["halo_color"].as_array()) {
                if (col->size() >= 3) {
                    cfg.textHaloColor = Color(static_cast<float>((*col)[0].value_or(1.0)),
                                              static_cast<float>((*col)[1].value_or(1.0)),
                                              static_cast<float>((*col)[2].value_or(1.0)),
                                              static_cast<float>(col->size() > 3
                                                  ? (*col)[3].value_or(0.0) : 0.0));
                }
            }
            cfg.textHaloWidth = static_cast<float>((*tx)["halo_width"].value_or(1.5));
            cfg.textFont      = (*tx)["font"].value_or("");
            const std::string space = lowerCase((*tx)["space"].value_or("world"));
            cfg.textScreenSpace = (space == "screen");
            cfg.textDepthTest   = (*tx)["depth_test"].value_or(true);
            if (auto* adj = (*tx)["adj"].as_array()) {
                if (adj->size() >= 2) {
                    cfg.textAdj = Vec2(static_cast<float>((*adj)[0].value_or(0.5)),
                                       static_cast<float>((*adj)[1].value_or(0.5)));
                }
            }
            if (auto* off = (*tx)["offset"].as_array()) {
                if (off->size() >= 2) {
                    cfg.textOffset = Vec2(static_cast<float>((*off)[0].value_or(0.0)),
                                          static_cast<float>((*off)[1].value_or(0.0)));
                }
            }
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
            cfg.fogSpace   = (*fg)["space"].value_or(std::string("world"));
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
// Helper: parse "X,Y,Z" string into three floats
// =========================================================================

static bool parseVec3(const std::string& s, float& r, float& g, float& b) {
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
// Helper: parse "X,Y" into two floats
// =========================================================================

static bool parseVec2(const std::string& s, float& x, float& y) {
    std::istringstream ss(s);
    std::string token;
    float vals[2];
    int i = 0;
    while (std::getline(ss, token, ',') && i < 2) {
        vals[i++] = static_cast<float>(std::atof(token.c_str()));
    }
    if (i != 2) return false;
    x = vals[0]; y = vals[1];
    return true;
}

// =========================================================================
// Helper: parse "R,G,B" or "R,G,B,A" (values 0-1) into a Color
// =========================================================================

static bool parseColor(const std::string& s, Color& color) {
    std::istringstream ss(s);
    std::string token;
    float vals[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    int i = 0;
    while (std::getline(ss, token, ',') && i < 4) {
        vals[i++] = static_cast<float>(std::atof(token.c_str()));
    }
    if (i < 3) return false;
    color = Color(vals[0], vals[1], vals[2], vals[3]);
    return true;
}

// =========================================================================
// Helper: turn the two-character sequence "\n" into a real newline
// =========================================================================

static std::string unescapeNewlines(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == 'n') { out.push_back('\n'); ++i; continue; }
            if (s[i + 1] == '\\') { out.push_back('\\'); ++i; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}

// =========================================================================
// CLI argument parsing
// =========================================================================

static void applyCLIArgs(int argc, char** argv, AppConfig& cfg) {
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
            nextStr(); // already handled before applyCLIArgs
        } else if (arg == "--mesh") {
            cfg.meshFiles.push_back(nextStr());
        } else if (arg == "--mesh-format") {
            cfg.meshFormat = nextStr();
        } else if (arg == "--output") {
            cfg.filename = nextStr();
        } else if (arg == "--format") {
            cfg.format = nextStr();
        } else if (arg == "--width") {
            cfg.width = nextInt();
            if (cfg.width <= 0) {
                fprintf(stderr, "Warning: --width must be > 0, using default 800.\n");
                cfg.width = 800;
            }
        } else if (arg == "--height") {
            cfg.height = nextInt();
            if (cfg.height <= 0) {
                fprintf(stderr, "Warning: --height must be > 0, using default 600.\n");
                cfg.height = 600;
            }
        } else if (arg == "--mesh-color") {
            std::string val = nextStr();
            if (!parseVec3(val, cfg.meshColorR, cfg.meshColorG, cfg.meshColorB)) {
                fprintf(stderr, "Warning: invalid value for --mesh-color '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--text") {
            cfg.textStrings.push_back(unescapeNewlines(nextStr()));
        } else if (arg == "--text-at") {
            std::string val = nextStr();
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (parseVec3(val, x, y, z)) {
                cfg.textPositions.push_back(Vec3(x, y, z));
            } else {
                fprintf(stderr, "Warning: invalid value for --text-at '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--text-size") {
            cfg.textSize = nextFloat();
        } else if (arg == "--text-color") {
            std::string val = nextStr();
            if (!parseColor(val, cfg.textColor)) {
                fprintf(stderr, "Warning: invalid value for --text-color '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--text-halo") {
            std::string val = nextStr();
            if (!parseColor(val, cfg.textHaloColor)) {
                fprintf(stderr, "Warning: invalid value for --text-halo '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--text-halo-width") {
            cfg.textHaloWidth = nextFloat();
        } else if (arg == "--text-font") {
            cfg.textFont = nextStr();
        } else if (arg == "--text-screen") {
            cfg.textScreenSpace = true;
        } else if (arg == "--text-no-depth") {
            cfg.textDepthTest = false;
        } else if (arg == "--text-adj") {
            std::string val = nextStr();
            float x = 0.5f, y = 0.5f;
            if (parseVec2(val, x, y)) {
                cfg.textAdj = Vec2(x, y);
            } else {
                fprintf(stderr, "Warning: invalid value for --text-adj '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--text-offset") {
            std::string val = nextStr();
            float x = 0.0f, y = 0.0f;
            if (parseVec2(val, x, y)) {
                cfg.textOffset = Vec2(x, y);
            } else {
                fprintf(stderr, "Warning: invalid value for --text-offset '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--no-normals") {
            cfg.computeNormals = false;
        } else if (arg == "--projection") {
            cfg.projection = nextStr();
        } else if (arg == "--fov") {
            cfg.fov = nextFloat();
            if (cfg.fov <= 0.0f) {
                fprintf(stderr, "Warning: --fov must be > 0, using default 45.\n");
                cfg.fov = 45.0f;
            }
        } else if (arg == "--margin") {
            cfg.margin = nextFloat();
        } else if (arg == "--near") {
            cfg.nearPlane = nextFloat();
        } else if (arg == "--far") {
            cfg.farPlane = nextFloat();
        } else if (arg == "--view-dir") {
            std::string val = nextStr();
            if (!parseVec3(val, cfg.viewDirX, cfg.viewDirY, cfg.viewDirZ)) {
                fprintf(stderr, "Warning: invalid value for --view-dir '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--up") {
            std::string val = nextStr();
            if (!parseVec3(val, cfg.upX, cfg.upY, cfg.upZ)) {
                fprintf(stderr, "Warning: invalid value for --up '%s', ignored.\n", val.c_str());
            }
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
            if (!parseVec3(val, cfg.wireframeColorR, cfg.wireframeColorG, cfg.wireframeColorB)) {
                fprintf(stderr, "Warning: invalid value for --wire-color '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--aa-samples") {
            cfg.aaSamples = nextInt();
            if (cfg.aaSamples != 1 && cfg.aaSamples != 2 && cfg.aaSamples != 4 && cfg.aaSamples != 8) {
                fprintf(stderr, "Warning: --aa-samples must be 1, 2, 4, or 8, using default 1.\n");
                cfg.aaSamples = 1;
            }
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
            if (!parseVec3(val, cfg.bgColorR, cfg.bgColorG, cfg.bgColorB)) {
                fprintf(stderr, "Warning: invalid value for --bg-color '%s', ignored.\n", val.c_str());
            }
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
        } else if (arg == "--fog-space") {
            cfg.fogSpace = nextStr();
        } else if (arg == "--fog-color") {
            std::string val = nextStr();
            if (!parseVec3(val, cfg.fogColorR, cfg.fogColorG, cfg.fogColorB)) {
                fprintf(stderr, "Warning: invalid value for --fog-color '%s', ignored.\n", val.c_str());
            }
        } else if (arg == "--crop") {
            cfg.cropToContent = true;
        } else if (arg == "--grow") {
            cfg.growPx = nextInt();
        } else if (arg == "--rotate") {
            cfg.rotateDeg = nextInt();
        } else if (arg == "--composite") {
            cfg.compositeMode = true;
        } else if (arg == "--info") {
            cfg.infoMode = true;
        } else if (arg == "--cols") {
            cfg.compositeCols = nextInt();
        } else if (arg == "--rows") {
            cfg.compositeRows = nextInt();
        } else if (arg == "--fit-mode") {
            cfg.fitMode = nextStr();
        } else if ((cfg.compositeMode || cfg.infoMode) && !arg.empty() && arg[0] != '-') {
            cfg.compositeImages.push_back(arg);
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

static Mesh loadMesh(const std::string& path, const std::string& formatOverride) {
    // If the user specified a format override, use it.
    std::string fmt = lowerCase(formatOverride);
    if (fmt == "obj") {
        return scimesh::obj_io::read_obj(path);
    } else if (fmt == "ply") {
        return scimesh::ply_io::read_ply(path);
    } else if (fmt == "stl") {
        return scimesh::stl_io::read_stl(path);
    } else if (fmt == "fs") {
        fs::Mesh fs_mesh;
        fs::read_surf(&fs_mesh, path);
        return scimesh::convert_fs_mesh(fs_mesh);
    } else if (!fmt.empty()) {
        fprintf(stderr, "Error: unknown mesh format '%s'. Use obj, ply, stl, or fs.\n",
                formatOverride.c_str());
        exit(1);
    }

    // Auto-detect by file extension
    std::string ext = fileExtension(path);
    if (ext == ".obj") {
        return scimesh::obj_io::read_obj(path);
    } else if (ext == ".ply") {
        return scimesh::ply_io::read_ply(path);
    } else if (ext == ".stl") {
        return scimesh::stl_io::read_stl(path);
    } else if (ext == ".white") {
        fs::Mesh fs_mesh;
        fs::read_surf(&fs_mesh, path);
        return scimesh::convert_fs_mesh(fs_mesh);
    } else {
        fprintf(stderr, "Error: unsupported file extension '%s'\n", ext.c_str());
        fprintf(stderr, "Supported formats: .obj, .ply, .stl, .white\n");
        fprintf(stderr, "Use --mesh-format fs for other FreeSurfer files (.pial, .sphere, etc.)\n");
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
    if (cfg.fogSpace == "ndc") {
        opts.fog_space = scimesh::FogSpace::NDC;
    } else {
        if (cfg.fogSpace != "world") {
            fprintf(stderr, "Warning: unknown fog space '%s', using world units.\n",
                    cfg.fogSpace.c_str());
        }
        opts.fog_space = scimesh::FogSpace::WORLD;   // distances in world units
    }

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

// Map a supported image file extension to its format name.  Returns true and
// sets `format` for ".png"/".ppm"/".bmp" (any case), false otherwise.
static bool imageFormatForExtension(const std::string& path, std::string& format) {
    const std::string ext = fileExtension(path);   // lowercase, includes the dot
    if (ext == ".png") { format = "png"; return true; }
    if (ext == ".ppm") { format = "ppm"; return true; }
    if (ext == ".bmp") { format = "bmp"; return true; }
    return false;
}

// Turn the requested output name into the path that is actually written.
// If the name already ends in a supported image extension, it is used as-is
// and defines the format (so `--output brain.png` does not become
// `brain.png.png`).  Otherwise the extension of `format` is appended.
// `format` is updated in place when the extension takes precedence.
static std::string resolveOutputPath(const std::string& filename,
                                     std::string& format) {
    std::string ext_format;
    if (imageFormatForExtension(filename, ext_format)) {
        if (ext_format != format) {
            fprintf(stderr, "Note: writing '%s' format (from the file extension), "
                            "ignoring --format %s.\n",
                    ext_format.c_str(), format.c_str());
        }
        format = ext_format;
        return filename;
    }
    return filename + "." + format;
}

static bool writeImage(const Image& img, const std::string& filename,
                       std::string format, std::string* path_written = nullptr) {
    const std::string path = resolveOutputPath(filename, format);
    bool ok = false;
    if (format == "png") {
        ok = img.write_png(path);
    } else if (format == "ppm") {
        ok = img.write_ppm(path);
    } else if (format == "bmp") {
        ok = img.write_bmp(path);
    } else {
        fprintf(stderr, "Error: unsupported format '%s'. Use png, ppm, or bmp.\n",
                format.c_str());
        return false;
    }
    if (ok && path_written != nullptr) {
        *path_written = path;
    }
    return ok;
}

// =========================================================================
// main
// =========================================================================

int main(int argc, char** argv) {
    // 1. Scan for --config to determine which config file to load
    std::string configPath = "config.toml";
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            configPath = argv[i + 1];
            break;
        }
    }

    // 2. Load config once
    AppConfig cfg = loadTOMLConfig(configPath);

    // 3. Apply CLI overrides (single pass, --config is a no-op here)
    applyCLIArgs(argc, argv, cfg);

    // 3b. Composite mode: load images, arrange, save, done.
    if (cfg.compositeMode) {
        if (cfg.compositeImages.empty()) {
            fprintf(stderr, "Error: --composite requires at least one image file.\n");
            fprintf(stderr, "Usage: %s --composite img1.png img2.png ... [--cols N] [--rows N] [--output name] [--format fmt] [--fit-mode pad|scale] [--bg-color R,G,B] [--crop] [--grow N]\n", argv[0]);
            return 1;
        }

        FitMode fm = (cfg.fitMode == "scale") ? FitMode::SCALE : FitMode::PAD;
        Color bg(cfg.bgColorR, cfg.bgColorG, cfg.bgColorB, 1.0f);

        std::vector<Image> images;
        for (const auto& path : cfg.compositeImages) {
            Image img = Image::read_image(path);
            if (img.width == 0) {
                fprintf(stderr, "Error: failed to load image '%s'\n", path.c_str());
                return 1;
            }
            // Pre-process each tile: crop uniform background border and/or add
            // padding, using the same --crop/--grow flags as the render path.
            if (cfg.cropToContent) {
                img.crop_to_content(CropContentDirection::ALL, bg);
            }
            if (cfg.growPx > 0) {
                img.grow(cfg.growPx, cfg.growPx, cfg.growPx, cfg.growPx, bg);
            }
            std::cout << "Loaded " << path << " (" << img.width << "×" << img.height << ")\n";
            images.push_back(std::move(img));
        }

        std::cout << "Arranging " << images.size() << " image(s) into grid"
                  << " (fit=" << cfg.fitMode << ")"
                  << "..." << std::endl;

        Image result = grid_arrange(images, cfg.compositeCols, cfg.compositeRows,
                                    fm, bg);

        std::cout << "Output: " << result.width << "×" << result.height << "\n";

        std::string written;
        bool ok = writeImage(result, cfg.filename, cfg.format, &written);
        if (ok) {
            std::cout << "Wrote " << written << std::endl;
        } else {
            std::cerr << "Error writing output image.\n";
            return 1;
        }
        std::cout << "Done.\n";
        return 0;
    }

    // 3c. Info mode: print image dimensions and exit.
    if (cfg.infoMode) {
        if (cfg.compositeImages.empty()) {
            fprintf(stderr, "Error: --info requires at least one image file.\n");
            fprintf(stderr, "Usage: %s --info img1.png img2.png ...\n", argv[0]);
            return 1;
        }
        for (const auto& path : cfg.compositeImages) {
            Image img = Image::read_image(path);
            if (img.width == 0) {
                fprintf(stderr, "Error: failed to load image '%s'\n", path.c_str());
                return 1;
            }
            printf("%s %dx%d\n", path.c_str(), img.width, img.height);
        }
        return 0;
    }

    // 4. Require at least one mesh file (text labels alone are allowed and are
    //    useful in screen space, e.g. to title a rendered or composed image)
    if (cfg.meshFiles.empty() && cfg.textStrings.empty()) {
        fprintf(stderr, "Error: no mesh file specified.\n");
        fprintf(stderr, "Set 'file' in [mesh] section of config.toml"
                        " or use --mesh <path>.\n");
        fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
        return 1;
    }

    // 5. Load meshes
    Scene scene;
    for (const auto& mf : cfg.meshFiles) {
        std::cout << "Loading mesh: " << mf << std::endl;
        Mesh mesh;
        try {
            mesh = loadMesh(mf, cfg.meshFormat);
        } catch (const std::exception& e) {
            fprintf(stderr, "Error loading mesh '%s': %s\n", mf.c_str(), e.what());
            return 1;
        }

        if (!mesh.is_valid()) {
            fprintf(stderr, "Error: loaded mesh '%s' is invalid.\n", mf.c_str());
            return 1;
        }
        std::cout << "  Vertices: " << mesh.vertices.size()
                  << ", triangles: " << mesh.triangles.size() << std::endl;

        // 6. Compute normals if needed
        if (cfg.computeNormals && mesh.normals.empty()) {
            std::cout << "  Computing vertex normals..." << std::endl;
            scimesh::compute_vertex_normals(mesh, mesh.normals);
        }

        scene.meshes.push_back(std::move(mesh));
    }
    std::cout << "Scene: " << scene.meshes.size() << " mesh(es) total." << std::endl;

    // 7. Set up camera with auto-framing (covers all meshes)
    Vec3 view_dir(cfg.viewDirX, cfg.viewDirY, cfg.viewDirZ);
    view_dir = glm::normalize(view_dir);
    Vec3 up(cfg.upX, cfg.upY, cfg.upZ);

    std::cout << "Setting up camera..." << std::endl;
    Camera cam;
    if (scene.meshes.empty()) {
        // Nothing to frame: only screen-space labels are meaningful here, and
        // those do not use the camera at all.
        std::cout << "  no meshes given, keeping the default camera" << std::endl;
    } else {
        cam = scimesh::camera_fit_scene(scene, view_dir, up, cfg.fov, cfg.margin);
        if (cfg.projection == "orthographic") {
            cam.projection = scimesh::ProjectionType::ORTHOGRAPHIC;
        }
    }

    std::cout << "  eye    = (" << cam.eye.x << ", " << cam.eye.y << ", "
              << cam.eye.z << ")\n";
    std::cout << "  center = (" << cam.center.x << ", " << cam.center.y
              << ", " << cam.center.z << ")\n";
    std::cout << "  fov    = " << cam.fov_degrees << "°\n";
    std::cout << "  projection = " << cfg.projection << "\n";

    // 8. Build render options
    RenderOptions opts = buildRenderOptions(cfg, cam);

    // 8b. Text labels (optional).  Positions are paired with the labels by
    //     index, so the n-th --text uses the n-th --text-at.
    if (!cfg.textStrings.empty()) {
        TextLayer layer;
        layer.strings = cfg.textStrings;
        layer.positions.reserve(cfg.textStrings.size());
        for (size_t i = 0; i < cfg.textStrings.size(); ++i) {
            if (i < cfg.textPositions.size()) {
                layer.positions.push_back(cfg.textPositions[i]);
            } else {
                if (cfg.textPositions.size() != cfg.textStrings.size()) {
                    fprintf(stderr, "Warning: no --text-at for text %zu, using (0,0,0).\n",
                            i + 1);
                }
                layer.positions.push_back(Vec3(0.0f, 0.0f, 0.0f));
            }
        }
        layer.colors.assign(cfg.textStrings.size(), cfg.textColor);
        layer.size = cfg.textSize;
        layer.font_file = cfg.textFont;
        layer.space = cfg.textScreenSpace ? TextSpace::SCREEN : TextSpace::WORLD;
        layer.adj = cfg.textAdj;
        layer.offset = cfg.textOffset;
        layer.depth_test = cfg.textDepthTest;
        layer.halo_color = cfg.textHaloColor;
        layer.halo_width = cfg.textHaloWidth;
        scene.add_texts(layer, Mat4(1.0f), "cli-text");

        std::cout << "Text: " << layer.strings.size() << " label(s), "
                  << layer.size << " px, "
                  << (cfg.textScreenSpace ? "screen" : "world") << " space"
                  << (cfg.textDepthTest ? "" : ", depth test off")
                  << (layer.halo_color.a > 0.0f ? ", halo" : "") << std::endl;
    }

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
    if (cfg.rotateDeg != 0) {
        int times = 0;
        if (cfg.rotateDeg == 90)       times = 1;
        else if (cfg.rotateDeg == 180) times = 2;
        else if (cfg.rotateDeg == 270) times = 3;
        else if (cfg.rotateDeg != 0) {
            fprintf(stderr, "Warning: --rotate must be 0, 90, 180, or 270, ignored.\n");
        }
        for (int i = 0; i < times; ++i) img.rotate_90(true);
    }
    if (cfg.cropToContent) {
        img.crop_to_content(CropContentDirection::ALL, opts.background_color);
    }
    if (cfg.growPx > 0) {
        img.grow(cfg.growPx, cfg.growPx, cfg.growPx, cfg.growPx,
                 opts.background_color);
    }

    // 11. Write output
    std::string outPath = cfg.filename;
    std::string written;
    bool ok = writeImage(img, outPath, cfg.format, &written);
    if (ok) {
        std::cout << "Wrote " << written << std::endl;
    } else {
        std::cerr << "Error writing output image.\n";
        return 1;
    }

    std::cout << "Done.\n";
    return 0;
}
