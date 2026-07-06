#pragma once
// In-game-style HUD / LAY preview draw core (#283). Display-free — no
// ImGui, no SDL — so the gui_tests pixel-assert it directly. Everything
// draws through fx_render::fa, the documented stand-in for the G_* raster
// layer the engine's own HUD and horizon code draw through (docs/fa/hud.md,
// docs/fa/renderer.md).
#include "fx/fnt.h"
#include "fx/hud.h"
#include "fx/lay.h"
#include "fx_render/fa.h"
#include <string>

namespace fxg {

// Text for the overlay: a rendered install FNT when one is available
// (fnt_render_glyphs already run), else a built-in 4x6 block font — a
// legibility stand-in, not a claim about the engine's font metrics.
class OverlayText {
public:
    OverlayText() = default;
    explicit OverlayText(const fx::FntFile* fnt) : fnt_(fnt) {}

    int height() const;
    int width(const std::string& s) const;
    void Draw(fx_render::fa::Raster& r, int x, int y, const std::string& s) const;

    bool using_fnt() const { return fnt_ != nullptr; }

private:
    const fx::FntFile* fnt_ = nullptr;
};

// Simulated flight state the drawers read — the preview's stand-in for the
// entity mirror the in-game drawers sample (docs/fa/hud.md).
struct HudPreviewState {
    int  heading_deg = 270;  // 0..359
    int  speed_kt    = 425;
    int  altitude_ft = 8200;
    bool gear = false, flap = false, brake = false, hook = false;
    bool warning = false;    // blinking warning line ("STALL")
};

// Draw the HUD symbology into the raster from the .HUD gauge parameters:
// flight-path marker (the layout anchor everything else offsets from),
// heading/speed/altitude tapes, config annunciators (the file's advisory
// icon labels), and the positioned readouts. `color` is the palette index
// used for all marks — the engine takes it from HUDInit's layout block,
// not the .HUD file, so the preview's green is a stand-in.
void DrawHudPreview(fx_render::fa::Raster& r, const fx::HudFile& hud,
                    const HudPreviewState& st, const OverlayText& text,
                    std::uint8_t color);

// Palette layout DrawLaySky writes the selected layer's colours into:
// the zenith ramp, then the horizon ramp, then the two base colours.
// (A preview arrangement — the engine loads LAY colours into its own sky
// palette range.)
inline constexpr std::uint8_t kLayZenithBase  = 96;   // 31 entries
inline constexpr std::uint8_t kLayHorizonBase = 128;  // 32 entries
inline constexpr std::uint8_t kLaySkyBase     = 160;  // base_rgb
inline constexpr std::uint8_t kLayGroundBase  = 161;  // horizon_base_rgb

// Render one layer's sky the way T_DefaultHorizon does (docs/fa/formats/
// LAY.md § Engine Notes): the zenith→horizon ramp above the horizon line
// and the horizon-downward ramp below, each band Gouraud-interpolated
// between adjacent ramp entries (GouraudHorizon) so the palette-index
// banding matches the engine's. Writes the layer's 6-bit colours into
// `pal` at the kLay* slots and fills `surface` through a Raster.
// `horizon_y` places the horizon line (clamped to the surface).
void DrawLaySky(fx_render::fa::Surface& surface, fx_render::fa::Palette& pal,
                const fx::LayLayer& layer, int horizon_y);

// The documented per-angle layer selection (SetActiveLayerByAngle,
// docs/fa/formats/LAY.md § Engine Notes): above the horizon
// `sky_layer_va[angle * sky_angle_scale >> 8]`, below −0xC0
// `below_layer_va[(−0xC0 − angle) * below_angle_scale >> 6]`, horizon band
// otherwise → sky band 0. `angle_units` is the signed elevation angle in
// the engine's small angle units (the exact unit scale is inferred —
// ~256 units per quadrant fits every install file's scales and 10-entry
// band tables). Returns the index into `layers`, or -1 when the band VA
// does not land on a layer slot.
int LaySelectLayer(const fx::LayFile& lay, int angle_units);

}  // namespace fxg
