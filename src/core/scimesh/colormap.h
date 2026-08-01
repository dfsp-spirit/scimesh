/// @file colormap.h
/// @brief Colormap data structure and data-to-color mapping utilities.
///
/// Provides a self-contained `ColorMap` type for sampling colors at
/// normalized positions, and the `apply_colormap()` family of functions
/// that map per-vertex (or per-element) scalar data to RGBA colors.
///
/// @par Key features
/// - **Self-contained**: `ColorMap` owns its LUT, no external colormap
///   library required.
/// - **Multi-dataset**: apply a single colormap across multiple data
///   vectors (e.g., two brain hemispheres) with an optional shared range.
/// - **NaN handling**: NaN/Inf values are mapped to a configurable color.
/// - **Winsorizing**: clip outliers by percentile before mapping.
/// - **scibar interop**: `ColorMap::from_uint8_colors()` duck-typed
///   factory converts scibar colormaps without any `#include` dependency
///   on scibar headers.
///
/// @par Quick start
/// @code{.cpp}
/// #include <scimesh/colormap.h>
///
/// // Build a colormap from scratch (linear blue→red gradient, 8 entries)
/// ColorMap cmap;
/// cmap.colors = {
///     Color(0.0f, 0.0f, 0.5f), Color(0.1f, 0.1f, 0.7f),
///     Color(0.3f, 0.2f, 0.9f), Color(0.5f, 0.3f, 1.0f),
///     Color(0.7f, 0.3f, 0.8f), Color(0.9f, 0.2f, 0.5f),
///     Color(1.0f, 0.1f, 0.3f), Color(1.0f, 0.0f, 0.0f)
/// };
///
/// // Per-vertex thickness data with some missing values (NaN = medial wall)
/// std::vector<float> thickness = {2.3f, 2.1f, NAN, 3.4f, 2.9f, NAN, ...};
///
/// // Apply colormap: auto-range, white for NaN, clip 2nd/98th percentiles
/// auto result = apply_colormap(thickness, cmap, NAN, NAN,
///                               Color(1,1,1,1),  // white NaN
///                               2.0f, 98.0f);    // winsorize
///
/// // result.colors[i] is the color for thickness[i]
/// // result.data_min / data_max hold the effective range (use for colorbar)
/// @endcode
///
/// @par scibar interop (no coupling)
/// @code{.cpp}
/// #include <scibar.hpp>   // user includes scibar, not scimesh
/// #include <scimesh/colormap.h>
///
/// auto scibar_cmap = scibar::util::viridis();        // std::vector<scibar::Color>
/// auto scm_cmap = ColorMap::from_uint8_colors(scibar_cmap);  // duck-typed copy
/// auto result = apply_colormap(data, scm_cmap);
/// @endcode

#pragma once

#include <scimesh/types.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstddef>

namespace scimesh {

// =========================================================================
// ColorMap
// =========================================================================

/// @brief Self-contained colormap — owns a colour lookup table and
/// supports linearly interpolated sampling.
///
/// A `ColorMap` is simply a vector of `Color` entries plus a `sample()`
/// method that performs linear interpolation between adjacent entries.
/// The first entry corresponds to `t = 0.0`, the last to `t = 1.0`.
///
/// @note The minimum useful size is 2 entries. Single-entry colormaps
/// always return that single colour regardless of `t`.
///
/// @see apply_colormap(), from_uint8_colors()
struct ColorMap {
    /// @brief The colour lookup table entries in [0, 1] float RGBA.
    std::vector<Color> colors;

    /// @brief Sample the colormap at a normalized position using linear
    /// interpolation.
    ///
    /// @param t Normalized position in [0, 1]. Values outside this range
    ///          are clamped: `t < 0` → first entry, `t > 1` → last entry.
    /// @return The interpolated colour at position `t`, or transparent
    ///         black if the colormap is empty.
    ///
    /// @par Example
    /// @code
    /// Color lo  = cmap.sample(0.0f);  // first colour
    /// Color mid = cmap.sample(0.5f);  // middle
    /// Color hi  = cmap.sample(1.0f);  // last colour
    /// @endcode
    Color sample(float t) const;

    /// @brief Number of entries in the colormap.
    size_t size() const { return colors.size(); }

    /// @brief True if the colormap has no entries.
    bool empty() const { return colors.empty(); }

    // ---------------------------------------------------------------------
    //  Factory: scibar interop (duck-typed, no scibar dependency)
    // ---------------------------------------------------------------------

    /// @brief Build a `ColorMap` from a vector of scibar-style uint8_t
    /// RGBA colours.
    ///
    /// This is a **duck-typed** template — it works with any type `T`
    /// that has `.r`, `.g`, `.b`, `.a` fields as `uint8_t` in the range
    /// [0, 255].  No `#include` of scibar headers is required; the user
    /// simply passes their `std::vector<scibar::Color>` and it compiles.
    ///
    /// @tparam T Any struct with `uint8_t r, g, b, a` fields.
    /// @param cmap The source colormap (e.g., from `scibar::util::viridis()`).
    /// @return A new `ColorMap` with the same entries converted to float
    ///         [0, 1] RGBA.
    ///
    /// @par Example (scibar → scimesh)
    /// @code
    /// auto scibar_cmap = scibar::util::viridis();
    /// auto scm_cmap = ColorMap::from_uint8_colors(scibar_cmap);
    /// @endcode
    template<typename T>
    static ColorMap from_uint8_colors(const std::vector<T>& cmap) {
        ColorMap out;
        out.colors.reserve(cmap.size());
        for (const auto& c : cmap) {
            out.colors.emplace_back(
                c.r / 255.0f, c.g / 255.0f,
                c.b / 255.0f, c.a / 255.0f);
        }
        return out;
    }

    /// @brief Build a `ColorMap` from a flat interleaved RGB(A) byte array.
    ///
    /// @param data   Flat array of RGB bytes (3 × n entries) or RGBA bytes
    ///               (4 × n entries).
    /// @param channels 3 for RGB data, 4 for RGBA data.
    /// @return A new `ColorMap` with `data.size() / channels` entries.
    ///
    /// @par Example (libfs viridis output)
    /// @code
    /// std::vector<uint8_t> rgb = fs::util::viridis(sulc_data);
    /// auto cmap = ColorMap::from_interleaved(rgb, 3);
    /// @endcode
    static ColorMap from_interleaved(const std::vector<uint8_t>& data,
                                     int channels = 3) {
        ColorMap out;
        size_t n = data.size() / static_cast<size_t>(channels);
        out.colors.reserve(n);
        for (size_t i = 0; i < n; i++) {
            float r = data[i * channels] / 255.0f;
            float g = data[i * channels + 1] / 255.0f;
            float b = data[i * channels + 2] / 255.0f;
            float a = (channels >= 4) ? data[i * channels + 3] / 255.0f : 1.0f;
            out.colors.emplace_back(r, g, b, a);
        }
        return out;
    }

    /// @brief The Viridis perceptually-uniform sequential colormap
    /// (256 entries).
    ///
    /// Returns a 256-entry RGBA LUT. Viridis is perceptually uniform,
    /// colourblind-friendly, and prints well in greyscale.  Identical
    /// to the matplotlib reference implementation.
    ///
    /// @return A 256-entry `ColorMap` (cached after first call).
    static const ColorMap& viridis();
};

// =========================================================================
// ApplyColormapResult — single dataset
// =========================================================================

/// @brief Result of `apply_colormap()` for a single dataset.
///
/// Contains the mapped colours plus metadata about the effective data
/// range, so that a matching colourbar can be created.
///
/// @see apply_colormap(), MultiApplyColormapResult
struct ApplyColormapResult {
    /// @brief Per-element RGBA colours, same order and length as the
    /// input data vector.
    std::vector<Color> colors;

    /// @brief The actual data range used for mapping (after winsorizing,
    /// or as explicitly set).  Use this for colourbar scale limits.
    float data_min = NAN;

    /// @brief The actual data range used for mapping (after winsorizing,
    /// or as explicitly set).  Use this for colourbar scale limits.
    float data_max = NAN;

    /// @brief Raw finite minimum of the input data (before winsorizing).
    /// `NAN` if all values were NaN/Inf.
    float raw_min = NAN;

    /// @brief Raw finite maximum of the input data (before winsorizing).
    /// `NAN` if all values were NaN/Inf.
    float raw_max = NAN;

    /// @brief Lower percentile cutoff used for winsorizing.
    /// `NAN` if no lower winsorizing was applied.
    float winsor_lo = NAN;

    /// @brief Upper percentile cutoff used for winsorizing.
    /// `NAN` if no upper winsorizing was applied.
    float winsor_hi = NAN;

    /// @brief Number of NaN / Inf values in the input data.
    size_t nan_count = 0;
};

// =========================================================================
// MultiApplyColormapResult — multiple datasets
// =========================================================================

/// @brief Aggregate result of `apply_colormap()` for multiple datasets.
///
/// Contains per-dataset colour vectors PLUS pooled statistics across all
/// datasets.  The pooled fields are always computed, regardless of whether
/// `global_range` was true or false.
///
/// @see apply_colormap(), ApplyColormapResult
struct MultiApplyColormapResult {
    /// @brief One result per input dataset, in the same order.
    std::vector<ApplyColormapResult> per_dataset;

    /// @brief Pooled effective data range (post-winsorizing) across all
    /// datasets.  Use this for a shared colourbar.
    float pooled_data_min = NAN;

    /// @brief Pooled effective data range (post-winsorizing) across all
    /// datasets.  Use this for a shared colourbar.
    float pooled_data_max = NAN;

    /// @brief Pooled raw finite minimum across all datasets.
    float pooled_raw_min = NAN;

    /// @brief Pooled raw finite maximum across all datasets.
    float pooled_raw_max = NAN;

    /// @brief Pooled lower percentile cutoff.
    float pooled_winsor_lo = NAN;

    /// @brief Pooled upper percentile cutoff.
    float pooled_winsor_hi = NAN;

    /// @brief Total number of NaN / Inf values across all datasets.
    size_t total_nan_count = 0;
};

// =========================================================================
// apply_colormap — function declarations
// =========================================================================

/// @brief Map a single vector of numeric data to RGBA colours using a
/// colormap.
///
/// Each element of `data` is normalised to [0, 1] based on the effective
/// data range and then mapped through the colormap.  NaN / Inf values
/// receive `nan_color`.
///
/// @param data       Per-vertex (or per-element) numeric values. NaN / Inf
///                   allowed — they will be mapped to `nan_color`.
/// @param colormap   The colour lookup table to sample from.
/// @param vmin       Lower bound of the data range. `NAN` = auto-detect
///                   from finite values (after winsorizing, if applicable).
/// @param vmax       Upper bound of the data range. `NAN` = auto-detect.
/// @param nan_color  RGBA colour for NaN / Inf positions.
/// @param lo_pct     Lower percentile for winsorizing (0.0 = off).
///                   E.g., `2.0` → clip values below the 2nd percentile.
/// @param hi_pct     Upper percentile for winsorizing (100.0 = off).
///                   E.g., `98.0` → clip values above the 98th percentile.
///
/// @return An `ApplyColormapResult` with the mapped colours and metadata.
///
/// @par Example — single dataset with winsorizing
/// @code
/// std::vector<float> data = {1.2f, 3.4f, NAN, 2.1f, 99.0f, 2.8f};
/// ColorMap cmap = build_viridis();  // user-supplied 256-entry colormap
///
/// auto r = apply_colormap(data, cmap, NAN, NAN,
///                          Color(1,1,1,1),  // white NaN
///                          5.0f, 95.0f);    // clip 5th/95th percentiles
///
/// // r.colors[i]      — colour for data[i]
/// // r.data_min/max   — effective range (after winsorizing) → use for colourbar
/// // r.winsor_lo/hi   — the actual percentile cutoff values
/// @endcode
ApplyColormapResult apply_colormap(
    const std::vector<float>& data,
    const ColorMap& colormap,
    float vmin       = NAN,
    float vmax       = NAN,
    const Color& nan_color = Color{0.5f, 0.5f, 0.5f, 1.0f},
    float lo_pct     = 0.0f,
    float hi_pct     = 100.0f
);

/// @brief Map multiple vectors of numeric data through a single colormap.
///
/// This is the multi-dataset overload.  Each dataset (e.g., per-vertex
/// data for the left and right brain hemispheres) is mapped independently,
/// and pooled statistics across all datasets are always computed.
///
/// @param datasets     One or more per-vertex (or per-element) data vectors.
/// @param colormap     The colour lookup table.
/// @param vmin         Lower bound. `NAN` = auto-detect.
/// @param vmax         Upper bound. `NAN` = auto-detect.
/// @param nan_color    RGBA colour for NaN / Inf positions.
/// @param lo_pct       Lower percentile for winsorizing (0.0 = off).
/// @param hi_pct       Upper percentile for winsorizing (100.0 = off).
/// @param global_range If `true`, compute `vmin`/`vmax` from all datasets
///                     pooled together, so both hemispheres use the same
///                     colour scale.  If `false`, each dataset gets its
///                     own independent range (unless `vmin`/`vmax` are
///                     explicitly set, which overrides everything).
///
/// @return A `MultiApplyColormapResult` with per-dataset colours and
///         pooled metadata.
///
/// @par Example — two hemispheres, shared colour scale
/// @code
/// std::vector<float> lh_thickness = {2.3f, 2.1f, NAN, ...};
/// std::vector<float> rh_thickness = {2.5f, 2.0f, NAN, ...};
///
/// auto r = apply_colormap({lh_thickness, rh_thickness}, cmap,
///                          NAN, NAN,
///                          Color(1,1,1,1),  // white NaN (medial wall)
///                          2.0f, 98.0f,     // clip outliers
///                          true);            // shared range
///
/// // r.per_dataset[0].colors  — colours for left hemisphere
/// // r.per_dataset[1].colors  — colours for right hemisphere
/// // r.pooled_data_min/max    — shared range → use for colourbar
/// @endcode
MultiApplyColormapResult apply_colormap(
    const std::vector<std::vector<float>>& datasets,
    const ColorMap& colormap,
    float vmin       = NAN,
    float vmax       = NAN,
    const Color& nan_color = Color{0.5f, 0.5f, 0.5f, 1.0f},
    float lo_pct     = 0.0f,
    float hi_pct     = 100.0f,
    bool  global_range = false
);

} // namespace scimesh
