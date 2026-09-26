/// Demo: text labels (TextLayer) — annotating scientific figures.
///
/// Shows what you need for figure annotations:
///
///   1. **World-space labels** that stick to a 3D location and follow the
///      camera (a label on a brain region, or on an atom).
///   2. **Occlusion**: a label anchored *behind* the rendered geometry is
///      hidden by default (TextLayer::depth_test), so an atom label on the far
///      side of a molecule does not shine through it.
///   3. **Screen-space labels** for everything that belongs to the figure
///      layout rather than to the 3D scene: titles, panel tags, captions.
///   4. **Halos** (outlines), which keep labels readable on dark or busy
//      geometry, and **rotation**, which turns a label into a vertical axis
//      label or makes it follow an annotation line.
///
/// Labels are billboards: they always face the camera and keep their physical
/// size, so they stay readable from every viewpoint — unlike 3D text geometry,
/// which skews as the camera moves.
///
/// Build:
///   cd examples/cpp/text_labels && mkdir -p build && cd build
///   cmake .. -DCMAKE_BUILD_TYPE=Release && make
///
/// Run (from build/):
///   ./text_labels
///
/// Output (in the current directory):
///   text_labels_brain.png    two hemispheres, world + screen labels, 2 views
///   text_labels_atoms.png    molecule with element labels + occlusion demo
///   text_labels_screen.png   figure layout labels on a dark background
///
/// The font is the bundled Inter-Regular.ttf; set the SCIMESH_FONT environment
/// variable or TextLayer::font_file to use any other .ttf file.

#include <scimesh/scene.h>
#include <scimesh/text.h>
#include <scimesh/font.h>
#include <scimesh/renderer.h>
#include <scimesh/camera.h>
#include <scimesh/image.h>
#include <scimesh/primitives.h>
#include <scimesh/types.h>

#include <cstdio>
#include <string>
#include <vector>

using scimesh::Camera;
using scimesh::Color;
using scimesh::FitMode;
using scimesh::Font;
using scimesh::Image;
using scimesh::Mat4;
using scimesh::RenderOptions;
using scimesh::Renderer;
using scimesh::Scene;
using scimesh::TextDrawStyle;
using scimesh::TextLayer;
using scimesh::TextSpace;
using scimesh::Vec2;
using scimesh::Vec3;

namespace {

/// Save an image and report the result.
int save(const Image &image, const std::string &filename) {
    if (!image.write_png(filename)) {
        std::printf("  ERROR: failed to write %s\n", filename.c_str());
        return 1;
    }
    std::printf("  wrote %s (%dx%d)\n", filename.c_str(), image.width, image.height);
    return 0;
}

/// Render options used by all demos: anti-aliased, on a plain background.
RenderOptions demo_options(int width, int height, const Color &background) {
    RenderOptions options;
    options.width = width;
    options.height = height;
    options.background_color = background;
    options.aa_samples = 2;  // labels are anti-aliased along with the meshes
    return options;
}

/// Draw a small label at an absolute pixel position of an image.
///
/// This is the low-level variant (draw_text): it works on any image, including
/// one that was already composed from several panels, and needs neither a camera
/// nor a scene.
void stamp_text(Image &image, const std::string &text, float size, float x,
                float y_top, const Color &color, const Color &halo,
                float halo_width = 0.0f) {
    const Font font = scimesh::cached_font("", size);
    TextDrawStyle style;
    style.color = color;
    style.halo_color = halo;
    style.halo_width = halo_width;
    // y_top is the top edge of the text box; the baseline sits ascent below it.
    scimesh::draw_text(image, text, font, x, y_top + font.metrics().ascent, style);
}

// ---------------------------------------------------------------------------
//  Demo 1: world-space labels on a brain-like scene
// ---------------------------------------------------------------------------

int demo_brain() {
    std::printf("Demo 1: world-space labels (hemisphere view)\n");

    Scene scene;
    scene.add(scimesh::generate_sphere(Vec3(-1.8f, 0.0f, 0.0f), 2.0f, 48,
                                       Color(0.56f, 0.62f, 0.76f)),
              Mat4(1.0f), "left");
    scene.add(scimesh::generate_sphere(Vec3(1.8f, 0.0f, 0.0f), 2.0f, 48,
                                       Color(0.63f, 0.69f, 0.81f)),
              Mat4(1.0f), "right");

    // Labels anchored in the 3D scene.  `adj` puts the bottom centre of the text
    // box onto the anchor point.  "anterior"/"posterior" are direction
    // annotations that must stay readable, so they opt out of the depth test.
    TextLayer labels;
    labels.strings = {"left", "right", "anterior", "posterior"};
    labels.positions = {Vec3(-1.8f, 2.3f, 0.0f), Vec3(1.8f, 2.3f, 0.0f),
                        Vec3(4.2f, 0.0f, 0.0f), Vec3(-4.2f, 0.0f, 0.0f)};
    labels.colors = {Color(0.12f, 0.12f, 0.16f, 1.0f), Color(0.12f, 0.12f, 0.16f, 1.0f),
                     Color(0.72f, 0.15f, 0.15f, 1.0f), Color(0.15f, 0.25f, 0.72f, 1.0f)};
    labels.size = 22.0f;
    labels.adj = Vec2(0.5f, 0.0f);
    labels.depth_test = false;
    scene.add_texts(labels);

    // Screen-space caption in the top left corner: fixed pixels, unaffected by
    // the camera, and multi-line text works with '\n'.
    TextLayer caption;
    caption.strings = {"world-space labels\n(billboarded, follow the camera)"};
    caption.positions = {Vec3(16.0f, 14.0f, 0.0f)};
    caption.colors = {Color(0.25f, 0.25f, 0.28f, 1.0f)};
    caption.space = TextSpace::SCREEN;
    caption.adj = Vec2(0.0f, 1.0f);  // anchor at the top left of the text box
    caption.size = 18.0f;
    caption.line_spacing = 1.35f;
    scene.add_texts(caption);

    // Text layers are ignored by the camera framing: the meshes alone decide
    // where the camera goes.
    const Camera cam = scimesh::camera_fit_scene(scene, Vec3(0.0f, 0.0f, 1.0f),
                                                Vec3(0.0f, 1.0f, 0.0f), 45.0f, 1.18f);
    const Camera orbited = scimesh::camera_orbit(cam, Vec3(0.0f, 1.0f, 0.0f), 55.0f);

    Renderer renderer;
    const RenderOptions options = demo_options(620, 480, Color(1.0f, 1.0f, 1.0f));
    const std::vector<Image> panels = {renderer.render_scene(scene, cam, options),
                                       renderer.render_scene(scene, orbited, options)};
    Image both = scimesh::stack_horizontal(panels, FitMode::PAD,
                                           Color(1.0f, 1.0f, 1.0f));

    // Panel tags are easiest to add after composing, with draw_text().
    const Color black(0.0f, 0.0f, 0.0f, 1.0f);
    const Color no_halo(0.0f, 0.0f, 0.0f, 0.0f);
    stamp_text(both, "A", 26.0f, 14.0f, 8.0f, black, no_halo);
    stamp_text(both, "B", 26.0f, 634.0f, 8.0f, black, no_halo);

    return save(both, "text_labels_brain.png");
}

// ---------------------------------------------------------------------------
//  Demo 2: atom labels and occlusion
// ---------------------------------------------------------------------------

/// Build a small molecule.  The sulfur atom sits *behind* the central carbon
/// atom, as seen from the camera in demo_atoms().
Scene molecule() {
    const Vec3 c(0.0f, 0.0f, 0.0f), n(1.5f, 0.85f, 0.3f), o(-1.45f, 0.7f, -0.35f),
        c2(0.25f, -1.5f, 0.25f), s(0.0f, 0.0f, -1.2f);
    const Color c_gray(0.36f, 0.36f, 0.40f), n_blue(0.25f, 0.35f, 0.85f),
        o_red(0.85f, 0.30f, 0.25f), s_yellow(0.85f, 0.72f, 0.20f);
    const Color bond_color(0.62f, 0.62f, 0.65f);

    Scene scene;
    scene.add(scimesh::generate_multi_spheres({c, n, o, c2, s},
                                             {0.70f, 0.55f, 0.50f, 0.70f, 0.45f},
                                             {c_gray, n_blue, o_red, c_gray, s_yellow}, 32),
              Mat4(1.0f), "atoms");
    scene.add(scimesh::generate_multi_cylinders(
                  {c, c, c, c}, {n, o, c2, s}, {0.10f, 0.10f, 0.10f, 0.10f},
                  {bond_color, bond_color, bond_color, bond_color}, 16, false),
              Mat4(1.0f), "bonds");
    return scene;
}

/// Element symbols, anchored on the atom centres and nudged up and to the right
/// by a pixel offset so that they do not cover the atoms.
TextLayer element_labels(bool show_occluded) {
    TextLayer layer;
    layer.strings = {"C", "N", "O", "C", "S"};
    layer.positions = {Vec3(0.0f, 0.0f, 0.0f), Vec3(1.5f, 0.85f, 0.3f),
                       Vec3(-1.45f, 0.7f, -0.35f), Vec3(0.25f, -1.5f, 0.25f),
                       Vec3(0.0f, 0.0f, -1.2f)};
    layer.colors = std::vector<Color>(5, Color(0.15f, 0.15f, 0.18f, 1.0f));
    layer.size = 20.0f;
    layer.adj = Vec2(0.0f, 0.5f);      // anchor at the left, vertically centred
    layer.offset = Vec2(9.0f, -9.0f);  // ...then nudge up and to the right
    layer.halo_color = Color(1.0f, 1.0f, 1.0f, 0.85f);  // readable on the atoms
    layer.halo_width = 1.5f;
    layer.depth_test = !show_occluded;
    return layer;
}

int demo_atoms() {
    std::printf("Demo 2: element labels next to atoms (with occlusion)\n");

    const Camera cam = scimesh::camera_look_at(Vec3(0.0f, 0.0f, 0.0f), 3.0f,
                                              Vec3(0.15f, 0.10f, 1.0f),
                                              Vec3(0.0f, 1.0f, 0.0f), 45.0f, 1.15f);
    Renderer renderer;
    const RenderOptions options = demo_options(560, 440, Color(1.0f, 1.0f, 1.0f));

    // Panel A: default behaviour.  The sulfur label is skipped, because its
    // anchor is behind the carbon atom.
    Scene hidden = molecule();
    hidden.add_texts(element_labels(false));
    Image panel_a = renderer.render_scene(hidden, cam, options);

    // Panel B: depth_test = FALSE.  Labels are always drawn, so the sulfur label
    // shines through the carbon atom — usually not what you want, but the right
    // choice when annotating a point that lies *on* a surface.
    Scene visible = molecule();
    visible.add_texts(element_labels(true));
    Image panel_b = renderer.render_scene(visible, cam, options);

    stamp_text(panel_a, "depth_test = TRUE", 17.0f, 14.0f, 12.0f,
               Color(0.10f, 0.35f, 0.10f, 1.0f), Color(1.0f, 1.0f, 1.0f, 0.9f), 2.0f);
    stamp_text(panel_a, "the S label is hidden", 15.0f, 14.0f, 34.0f,
               Color(0.25f, 0.25f, 0.28f, 1.0f), Color(1.0f, 1.0f, 1.0f, 0.9f), 2.0f);
    stamp_text(panel_b, "depth_test = FALSE", 17.0f, 14.0f, 12.0f,
               Color(0.60f, 0.15f, 0.15f, 1.0f), Color(1.0f, 1.0f, 1.0f, 0.9f), 2.0f);
    stamp_text(panel_b, "the S label shines through", 15.0f, 14.0f, 34.0f,
               Color(0.25f, 0.25f, 0.28f, 1.0f), Color(1.0f, 1.0f, 1.0f, 0.9f), 2.0f);

    Image both = scimesh::stack_horizontal({panel_a, panel_b}, FitMode::PAD,
                                           Color(1.0f, 1.0f, 1.0f));
    return save(both, "text_labels_atoms.png");
}

// ---------------------------------------------------------------------------
//  Demo 3: screen-space labels (figure layout)
// ---------------------------------------------------------------------------

int demo_screen_space() {
    std::printf("Demo 3: screen-space labels (titles, tags, captions)\n");

    Scene scene;
    scene.add(scimesh::generate_sphere(Vec3(0.0f, 0.0f, 0.0f), 2.4f, 64,
                                       Color(0.78f, 0.48f, 0.26f)),
              Mat4(1.0f), "object");

    // Screen space is measured in output pixels from the top left corner, so a
    // label does not care about the camera at all.  `size` and `adj` are
    // properties of a layer (like the width of a LineLayer), so labels with
    // different sizes or anchors live in different layers.
    TextLayer title;
    title.space = TextSpace::SCREEN;
    title.strings = {"screen-space labels"};
    title.positions = {Vec3(24.0f, 22.0f, 0.0f)};
    title.colors = {Color(0.95f, 0.95f, 0.96f, 1.0f)};
    title.size = 26.0f;
    title.adj = Vec2(0.0f, 1.0f);
    title.depth_test = false;
    title.halo_color = Color(0.0f, 0.0f, 0.0f, 0.9f);
    title.halo_width = 1.5f;
    scene.add_texts(title);

    TextLayer tag;
    tag.space = TextSpace::SCREEN;
    tag.strings = {"A"};
    tag.positions = {Vec3(696.0f, 18.0f, 0.0f)};
    tag.colors = {Color(0.95f, 0.95f, 0.96f, 1.0f)};
    tag.size = 30.0f;
    tag.adj = Vec2(1.0f, 1.0f);
    tag.depth_test = false;
    tag.halo_color = Color(0.0f, 0.0f, 0.0f, 0.9f);
    tag.halo_width = 1.5f;
    scene.add_texts(tag);

    TextLayer caption;
    caption.space = TextSpace::SCREEN;
    caption.strings = {"measured in output pixels from the top-left corner.\n"
                       "Use this space for titles, panel tags and captions."};
    caption.positions = {Vec3(24.0f, 498.0f, 0.0f)};
    caption.colors = {Color(0.82f, 0.83f, 0.86f, 1.0f)};
    caption.size = 17.0f;
    caption.adj = Vec2(0.0f, 0.0f);  // anchor at the bottom left of the text box
    caption.line_spacing = 1.35f;
    caption.depth_test = false;
    scene.add_texts(caption);

    TextLayer right_aligned;
    right_aligned.space = TextSpace::SCREEN;
    right_aligned.strings = {"right-aligned, 1 px halo"};
    right_aligned.positions = {Vec3(696.0f, 498.0f, 0.0f)};
    right_aligned.colors = {Color(0.82f, 0.83f, 0.86f, 1.0f)};
    right_aligned.size = 15.0f;
    right_aligned.adj = Vec2(1.0f, 0.0f);  // anchor at the bottom right
    right_aligned.depth_test = false;
    scene.add_texts(right_aligned);

    // Rotation turns a label around its anchor: 90 degrees reads bottom to top,
    // which is the usual orientation of a y-axis label.
    TextLayer vertical;
    vertical.space = TextSpace::SCREEN;
    vertical.strings = {"rotated 90 degrees (reads bottom to top)"};
    vertical.positions = {Vec3(26.0f, 258.0f, 0.0f)};
    vertical.colors = {Color(0.82f, 0.83f, 0.86f, 1.0f)};
    vertical.size = 15.0f;
    vertical.adj = Vec2(0.0f, 0.5f);  // anchor at the start of the text
    vertical.rotation = 90.0f;
    vertical.depth_test = false;
    scene.add_texts(vertical);

    const Camera cam = scimesh::camera_fit_scene(scene, Vec3(0.0f, 0.0f, 1.0f),
                                                Vec3(0.0f, 1.0f, 0.0f), 45.0f, 1.15f);
    Renderer renderer;
    const RenderOptions options = demo_options(720, 520, Color(0.13f, 0.14f, 0.17f));

    Image image = renderer.render_scene(scene, cam, options);
    return save(image, "text_labels_screen.png");
}

} // namespace

int main() {
    const Font font = scimesh::cached_font("", 16.0f);
    std::printf("scimesh text label demo\n  font: %s (%s, %.0f px)\n",
                font.family_name().c_str(), font.source_path().c_str(),
                font.pixel_size());

    int errors = 0;
    errors += demo_brain();
    errors += demo_atoms();
    errors += demo_screen_space();

    std::printf("%s\n", errors == 0 ? "All demos finished." : "Some demos failed.");
    return errors == 0 ? 0 : 1;
}
