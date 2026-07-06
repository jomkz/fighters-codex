#include "overlay_preview.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

using fx_render::fa::PolyVertex;
using fx_render::fa::Raster;
using fx_render::fa::Surface;
using fx_render::fa::ToFx;

namespace fxg {

// ---------------------------------------------------------------------------
// OverlayText
// ---------------------------------------------------------------------------

// Built-in 4x6 block font (digits, upper-case letters, and the few symbols
// the readouts use). Each row is the high 4 bits' worth of pixels, MSB left.
namespace {

struct MiniGlyph {
    char ch;
    std::uint8_t rows[6];
};

const MiniGlyph kMiniFont[] = {
    {'0', {0x6, 0x9, 0x9, 0x9, 0x9, 0x6}}, {'1', {0x2, 0x6, 0x2, 0x2, 0x2, 0x7}},
    {'2', {0x6, 0x9, 0x1, 0x6, 0x8, 0xF}}, {'3', {0xE, 0x1, 0x6, 0x1, 0x1, 0xE}},
    {'4', {0x9, 0x9, 0xF, 0x1, 0x1, 0x1}}, {'5', {0xF, 0x8, 0xE, 0x1, 0x1, 0xE}},
    {'6', {0x6, 0x8, 0xE, 0x9, 0x9, 0x6}}, {'7', {0xF, 0x1, 0x2, 0x2, 0x4, 0x4}},
    {'8', {0x6, 0x9, 0x6, 0x9, 0x9, 0x6}}, {'9', {0x6, 0x9, 0x9, 0x7, 0x1, 0x6}},
    {'A', {0x6, 0x9, 0x9, 0xF, 0x9, 0x9}}, {'B', {0xE, 0x9, 0xE, 0x9, 0x9, 0xE}},
    {'C', {0x6, 0x9, 0x8, 0x8, 0x9, 0x6}}, {'D', {0xE, 0x9, 0x9, 0x9, 0x9, 0xE}},
    {'E', {0xF, 0x8, 0xE, 0x8, 0x8, 0xF}}, {'F', {0xF, 0x8, 0xE, 0x8, 0x8, 0x8}},
    {'G', {0x6, 0x8, 0xB, 0x9, 0x9, 0x6}}, {'H', {0x9, 0x9, 0xF, 0x9, 0x9, 0x9}},
    {'I', {0x7, 0x2, 0x2, 0x2, 0x2, 0x7}}, {'J', {0x3, 0x1, 0x1, 0x1, 0x9, 0x6}},
    {'K', {0x9, 0xA, 0xC, 0xC, 0xA, 0x9}}, {'L', {0x8, 0x8, 0x8, 0x8, 0x8, 0xF}},
    {'M', {0x9, 0xF, 0xF, 0x9, 0x9, 0x9}}, {'N', {0x9, 0xD, 0xD, 0xB, 0xB, 0x9}},
    {'O', {0x6, 0x9, 0x9, 0x9, 0x9, 0x6}}, {'P', {0xE, 0x9, 0x9, 0xE, 0x8, 0x8}},
    {'Q', {0x6, 0x9, 0x9, 0x9, 0xA, 0x5}}, {'R', {0xE, 0x9, 0x9, 0xE, 0xA, 0x9}},
    {'S', {0x7, 0x8, 0x6, 0x1, 0x1, 0xE}}, {'T', {0x7, 0x2, 0x2, 0x2, 0x2, 0x2}},
    {'U', {0x9, 0x9, 0x9, 0x9, 0x9, 0x6}}, {'V', {0x9, 0x9, 0x9, 0x9, 0x6, 0x6}},
    {'W', {0x9, 0x9, 0x9, 0xF, 0xF, 0x9}}, {'X', {0x9, 0x9, 0x6, 0x6, 0x9, 0x9}},
    {'Y', {0x9, 0x9, 0x6, 0x2, 0x2, 0x2}}, {'Z', {0xF, 0x1, 0x2, 0x4, 0x8, 0xF}},
    {'-', {0x0, 0x0, 0x0, 0xE, 0x0, 0x0}}, {'.', {0x0, 0x0, 0x0, 0x0, 0x0, 0x4}},
    {'/', {0x1, 0x1, 0x2, 0x4, 0x8, 0x8}}, {':', {0x0, 0x4, 0x0, 0x0, 0x4, 0x0}},
};

const MiniGlyph* mini_glyph(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (const auto& g : kMiniFont)
        if (g.ch == c) return &g;
    return nullptr;
}

constexpr int kMiniW = 4, kMiniH = 6, kMiniAdvance = 5;

}  // namespace

int OverlayText::height() const {
    return fnt_ ? (int)fnt_->font_height : kMiniH;
}

int OverlayText::width(const std::string& s) const {
    int w = 0;
    if (fnt_) {
        for (char c : s) w += (int)fnt_->glyph_width[(std::uint8_t)c] + 1;
    } else {
        w = (int)s.size() * kMiniAdvance;
    }
    return w > 0 ? w - 1 : 0;
}

void OverlayText::Draw(Raster& r, int x, int y, const std::string& s) const {
    if (fnt_) {
        for (char c : s) {
            const fx::FntGlyph& g = fnt_->glyphs[(std::uint8_t)c];
            for (std::uint32_t py = 0; py < g.height; ++py)
                for (std::uint32_t px = 0; px < g.width; ++px)
                    if (!g.pixels.empty() && g.pixels[py * g.width + px])
                        r.Point(x + (int)px, y + (int)py);
            x += (int)g.width + 1;
        }
        return;
    }
    for (char c : s) {
        if (const MiniGlyph* g = mini_glyph(c)) {
            for (int py = 0; py < kMiniH; ++py)
                for (int px = 0; px < kMiniW; ++px)
                    if (g->rows[py] & (0x8 >> px)) r.Point(x + px, y + py);
        }
        x += kMiniAdvance;
    }
}

// ---------------------------------------------------------------------------
// HUD preview
// ---------------------------------------------------------------------------

namespace {

int param(const fx::HudFile& hud, const char* gauge, const char* field) {
    for (const auto& p : hud.params)
        if (p.gauge == gauge && p.field == field) return p.value;
    return 0;
}

void draw_circle(Raster& r, int cx, int cy, int rad) {
    // Eight-segment octagon — the marker ring at symbology scale.
    const int k = (rad * 29) / 41;  // rad / sqrt(2)
    const int px[8] = {rad, k, 0, -k, -rad, -k, 0, k};
    const int py[8] = {0, k, rad, k, 0, -k, -rad, -k};
    for (int i = 0; i < 8; ++i) {
        int j = (i + 1) & 7;
        r.Line(cx + px[i], cy + py[i], cx + px[j], cy + py[j]);
    }
}

char* fmt(char* buf, size_t n, const char* f, int v) {
    snprintf(buf, n, f, v);
    return buf;
}

}  // namespace

void DrawHudPreview(Raster& r, const fx::HudFile& hud,
                    const HudPreviewState& st, const OverlayText& text,
                    std::uint8_t color) {
    const Surface& s = r.target();
    const int cx = s.width() / 2, cy = s.height() / 2;
    const int lh = text.height() + 2;
    char buf[32];
    r.SetColor(color);
    r.SetFillType(fx_render::fa::FillType::Flat);

    // Flight-path marker — the layout anchor (docs/fa/hud.md: every element
    // positions relative to _hudFpmX/Y plus its layout offset).
    const int fx0 = cx + param(hud, "flight_path_marker", "dx");
    const int fy0 = cy + param(hud, "flight_path_marker", "dy");
    const int bhw = std::max(3, param(hud, "flight_path_marker", "box_half_width"));
    const int bhh = std::max(3, param(hud, "flight_path_marker", "box_half_height"));
    draw_circle(r, fx0, fy0, 3);
    r.Line(fx0 - bhw, fy0, fx0 - 3, fy0);  // wings
    r.Line(fx0 + 3, fy0, fx0 + bhw, fy0);
    r.Line(fx0, fy0 - bhh, fx0, fy0 - 3);  // fin

    if (param(hud, "hud", "center_dot_enable")) r.Rect(cx, cy, cx + 1, cy + 1);

    // Heading tape — top strip: ticks every 5 deg at tick_spacing px, labels
    // every 10 deg, the current heading under a centre caret.
    {
        const int w = param(hud, "heading_tape", "width");
        const int y = fy0 + param(hud, "heading_tape", "dy");
        const int spacing = std::max(4, param(hud, "heading_tape", "tick_spacing"));
        if (w > 0) {
            r.Line(fx0 - w / 2, y, fx0 + w / 2, y);
            r.Line(fx0, y + 2, fx0, y + 6);  // caret
            // Ticks: 5-deg steps; px per degree = spacing / 5.
            const int first = (st.heading_deg / 5) * 5 - (w / 2) * 5 / spacing;
            for (int deg = first; deg <= first + w * 5 / spacing + 5; deg += 5) {
                int x = fx0 + (deg - st.heading_deg) * spacing / 5;
                if (x < fx0 - w / 2 || x > fx0 + w / 2) continue;
                bool major = (deg % 10) == 0;
                r.Line(x, y - (major ? 5 : 3), x, y - 1);
                if (major) {
                    fmt(buf, sizeof buf, "%02d", ((deg % 360) + 360) % 360 / 10);
                    text.Draw(r, x - text.width(buf) / 2, y - 6 - text.height(), buf);
                }
            }
        }
    }

    // Speed / altitude tapes — vertical scales with the current value boxed
    // at the tape midpoint, ticks each tick_increment units.
    auto tape = [&](const char* gauge, int value, const char* value_fmt) {
        const int x = fx0 + param(hud, gauge, "dx");
        const int y0 = fy0 + param(hud, gauge, "dy");
        const int h = param(hud, gauge, "height");
        const int inc = std::max(1, param(hud, gauge, "tick_increment"));
        if (h <= 0) return;
        r.Line(x, y0, x, y0 + h);
        const bool left = x < fx0;  // ticks/labels face away from centre
        const int mid = y0 + h / 2;
        // One tick per increment, 8 increments over the tape height.
        const int px_per_inc = std::max(4, h / 8);
        for (int t = -(h / 2) / px_per_inc; t <= (h / 2) / px_per_inc; ++t) {
            int ty = mid - t * px_per_inc;
            if (ty < y0 || ty > y0 + h) continue;
            r.Line(x, ty, x + (left ? -4 : 4), ty);
            int tv = value + t * inc;
            if (tv < 0) continue;
            fmt(buf, sizeof buf, "%d", tv - tv % inc);
            int tx = left ? x - 6 - text.width(buf) : x + 6;
            if (t != 0) text.Draw(r, tx, ty - text.height() / 2, buf);
        }
        // Current-value box at the midpoint.
        fmt(buf, sizeof buf, value_fmt, value);
        int bw = text.width(buf) + 4;
        int bx = left ? x - 8 - bw : x + 8;
        r.Line(bx, mid - lh / 2, bx + bw, mid - lh / 2);
        r.Line(bx, mid + lh / 2, bx + bw, mid + lh / 2);
        r.Line(bx, mid - lh / 2, bx, mid + lh / 2);
        r.Line(bx + bw, mid - lh / 2, bx + bw, mid + lh / 2);
        text.Draw(r, bx + 2, mid - text.height() / 2, buf);
    };
    tape("speed_tape", st.speed_kt, "%d");
    tape("altitude_tape", st.altitude_ft, "%d");

    // Config annunciators — the file's advisory icon labels, stacked at the
    // warning_lights offset when their state is on.
    {
        int x = fx0 + param(hud, "warning_lights", "dx");
        int y = fy0 + param(hud, "warning_lights", "dy");
        const std::string* labels[4] = {&hud.icon_a, &hud.icon_b, &hud.icon_c,
                                        &hud.icon_d};
        const bool on[4] = {st.gear, st.flap, st.brake, st.hook};
        for (int i = 0; i < 4; ++i) {
            if (!on[i] || labels[i]->empty()) continue;
            text.Draw(r, x, y, *labels[i]);
            y += lh;
        }
    }

    // Positioned readouts — content is simulated, position is the file's.
    text.Draw(r, fx0 + param(hud, "throttle_readout", "dx"),
              fy0 + param(hud, "throttle_readout", "dy"), "87");
    text.Draw(r, fx0 + param(hud, "weapon_info", "dx"),
              fy0 + param(hud, "weapon_info", "dy"), "AIM-9M 2");
    text.Draw(r, fx0 + param(hud, "range_info", "dx"),
              fy0 + param(hud, "range_info", "dy"), "RNG 4.2");
    text.Draw(r, fx0 + param(hud, "score_indicator", "dx"),
              fy0 + param(hud, "score_indicator", "dy"), "0");

    if (param(hud, "ecm_bar", "enable")) {
        int y = s.height() - 12;
        r.Rect(cx - 20, y, cx + 20, y + 3);
    }
    if (param(hud, "lead_indicator", "enable")) {
        r.Line(fx0 - 2, fy0 - 14, fx0 + 2, fy0 - 14);
        r.Line(fx0, fy0 - 16, fx0, fy0 - 12);
    }

    // Blinking warning line (HUDSetWarning/HUDDrawWarning): below the FPM.
    if (st.warning) {
        const char* w = "STALL";
        text.Draw(r, fx0 - text.width(w) / 2, fy0 + bhh + lh, w);
    }
}

// ---------------------------------------------------------------------------
// LAY sky preview
// ---------------------------------------------------------------------------

void DrawLaySky(Surface& surface, fx_render::fa::Palette& pal,
                const fx::LayLayer& layer, int horizon_y) {
    const int W = surface.width(), H = surface.height();
    horizon_y = std::max(1, std::min(H - 2, horizon_y));

    // The layer's 6-bit colours land at the preview slots.
    for (int i = 0; i < 31; ++i)
        pal.entries[kLayZenithBase + i] = {layer.zenith_grad[i].r,
                                           layer.zenith_grad[i].g,
                                           layer.zenith_grad[i].b};
    for (int i = 0; i < 32; ++i)
        pal.entries[kLayHorizonBase + i] = {layer.horizon_grad[i].r,
                                            layer.horizon_grad[i].g,
                                            layer.horizon_grad[i].b};
    pal.entries[kLaySkyBase] = {layer.base_rgb[0], layer.base_rgb[1],
                                layer.base_rgb[2]};
    pal.entries[kLayGroundBase] = {layer.horizon_base_rgb[0],
                                   layer.horizon_base_rgb[1],
                                   layer.horizon_base_rgb[2]};

    Raster r(surface);
    r.SetFillType(fx_render::fa::FillType::Shaded);

    // One Gouraud band per adjacent ramp pair (GouraudHorizon): the packed
    // shade term walks the preview palette slots, so the banding matches the
    // engine's index interpolation.
    auto band = [&](int y0, int y1, int c0, int c1) {
        if (y1 <= y0) return;
        PolyVertex v[4];
        v[0] = {ToFx(0), ToFx(y0), 0, 0, ToFx(c0)};
        v[1] = {ToFx(W), ToFx(y0), 0, 0, ToFx(c0)};
        v[2] = {ToFx(W), ToFx(y1), 0, 0, ToFx(c1)};
        v[3] = {ToFx(0), ToFx(y1), 0, 0, ToFx(c1)};
        r.SUPolygon(v, 4);
    };

    // Zenith ramp: entry 0 at the top of frame, entry 30 at the horizon.
    for (int k = 0; k < 30; ++k)
        band(horizon_y * k / 30, horizon_y * (k + 1) / 30,
             kLayZenithBase + k, kLayZenithBase + k + 1);

    // Horizon-downward ramp: entry 0 just below the line to entry 31 at the
    // bottom of frame.
    const int below = H - 1 - horizon_y;
    for (int k = 0; k < 31; ++k)
        band(horizon_y + 1 + below * k / 31, horizon_y + 1 + below * (k + 1) / 31,
             kLayHorizonBase + k, kLayHorizonBase + k + 1);

    // The horizon line itself wears the layer's base colour.
    r.SetFillType(fx_render::fa::FillType::Flat);
    r.SetColor(kLaySkyBase);
    r.Line(0, horizon_y, W - 1, horizon_y);
}

int LaySelectLayer(const fx::LayFile& lay, int angle_units) {
    auto to_index = [&](std::uint32_t va) -> int {
        if (va < lay.layer_array_va) return -1;
        std::uint32_t off = va - lay.layer_array_va;
        if (off % 0x160) return -1;
        std::uint32_t idx = off / 0x160;
        return idx < lay.layers.size() ? (int)idx : -1;
    };
    auto clamp_band = [](int b) { return std::max(0, std::min(9, b)); };

    if (angle_units > 0)
        return to_index(
            lay.sky_layer_va[clamp_band((angle_units * (int)lay.sky_angle_scale) >> 8)]);
    if (angle_units >= -0xC0)  // the near/at-horizon band
        return to_index(lay.sky_layer_va[0]);
    return to_index(lay.below_layer_va[clamp_band(
        ((-0xC0 - angle_units) * (int)lay.below_angle_scale) >> 6)]);
}

}  // namespace fxg
