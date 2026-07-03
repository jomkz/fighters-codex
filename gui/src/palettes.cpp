#include "palettes.h"
#include "app.h"
#include "util.h"
#include "imgui.h"
#include "fx/ealib.h"

#include <cstdio>
#include <filesystem>
namespace fs = std::filesystem;

namespace fxg {

static std::string EntryExt(const std::string& name) {
    auto dot = name.rfind('.');
    return (dot != std::string::npos) ? name.substr(dot + 1) : "";
}

std::vector<PalChoice> EnumeratePalettes(const App& app) {
    std::vector<PalChoice> out;
    for (int li = 0; li < (int)app.sessions.size(); li++) {
        const auto& sess = app.sessions[li];
        for (int ei = 0; ei < (int)sess.entries.size(); ei++) {
            if (!ci_equal(EntryExt(sess.entries[ei].name).c_str(), "pal"))
                continue;
            std::string label = fs::path(sess.path).filename().string();
            if (!sess.standalone) {
                label += ": ";
                label += sess.entries[ei].name;
            }
            out.push_back({li, ei, std::move(label)});
        }
    }
    return out;
}

static fx::Palette LoadPalEntry(const App& app, int lib, int entry) {
    const auto& sess = app.sessions[lib];
    if (sess.standalone)
        return fx::pal_load(sess.data.data(), sess.data.size());
    auto raw = fx::ealib_extract(sess.data.data(), sess.data.size(),
                                 sess.entries[entry], true);
    return fx::pal_load(raw.data(), raw.size());
}

bool SelectedPalette(const App& app, fx::Palette* out) {
    if (app.palLib < 0 || app.palLib >= (int)app.sessions.size())
        return false;
    if (app.palEntry < 0 ||
        app.palEntry >= (int)app.sessions[app.palLib].entries.size())
        return false;
    *out = LoadPalEntry(app, app.palLib, app.palEntry);
    return true;
}

fx::Palette ResolvePreviewPalette(const App& app) {
    fx::Palette pal;
    if (SelectedPalette(app, &pal))
        return pal;
    if (app.palLib == kPalGreyscale)
        return fx::pal_load(nullptr, 0);
    // Auto: PALETTE.PAL from any open session, greyscale fallback.
    for (const auto& sess : app.sessions) {
        if (const fx::Entry* entry = fx::ealib_find(sess.entries, "PALETTE.PAL")) {
            auto raw = fx::ealib_extract(sess.data.data(), sess.data.size(),
                                         *entry);
            if (!raw.empty())
                return fx::pal_load(raw.data(), raw.size());
        }
    }
    return fx::pal_load(nullptr, 0);
}

void DrawPaletteCombo(App& app, const char* autoLabel) {
    auto choices = EnumeratePalettes(app);

    const char* current = autoLabel;
    if (app.palLib == kPalGreyscale) {
        current = "Greyscale";
    } else if (app.palLib >= 0) {
        for (const auto& c : choices)
            if (c.lib == app.palLib && c.entry == app.palEntry)
                { current = c.label.c_str(); break; }
    }

    if (!ImGui::BeginCombo("##preview-palette", current))
        return;
    if (ImGui::Selectable(autoLabel, app.palLib == kPalAuto)) {
        app.palLib = kPalAuto;  app.palEntry = -1;  app.palGen++;
    }
    if (ImGui::Selectable("Greyscale", app.palLib == kPalGreyscale)) {
        app.palLib = kPalGreyscale;  app.palEntry = -1;  app.palGen++;
    }
    if (!choices.empty())
        ImGui::Separator();
    for (const auto& c : choices) {
        bool sel = (c.lib == app.palLib && c.entry == app.palEntry);
        if (ImGui::Selectable(c.label.c_str(), sel)) {
            app.palLib = c.lib;  app.palEntry = c.entry;  app.palGen++;
        }
    }
    ImGui::EndCombo();
}

void DrawPaletteSwatches(const char* id, const fx::Palette& pal, int count) {
    if (count <= 0)   return;
    if (count > 256)  count = 256;

    ImGui::PushID(id);
    float  sz      = ImGui::GetFontSize() * 1.05f;
    ImVec2 swatch  = {sz, sz};
    for (int i = 0; i < count; i++) {
        if (i % 16 != 0) ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushID(i);
        ImVec4 col = {pal.r[i] / 255.0f, pal.g[i] / 255.0f,
                      pal.b[i] / 255.0f, 1.0f};
        ImGui::ColorButton("##swatch", col,
                           ImGuiColorEditFlags_NoAlpha |
                           ImGuiColorEditFlags_NoTooltip |
                           ImGuiColorEditFlags_NoDragDrop, swatch);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Index %d\nRGB (%u, %u, %u)  #%02X%02X%02X",
                              i, pal.r[i], pal.g[i], pal.b[i],
                              pal.r[i], pal.g[i], pal.b[i]);
        ImGui::PopID();
    }
    ImGui::PopID();
}

} // namespace fxg
