#include "pal_editor.h"
#include "../app.h"
#include "../palettes.h"
#include "imgui.h"
#include "fx/pal.h"

#include <algorithm>

void DrawPalEditor(App& app) {
    auto& ed = app.editor;

    fx::Palette pal   = fx::pal_load(ed.data.data(), ed.data.size());
    int         count = ed.data.size() < 3
        ? 256 // greyscale fallback fills all 256 slots
        : (int)std::min(ed.data.size() / 3, (size_t)256);

    ImGui::Text("%d colors  |  %zu bytes (VGA 6-bit, 768 expected)",
                count, ed.data.size());
    bool active = (ed.libIdx == app.palLib && ed.entryIdx == app.palEntry);
    if (active) {
        ImGui::TextDisabled("Current preview palette.");
    } else if (ImGui::Button("Use as preview palette")) {
        app.palLib   = ed.libIdx;
        app.palEntry = ed.entryIdx;
        app.palGen++;
        app.statusMsg  = "Preview palette set.";
        app.statusKind = App::StatusKind::Info;
    }
    ImGui::Separator();

    fxg::DrawPaletteSwatches("pal-editor", pal, count);
}
