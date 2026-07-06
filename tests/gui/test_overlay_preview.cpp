// HUD / LAY in-game-style preview draw core (#283) — display-free pixel
// assertions against fx_render::fa surfaces.
#include <catch2/catch_test_macros.hpp>
#include "editors/overlay_preview.h"
#include <cstdint>

using namespace fxg;
using fx_render::fa::Palette;
using fx_render::fa::Raster;
using fx_render::fa::Surface;

namespace {

int count_col(const Surface& s, int x, std::uint8_t idx) {
    int n = 0;
    for (int y = 0; y < s.height(); ++y)
        if (s.row(y)[x] == idx) ++n;
    return n;
}

int count_region(const Surface& s, int x0, int y0, int x1, int y1,
                 std::uint8_t idx) {
    int n = 0;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (s.row(y)[x] == idx) ++n;
    return n;
}

void set_param(fx::HudFile& h, const char* g, const char* f, int16_t v) {
    h.params.push_back({g, f, v});
}

fx::HudFile minimal_hud() {
    fx::HudFile h;
    h.valid = true;
    h.icon_a = "GEAR";
    h.icon_b = "FLAP";
    h.icon_c = "BRAKE";
    h.icon_d = "HOOK";
    set_param(h, "flight_path_marker", "dx", 0);
    set_param(h, "flight_path_marker", "dy", 0);
    set_param(h, "flight_path_marker", "box_half_width", 12);
    set_param(h, "flight_path_marker", "box_half_height", 10);
    set_param(h, "speed_tape", "dx", -60);
    set_param(h, "speed_tape", "dy", -40);
    set_param(h, "speed_tape", "height", 80);
    set_param(h, "speed_tape", "tick_increment", 50);
    set_param(h, "altitude_tape", "dx", 60);
    set_param(h, "altitude_tape", "dy", -40);
    set_param(h, "altitude_tape", "height", 80);
    set_param(h, "altitude_tape", "tick_increment", 500);
    set_param(h, "heading_tape", "width", 100);
    set_param(h, "heading_tape", "dy", -70);
    set_param(h, "heading_tape", "tick_spacing", 10);
    set_param(h, "warning_lights", "dx", -80);
    set_param(h, "warning_lights", "dy", 60);  // clear of the tapes
    set_param(h, "hud", "center_dot_enable", 1);
    return h;
}

}  // namespace

TEST_CASE("overlay text draws with the built-in font") {
    OverlayText text;
    CHECK(text.height() == 6);
    CHECK(text.width("420") == 14);  // 3 * 5 - 1

    Surface s(64, 16);
    s.Clear(0);
    Raster r(s);
    r.SetColor(1);
    text.Draw(r, 2, 2, "420");
    CHECK(count_region(s, 0, 0, 63, 15, 1) > 10);
}

TEST_CASE("hud preview places the tapes at the file's offsets") {
    Surface s(240, 180);
    s.Clear(0);
    Raster r(s);
    fx::HudFile hud = minimal_hud();
    HudPreviewState st;

    OverlayText text;
    DrawHudPreview(r, hud, st, text, 1);

    const int cx = 120, cy = 90;
    // Speed tape: a vertical line the tape's height at cx + dx.
    CHECK(count_col(s, cx - 60, 1) >= 60);
    // Altitude tape mirrored at cx + dx.
    CHECK(count_col(s, cx + 60, 1) >= 60);
    // FPM marks near the anchor.
    CHECK(count_region(s, cx - 14, cy - 12, cx + 14, cy + 6, 1) > 10);
    // Heading tape baseline row at cy + dy.
    CHECK(count_region(s, cx - 50, cy - 70, cx + 50, cy - 70, 1) >= 90);

    // Moving a tape moves its pixels.
    Surface s2(240, 180);
    s2.Clear(0);
    Raster r2(s2);
    for (auto& p : hud.params)
        if (p.gauge == "speed_tape" && p.field == "dx") p.value = -90;
    DrawHudPreview(r2, hud, st, text, 1);
    CHECK(count_col(s2, cx - 90, 1) >= 60);
    CHECK(count_col(s2, cx - 60, 1) < 10);
}

TEST_CASE("hud preview annunciators follow the config state") {
    Surface off(240, 180), on(240, 180);
    off.Clear(0);
    on.Clear(0);
    fx::HudFile hud = minimal_hud();
    OverlayText text;

    HudPreviewState st;   // all config flags false
    Raster r_off(off);
    DrawHudPreview(r_off, hud, st, text, 1);
    st.gear = st.flap = true;
    Raster r_on(on);
    DrawHudPreview(r_on, hud, st, text, 1);

    // GEAR/FLAP labels appear at the warning_lights offset only when on.
    const int x0 = 120 - 80, y0 = 90 + 60;
    int before = count_region(off, x0, y0, x0 + 40, y0 + 20, 1);
    int after  = count_region(on, x0, y0, x0 + 40, y0 + 20, 1);
    CHECK(before == 0);
    CHECK(after > 10);
}

TEST_CASE("lay sky preview fills the ramps around the horizon line") {
    fx::LayLayer layer{};
    for (int i = 0; i < 31; ++i)
        layer.zenith_grad[i] = {(std::uint8_t)(i + 1), 0, 0};       // reds
    for (int i = 0; i < 32; ++i)
        layer.horizon_grad[i] = {0, 0, (std::uint8_t)(i + 1)};      // blues
    layer.base_rgb[0] = 63; layer.base_rgb[1] = 63; layer.base_rgb[2] = 0;

    Surface s(64, 100);
    s.Clear(0);
    Palette pal;
    const int horizon = 50;
    DrawLaySky(s, pal, layer, horizon);

    // Palette slots carry the layer's 6-bit colours.
    CHECK(pal.entries[kLayZenithBase].r == 1);
    CHECK(pal.entries[kLayZenithBase + 30].r == 31);
    CHECK(pal.entries[kLayHorizonBase + 31].b == 32);
    CHECK(pal.entries[kLaySkyBase].r == 63);

    // Top row starts the zenith ramp; the bottom sits at the end of the
    // horizon ramp; the horizon line wears the base colour.
    int top = s.row(0)[32];
    CHECK(top >= kLayZenithBase);
    CHECK(top <= kLayZenithBase + 1);
    int bottom = s.row(99)[32];
    CHECK(bottom >= kLayHorizonBase + 29);
    CHECK(bottom <= kLayHorizonBase + 31);
    CHECK(s.row(horizon)[32] == kLaySkyBase);

    // Sky indices step monotonically down the frame above the horizon.
    int prev = -1;
    for (int y = 0; y < horizon; ++y) {
        int v = s.row(y)[32];
        CHECK(v >= prev);
        prev = v;
    }
}

TEST_CASE("lay per-angle band selection follows the documented formula") {
    fx::LayFile lay;
    lay.valid = true;
    lay.layer_array_va = 0x1078;
    lay.layers.resize(3);
    lay.sky_angle_scale = 7;
    lay.below_angle_scale = 6;
    for (int i = 0; i < 10; ++i) {
        lay.sky_layer_va[i] = 0x1078;                    // layer 0
        lay.below_layer_va[i] = 0x1078 + 0x160;          // layer 1
    }
    lay.sky_layer_va[9] = 0x1078 + 2 * 0x160;            // layer 2

    CHECK(LaySelectLayer(lay, 0) == 0);          // at the horizon: sky band 0
    CHECK(LaySelectLayer(lay, -0x40) == 0);      // near-horizon band
    CHECK(LaySelectLayer(lay, 100) == 0);        // 100*7>>8 = 2 -> layer 0
    CHECK(LaySelectLayer(lay, 336) == 2);        // 336*7>>8 = 9 -> layer 2
    CHECK(LaySelectLayer(lay, 4000) == 2);       // clamped to band 9
    CHECK(LaySelectLayer(lay, -0xC1) == 1);      // below-horizon band 0
    CHECK(LaySelectLayer(lay, -2000) == 1);      // clamped below band

    lay.sky_layer_va[0] = 0x1079;                // misaligned VA
    CHECK(LaySelectLayer(lay, 0) == -1);
}
