#pragma once
#include "fx/pal.h"
#include <string>
#include <vector>

class App;

// Preview-palette selection shared by the Preview panel (PIC) and the CB8
// editor. App::palLib holds a session index or one of the sentinels below;
// App::palGen bumps on every change so cached preview textures re-decode.
namespace fxg {

// Auto preserves the pre-switcher behavior: PIC previews hunt PALETTE.PAL
// across open sessions; CB8 renders greyscale (the engine palette CB8 uses
// is not stored in any LIB, and PALETTE.PAL garbles it — see cb8_editor).
constexpr int kPalAuto      = -1;
constexpr int kPalGreyscale = -2;

// One .PAL record selectable as the preview palette.
struct PalChoice {
    int         lib;    // index into App::sessions
    int         entry;  // index into LibSession::entries
    std::string label;  // "FILE.LIB: ICON.PAL"
};

// Every .PAL entry across open sessions, in session order.
std::vector<PalChoice> EnumeratePalettes(const App& app);

// The palette the current selection resolves to for PIC previews:
// a chosen .PAL entry, greyscale, or the Auto PALETTE.PAL hunt.
fx::Palette ResolvePreviewPalette(const App& app);

// True when the selection names a real .PAL entry; *out receives it.
// Auto and Greyscale return false — CB8 renders greyscale for both.
bool SelectedPalette(const App& app, fx::Palette* out);

// Combo bound to App::palLib/palEntry; bumps App::palGen on change.
// autoLabel names what Auto means where the combo is drawn.
void DrawPaletteCombo(App& app, const char* autoLabel);

// 16-per-row swatch grid with an index/RGB tooltip per color.
void DrawPaletteSwatches(const char* id, const fx::Palette& pal, int count);

} // namespace fxg
