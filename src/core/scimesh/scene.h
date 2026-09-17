/// @file scene.h
/// @brief The Scene — a collection of meshes rendered together.

#pragma once

#include <scimesh/mesh.h>
#include <scimesh/lines.h>
#include <scimesh/text.h>
#include <scimesh/math_utils.h>
#include <string>

namespace scimesh {

/// @brief A lightweight, non-owning reference to one mesh in a scene, together
///        with its placement transform (and optional name).
///
/// Used by the renderer pipeline and by exporters (e.g. glTF) to consume a
/// scene without copying mesh data.  The mesh itself is owned by the `Scene`.
///
/// @see Scene, Scene::node(), Scene::nodes()
struct SceneNodeRef {
    const Mesh *mesh = nullptr;     ///< Pointer to the owned mesh (never null for a valid node).
    Mat4 transform = Mat4(1.0f);    ///< Model matrix placing the mesh in world space (identity = as-is).
    std::string name;               ///< Optional node name (used by exporters / debugging).
};

/// @brief A collection of `Mesh` objects to be rendered together.
///
/// A `Scene` is a list of meshes with optional per-mesh placement transforms
/// and names.  When rendered, meshes are drawn in order — later meshes appear
/// on top of earlier ones.  Each mesh's placement transform is applied as a
/// model matrix at render time (meshes themselves are never modified).
///
/// Next to meshes a scene can hold two kinds of screen-oriented decorations,
/// which create no geometry and are drawn after the meshes:
/// LineLayer objects (see `lines` / add_lines()) and text labels
/// (see `texts` / add_texts()).  Both are ignored by the bounding box and by
/// the mesh exporters.
///
/// ## Construction
///
/// @code{.cpp}
/// Scene scene;
/// scene.add(sphere);   // identity transform
/// scene.add(cube, glm::translate(Mat4(1.0f), Vec3(2.0f, 0.0f, 0.0f)));
/// scene.add(arrow_mesh, glm::rotate(Mat4(1.0f), 0.5f, Vec3(0,1,0)), "arrow");
/// @endcode
///
/// For convenience the raw `meshes` vector is still public and can be modified
/// directly (e.g. `scene.meshes.push_back(m)`); any mesh without a matching
/// transform is treated as identity.
///
/// ## Bounding box
///
/// The combined bounding box of all meshes (after their transforms) can be
/// computed for camera framing:
///
/// @code{.cpp}
/// Vec3 bmin, bmax;
/// scene.compute_bounding_box(bmin, bmax);
/// Camera cam = camera_look_at((bmin+bmax)*0.5f, ...);
/// @endcode
///
/// @see Mesh, SceneNodeRef, Renderer::render_scene(), camera_fit_scene()
struct Scene {
    /// @brief The meshes in this scene, drawn in order.
    std::vector<Mesh> meshes;

    /// @brief Per-mesh placement transforms, parallel to `meshes`.
    ///
    /// May be shorter than `meshes`; missing entries default to identity.
    std::vector<Mat4> transforms;

    /// @brief Optional per-mesh node names, parallel to `meshes`.
    std::vector<std::string> names;

    /// @brief Line layers drawn together with the meshes, after them.
    ///
    /// Line layers use a screen-space width and create no geometry, see
    /// LineLayer.  They are ignored by compute_bounding_box() and by the mesh
    /// exporters.
    std::vector<LineLayer> lines;

    /// @brief Per-layer placement transforms, parallel to `lines`.
    ///
    /// May be shorter than `lines`; missing entries default to identity.
    std::vector<Mat4> line_transforms;

    /// @brief Optional per-layer names, parallel to `lines`.
    std::vector<std::string> line_names;

    /// @brief Text layers drawn together with the meshes, after them.
    ///
    /// Text layers hold strings with world-space or screen-space positions and
    /// are drawn as billboards (see TextLayer).  Like line layers, they create
    /// no geometry and are ignored by compute_bounding_box() and by the mesh
    /// exporters, so adding labels never changes the camera framing.
    std::vector<TextLayer> texts;

    /// @brief Per-layer placement transforms, parallel to `texts`.
    ///
    /// May be shorter than `texts`; missing entries default to identity.  The
    /// transform is applied to the anchor positions and ignored for
    /// screen-space layers.
    std::vector<Mat4> text_transforms;

    /// @brief Optional per-layer names, parallel to `texts`.
    std::vector<std::string> text_names;

    /// @brief Add a mesh to the scene with an optional placement transform and name.
    ///
    /// The mesh's transparency flag is refreshed on the stored copy, so
    /// meshes whose `colors` (or `default_color`) have alpha < 1.0 are
    /// rendered translucently without any extra bookkeeping by the caller.
    ///
    /// @param mesh      The mesh to add (copied into the scene).
    /// @param transform Model matrix placing the mesh in world space
    ///                  (default: identity).
    /// @param name      Optional node name (used by exporters / debugging).
    void add(const Mesh &mesh, const Mat4 &transform = Mat4(1.0f),
             const std::string &name = "") {
        meshes.push_back(mesh);
        meshes.back().update_transparency();
        transforms.push_back(transform);
        names.push_back(name);
    }

    /// @brief Set the placement transform of the mesh at `index`.
    void set_transform(size_t index, const Mat4 &transform) {
        if (index >= meshes.size())
            return;
        while (transforms.size() <= index)
            transforms.push_back(Mat4(1.0f));
        transforms[index] = transform;
    }

    /// @brief Set the name of the mesh at `index`.
    void set_name(size_t index, const std::string &name) {
        if (index >= meshes.size())
            return;
        while (names.size() <= index)
            names.push_back("");
        names[index] = name;
    }

    /// @brief The placement transform of the mesh at `index` (identity when unset).
    const Mat4 &transform(size_t index) const {
        static const Mat4 kIdentity(1.0f);
        if (index >= transforms.size())
            return kIdentity;
        return transforms[index];
    }

    /// @brief The name of the mesh at `index` (empty when unset).
    const std::string &name(size_t index) const {
        static const std::string kEmpty;
        if (index >= names.size())
            return kEmpty;
        return names[index];
    }

    /// @brief Number of meshes in the scene.
    size_t size() const { return meshes.size(); }

    /// @brief Whether the scene holds no meshes.
    bool empty() const { return meshes.empty(); }

    /// @brief A non-owning reference to the mesh at `index` (mesh + transform + name).
    ///
    /// @pre `index < meshes.size()`
    SceneNodeRef node(size_t index) const {
        SceneNodeRef r;
        r.mesh = &meshes[index];
        r.transform = transform(index);
        r.name = name(index);
        return r;
    }

    /// @brief Non-owning references to all meshes, in draw order.
    std::vector<SceneNodeRef> nodes() const {
        std::vector<SceneNodeRef> out;
        out.reserve(meshes.size());
        for (size_t i = 0; i < meshes.size(); ++i)
            out.push_back(node(i));
        return out;
    }

    /// @brief Add a line layer to the scene with an optional placement
    ///        transform and name.
    ///
    /// Line layers are drawn after the meshes and use a screen-space width
    /// (see LineLayer).  They do not contribute to the bounding box, so they
    /// never change the camera framing.
    ///
    /// @param layer     The line layer to add (copied into the scene).
    /// @param transform Model matrix placing the lines in world space
    ///                  (default: identity).
    /// @param name      Optional node name (used by exporters / debugging).
    void add_lines(const LineLayer &layer, const Mat4 &transform = Mat4(1.0f),
                   const std::string &name = "") {
        lines.push_back(layer);
        line_transforms.push_back(transform);
        line_names.push_back(name);
    }

    /// @brief Set the placement transform of the line layer at `index`.
    void set_line_transform(size_t index, const Mat4 &transform) {
        if (index >= lines.size())
            return;
        while (line_transforms.size() <= index)
            line_transforms.push_back(Mat4(1.0f));
        line_transforms[index] = transform;
    }

    /// @brief The placement transform of the line layer at `index`
    ///        (identity when unset).
    const Mat4 &line_transform(size_t index) const {
        static const Mat4 kIdentity(1.0f);
        if (index >= line_transforms.size())
            return kIdentity;
        return line_transforms[index];
    }

    /// @brief The name of the line layer at `index` (empty when unset).
    const std::string &line_name(size_t index) const {
        static const std::string kEmpty;
        if (index >= line_names.size())
            return kEmpty;
        return line_names[index];
    }

    /// @brief Number of line layers in the scene.
    size_t line_count() const { return lines.size(); }

    /// @brief A non-owning reference to the line layer at `index`.
    ///
    /// @pre `index < lines.size()`
    LineNodeRef line_node(size_t index) const {
        LineNodeRef r;
        r.layer = &lines[index];
        r.transform = line_transform(index);
        r.name = line_name(index);
        return r;
    }

    /// @brief Non-owning references to all line layers, in draw order.
    std::vector<LineNodeRef> line_nodes() const {
        std::vector<LineNodeRef> out;
        out.reserve(lines.size());
        for (size_t i = 0; i < lines.size(); ++i)
            out.push_back(line_node(i));
        return out;
    }

    /// @brief Add a text layer to the scene with an optional placement
    ///        transform and name.
    ///
    /// Text layers are drawn after the meshes and the line layers (see
    /// TextLayer).  They do not contribute to the bounding box, so they never
    /// change the camera framing.
    ///
    /// @param layer     The text layer to add (copied into the scene).
    /// @param transform Model matrix applied to the anchor positions of the
    ///                  layer (default: identity).
    /// @param name      Optional node name (used for debugging).
    void add_texts(const TextLayer &layer, const Mat4 &transform = Mat4(1.0f),
                   const std::string &name = "") {
        texts.push_back(layer);
        text_transforms.push_back(transform);
        text_names.push_back(name);
    }

    /// @brief Set the placement transform of the text layer at `index`.
    void set_text_transform(size_t index, const Mat4 &transform) {
        if (index >= texts.size())
            return;
        while (text_transforms.size() <= index)
            text_transforms.push_back(Mat4(1.0f));
        text_transforms[index] = transform;
    }

    /// @brief The placement transform of the text layer at `index`
    ///        (identity when unset).
    const Mat4 &text_transform(size_t index) const {
        static const Mat4 kIdentity(1.0f);
        if (index >= text_transforms.size())
            return kIdentity;
        return text_transforms[index];
    }

    /// @brief The name of the text layer at `index` (empty when unset).
    const std::string &text_name(size_t index) const {
        static const std::string kEmpty;
        if (index >= text_names.size())
            return kEmpty;
        return text_names[index];
    }

    /// @brief Number of text layers in the scene.
    size_t text_count() const { return texts.size(); }

    /// @brief A non-owning reference to the text layer at `index`.
    ///
    /// @pre `index < texts.size()`
    TextNodeRef text_node(size_t index) const {
        TextNodeRef r;
        r.layer = &texts[index];
        r.transform = text_transform(index);
        r.name = text_name(index);
        return r;
    }

    /// @brief Non-owning references to all text layers, in draw order.
    std::vector<TextNodeRef> text_nodes() const {
        std::vector<TextNodeRef> out;
        out.reserve(texts.size());
        for (size_t i = 0; i < texts.size(); ++i)
            out.push_back(text_node(i));
        return out;
    }

    /// @brief Compute the combined axis-aligned bounding box of all meshes,
    ///        after applying each mesh's placement transform.
    /// Iterates over all meshes, transforms the 8 corners of each mesh's
    /// bounding box, and computes the union.  Empty meshes are skipped.
    ///
    /// @param[out] min_bound  Minimum corner of the combined AABB.
    /// @param[out] max_bound  Maximum corner of the combined AABB.
    ///
    /// @see Mesh::compute_bounding_box()
    void compute_bounding_box(Vec3 &min_bound, Vec3 &max_bound) const {
        bool first = true;
        for (size_t i = 0; i < meshes.size(); ++i) {
            const Mesh &mesh = meshes[i];
            if (mesh.vertices.empty())
                continue;
            Vec3 mesh_min, mesh_max;
            mesh.compute_bounding_box(mesh_min, mesh_max);
            const Mat4 &m = transform(i);
            for (int c = 0; c < 8; ++c) {
                Vec3 corner(
                    (c & 1) ? mesh_max.x : mesh_min.x,
                    (c & 2) ? mesh_max.y : mesh_min.y,
                    (c & 4) ? mesh_max.z : mesh_min.z);
                Vec3 t = transform_point(m, corner);
                if (first) {
                    min_bound = t;
                    max_bound = t;
                    first = false;
                } else {
                    min_bound = glm::min(min_bound, t);
                    max_bound = glm::max(max_bound, t);
                }
            }
        }
        if (first) {  // scene empty or all meshes empty
            min_bound = Vec3(0.0f);
            max_bound = Vec3(0.0f);
        }
    }
};

} // namespace scimesh
