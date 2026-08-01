#include "catch_amalgamated.hpp"
#include <scimesh/colormap.h>
#include <cmath>

using namespace scimesh;
using Catch::Approx;

// =========================================================================
// ColorMap::sample
// =========================================================================

TEST_CASE("ColorMap::sample with single entry returns that entry", "[colormap]") {
    ColorMap cmap;
    cmap.colors = { Color(1.0f, 0.0f, 0.0f, 1.0f) };

    Color c0 = cmap.sample(0.0f);
    Color c1 = cmap.sample(1.0f);
    Color cm = cmap.sample(0.5f);

    REQUIRE(c0.r == Approx(1.0f));
    REQUIRE(c1.r == Approx(1.0f));
    REQUIRE(cm.r == Approx(1.0f));
}

TEST_CASE("ColorMap::sample with empty map returns transparent black", "[colormap]") {
    ColorMap cmap;
    Color c = cmap.sample(0.5f);
    REQUIRE(c.r == Approx(0.0f));
    REQUIRE(c.g == Approx(0.0f));
    REQUIRE(c.b == Approx(0.0f));
    REQUIRE(c.a == Approx(0.0f));
}

TEST_CASE("ColorMap::sample clamps out-of-range t", "[colormap]") {
    ColorMap cmap;
    cmap.colors = {
        Color(0.0f, 0.0f, 1.0f, 1.0f),  // blue
        Color(1.0f, 0.0f, 0.0f, 1.0f)   // red
    };

    Color c_neg = cmap.sample(-0.5f);
    Color c_over = cmap.sample(2.0f);

    // Should clamp to first / last
    REQUIRE(c_neg.b == Approx(1.0f));
    REQUIRE(c_neg.r == Approx(0.0f));
    REQUIRE(c_over.r == Approx(1.0f));
    REQUIRE(c_over.b == Approx(0.0f));
}

TEST_CASE("ColorMap::sample linear interpolation", "[colormap]") {
    ColorMap cmap;
    // 3 entries: blue, green, red
    cmap.colors = {
        Color(0.0f, 0.0f, 1.0f, 1.0f),
        Color(0.0f, 1.0f, 0.0f, 1.0f),
        Color(1.0f, 0.0f, 0.0f, 1.0f)
    };

    // Exact hit at entry 0
    Color c0 = cmap.sample(0.0f);
    REQUIRE(c0.r == Approx(0.0f));
    REQUIRE(c0.g == Approx(0.0f));
    REQUIRE(c0.b == Approx(1.0f));

    // Exact hit at entry 1 (t=0.5 for 3 entries)
    Color c1 = cmap.sample(0.5f);
    REQUIRE(c1.r == Approx(0.0f));
    REQUIRE(c1.g == Approx(1.0f));
    REQUIRE(c1.b == Approx(0.0f));

    // Midpoint between entry 0 and 1 (t=0.25)
    Color cm = cmap.sample(0.25f);
    REQUIRE(cm.r == Approx(0.0f));
    REQUIRE(cm.g == Approx(0.5f));
    REQUIRE(cm.b == Approx(0.5f));
}

TEST_CASE("ColorMap::from_uint8_colors duck-typed", "[colormap]") {
    // Simulate scibar::Color with uint8_t fields
    struct FakeScibarColor {
        uint8_t r, g, b, a;
    };
    std::vector<FakeScibarColor> scibar_cmap = {
        {0, 0, 255, 255},    // blue
        {255, 0, 0, 255},    // red
        {0, 255, 0, 128}     // half-transparent green
    };

    ColorMap cmap = ColorMap::from_uint8_colors(scibar_cmap);

    REQUIRE(cmap.size() == 3);
    REQUIRE(cmap.colors[0].r == Approx(0.0f));
    REQUIRE(cmap.colors[0].b == Approx(1.0f));
    REQUIRE(cmap.colors[1].r == Approx(1.0f));
    REQUIRE(cmap.colors[2].g == Approx(1.0f));
    REQUIRE(cmap.colors[2].a == Approx(128.0f / 255.0f));
}

// =========================================================================
// apply_colormap — single dataset
// =========================================================================

static ColorMap make_test_cmap() {
    // Simple blue→red 4-entry gradient
    ColorMap cmap;
    cmap.colors = {
        Color(0.0f, 0.0f, 1.0f, 1.0f),
        Color(0.0f, 1.0f, 1.0f, 1.0f),
        Color(1.0f, 1.0f, 0.0f, 1.0f),
        Color(1.0f, 0.0f, 0.0f, 1.0f)
    };
    return cmap;
}

TEST_CASE("apply_colormap basic mapping", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {0.0f, 5.0f, 10.0f};

    auto result = apply_colormap(data, cmap);

    REQUIRE(result.colors.size() == 3);
    REQUIRE(result.data_min == Approx(0.0f));
    REQUIRE(result.data_max == Approx(10.0f));
    REQUIRE(result.nan_count == 0);

    // Min → first colour (blue)
    REQUIRE(result.colors[0].b == Approx(1.0f));
    REQUIRE(result.colors[0].r == Approx(0.0f));

    // Max → last colour (red)
    REQUIRE(result.colors[2].r == Approx(1.0f));
    REQUIRE(result.colors[2].b == Approx(0.0f));
}

TEST_CASE("apply_colormap NaN handling", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {1.0f, NAN, 3.0f, NAN};

    Color nan_col(1.0f, 0.0f, 0.0f, 0.5f);  // semi-transparent red for NaN
    auto result = apply_colormap(data, cmap, NAN, NAN, nan_col);

    REQUIRE(result.nan_count == 2);

    // NaN positions
    REQUIRE(result.colors[1].r == Approx(1.0f));
    REQUIRE(result.colors[1].a == Approx(0.5f));
    REQUIRE(result.colors[3].r == Approx(1.0f));
    REQUIRE(result.colors[3].a == Approx(0.5f));

    // Non-NaN positions should not be NaN colour
    bool is_nan_color_0 = (result.colors[0].r == Approx(1.0f) && result.colors[0].a == Approx(0.5f));
    REQUIRE_FALSE(is_nan_color_0);
}

TEST_CASE("apply_colormap explicit vmin/vmax", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {0.0f, 5.0f, 10.0f};

    auto result = apply_colormap(data, cmap, -5.0f, 15.0f);

    // Middle of explicit range → should be mid-colormap
    // (5 - (-5)) / (15 - (-5)) = 10/20 = 0.5
    Color mid = result.colors[1];
    // At t=0.5 with 4 entries: between entries 1 and 2 (cyan and yellow)
    REQUIRE(mid.g == Approx(1.0f));  // both cyan and yellow have full green
}

TEST_CASE("apply_colormap winsorizing", "[colormap]") {
    auto cmap = make_test_cmap();
    // 20 values: 1..10 (normal), 50, 100 (outliers)
    std::vector<float> data = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f,
        1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f,
        50.0f, 100.0f
    };

    auto result = apply_colormap(data, cmap, NAN, NAN,
                                  Color(0.5f, 0.5f, 0.5f, 1.0f),
                                  10.0f, 90.0f);  // clip 10th/90th percentiles

    // Effective range should be winsorized, not [1, 100]
    REQUIRE(result.data_min < 10.0f);
    REQUIRE(result.data_max > 5.0f);
    REQUIRE(result.data_max < 15.0f);

    // winsor_lo and winsor_hi should be set
    REQUIRE_FALSE(std::isnan(result.winsor_lo));
    REQUIRE_FALSE(std::isnan(result.winsor_hi));

    // The outliers should be clamped: 50 and 100 → data_max colour (red)
    REQUIRE(result.colors[18].r == Approx(1.0f));
    REQUIRE(result.colors[19].r == Approx(1.0f));
}

TEST_CASE("apply_colormap constant data", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {5.0f, 5.0f, 5.0f};

    auto result = apply_colormap(data, cmap);

    // All colours should be identical (mid-colormap)
    for (size_t i = 1; i < result.colors.size(); i++) {
        REQUIRE(result.colors[i].r == Approx(result.colors[0].r));
        REQUIRE(result.colors[i].g == Approx(result.colors[0].g));
        REQUIRE(result.colors[i].b == Approx(result.colors[0].b));
    }
}

TEST_CASE("apply_colormap all NaN data", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {NAN, NAN, NAN};

    Color nan_col(1.0f, 1.0f, 1.0f, 1.0f);
    auto result = apply_colormap(data, cmap, NAN, NAN, nan_col);

    REQUIRE(result.nan_count == 3);
    for (const auto& c : result.colors) {
        REQUIRE(c.r == Approx(1.0f));
        REQUIRE(c.g == Approx(1.0f));
        REQUIRE(c.b == Approx(1.0f));
    }
}

TEST_CASE("apply_colormap raw_min/raw_max correct", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {2.0f, 8.0f};

    auto result = apply_colormap(data, cmap);

    REQUIRE(result.raw_min == Approx(2.0f));
    REQUIRE(result.raw_max == Approx(8.0f));
    REQUIRE(result.data_min == Approx(2.0f));
    REQUIRE(result.data_max == Approx(8.0f));
}

TEST_CASE("apply_colormap empty colormap throws", "[colormap]") {
    ColorMap cmap;
    std::vector<float> data = {1.0f, 2.0f, 3.0f};

    REQUIRE_THROWS_AS(apply_colormap(data, cmap), std::invalid_argument);
}

TEST_CASE("apply_colormap invalid percentiles throws", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> data = {1.0f, 2.0f};

    REQUIRE_THROWS_AS(apply_colormap(data, cmap, NAN, NAN,
                                      Color(1,1,1,1), 50.0f, 30.0f),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(apply_colormap(data, cmap, NAN, NAN,
                                      Color(1,1,1,1), -1.0f, 90.0f),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(apply_colormap(data, cmap, NAN, NAN,
                                      Color(1,1,1,1), 0.0f, 110.0f),
                      std::invalid_argument);
}

// =========================================================================
// apply_colormap — multi-dataset
// =========================================================================

TEST_CASE("apply_colormap multi basic", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> ds1 = {0.0f, 5.0f, 10.0f};
    std::vector<float> ds2 = {2.0f, 4.0f, 6.0f};

    auto result = apply_colormap({ds1, ds2}, cmap, NAN, NAN,
                                  Color(0.5f, 0.5f, 0.5f, 1.0f),
                                  0.0f, 100.0f, false);

    REQUIRE(result.per_dataset.size() == 2);
    REQUIRE(result.per_dataset[0].colors.size() == 3);
    REQUIRE(result.per_dataset[1].colors.size() == 3);

    // With global_range=false, each dataset has its own range
    REQUIRE(result.per_dataset[0].data_min == Approx(0.0f));
    REQUIRE(result.per_dataset[0].data_max == Approx(10.0f));
    REQUIRE(result.per_dataset[1].data_min == Approx(2.0f));
    REQUIRE(result.per_dataset[1].data_max == Approx(6.0f));

    // Pooled stats should always be present
    REQUIRE(result.pooled_data_min == Approx(0.0f));
    REQUIRE(result.pooled_data_max == Approx(10.0f));
}

TEST_CASE("apply_colormap multi global_range", "[colormap]") {
    auto cmap = make_test_cmap();
    // lh hemisphere data: 1–3, rh hemisphere data: 7–9
    std::vector<float> lh = {1.0f, 2.0f, 3.0f};
    std::vector<float> rh = {7.0f, 8.0f, 9.0f};

    auto result = apply_colormap({lh, rh}, cmap, NAN, NAN,
                                  Color(0.5f, 0.5f, 0.5f, 1.0f),
                                  0.0f, 100.0f, true);  // global_range

    // Both datasets should share the pooled range [1, 9]
    REQUIRE(result.per_dataset[0].data_min == Approx(1.0f));
    REQUIRE(result.per_dataset[0].data_max == Approx(9.0f));
    REQUIRE(result.per_dataset[1].data_min == Approx(1.0f));
    REQUIRE(result.per_dataset[1].data_max == Approx(9.0f));

    // lh min (1) should map to first colour (blue)
    REQUIRE(result.per_dataset[0].colors[0].b > 0.5f);
    REQUIRE(result.per_dataset[0].colors[0].r < 0.3f);

    // rh max (9) should map to last colour (red)
    REQUIRE(result.per_dataset[1].colors[2].r > 0.5f);
    REQUIRE(result.per_dataset[1].colors[2].b < 0.3f);

    // Pooled range
    REQUIRE(result.pooled_data_min == Approx(1.0f));
    REQUIRE(result.pooled_data_max == Approx(9.0f));
}

TEST_CASE("apply_colormap multi with winsorizing and global_range", "[colormap]") {
    auto cmap = make_test_cmap();
    // lh: mostly 1-5, one outlier at -100
    std::vector<float> lh = {-100.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
                              1.5f, 2.5f, 3.5f, 4.5f};
    // rh: mostly 6-10, one outlier at 100
    std::vector<float> rh = {6.0f, 7.0f, 8.0f, 9.0f, 10.0f,
                              6.5f, 7.5f, 8.5f, 9.5f, 100.0f};

    auto result = apply_colormap({lh, rh}, cmap, NAN, NAN,
                                  Color(0.5f, 0.5f, 0.5f, 1.0f),
                                  10.0f, 90.0f,   // clip 10th/90th percentiles
                                  true);           // global_range

    // Pooled range should NOT be [-100, 100] — it should be winsorized
    REQUIRE(result.pooled_data_min > -10.0f);
    REQUIRE(result.pooled_data_max < 20.0f);

    // pooled_winsor cutoffs should be set
    REQUIRE_FALSE(std::isnan(result.pooled_winsor_lo));
    REQUIRE_FALSE(std::isnan(result.pooled_winsor_hi));

    // Both datasets share the pooled range
    REQUIRE(result.per_dataset[0].data_min == Approx(result.pooled_data_min));
    REQUIRE(result.per_dataset[1].data_min == Approx(result.pooled_data_min));
}

TEST_CASE("apply_colormap multi NaN across datasets", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> lh = {1.0f, NAN, 3.0f};
    std::vector<float> rh = {NAN, 5.0f, NAN};

    Color nan_col(1.0f, 1.0f, 1.0f, 1.0f);
    auto result = apply_colormap({lh, rh}, cmap, NAN, NAN, nan_col);

    REQUIRE(result.total_nan_count == 3);
    REQUIRE(result.per_dataset[0].nan_count == 1);
    REQUIRE(result.per_dataset[1].nan_count == 2);

    // NaN positions get nan_color
    REQUIRE(result.per_dataset[0].colors[1].r == Approx(1.0f));
    REQUIRE(result.per_dataset[0].colors[1].g == Approx(1.0f));
    REQUIRE(result.per_dataset[1].colors[0].r == Approx(1.0f));
    REQUIRE(result.per_dataset[1].colors[2].r == Approx(1.0f));
}

TEST_CASE("apply_colormap multi empty datasets", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<std::vector<float>> datasets;

    auto result = apply_colormap(datasets, cmap,
                                  NAN, NAN,
                                  Color(0.5f, 0.5f, 0.5f, 1.0f),
                                  0.0f, 100.0f, false);

    REQUIRE(result.per_dataset.empty());
}

TEST_CASE("apply_colormap multi explicit vmin/vmax overrides global_range", "[colormap]") {
    auto cmap = make_test_cmap();
    std::vector<float> ds1 = {0.0f, 5.0f, 10.0f};
    std::vector<float> ds2 = {2.0f, 4.0f, 6.0f};

    // Explicit range [0, 20] — should be used regardless of global_range or data
    auto result = apply_colormap({ds1, ds2}, cmap, 0.0f, 20.0f,
                                  Color(0.5f, 0.5f, 0.5f, 1.0f),
                                  0.0f, 100.0f, true);

    REQUIRE(result.per_dataset[0].data_min == Approx(0.0f));
    REQUIRE(result.per_dataset[0].data_max == Approx(20.0f));
    REQUIRE(result.per_dataset[1].data_min == Approx(0.0f));
    REQUIRE(result.per_dataset[1].data_max == Approx(20.0f));
    REQUIRE(result.pooled_data_min == Approx(0.0f));
    REQUIRE(result.pooled_data_max == Approx(20.0f));
}
