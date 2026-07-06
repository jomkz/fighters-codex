#include "hud_editor.h"
#include "../app.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/fnt.h"
#include "fx/hud.h"
#include "fx/pe.h"
#include "fx_render/render.h"
#include "overlay_preview.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <cstdio>

static fx::HudFile s_hud;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

// ---- In-game-style preview state (#283) ----
static fxg::HudPreviewState s_state;
static GpuTexture           s_previewTex;
static bool                 s_previewDirty = true;
static fx::FntFile          s_font;      // rendered install FNT, when found
static bool                 s_fontValid  = false;
static std::string          s_fontName;

// Find a HUD-usable FNT across the open sessions: first entry whose name
// starts with one of the file's font asset strings (then the generic
// prefixes), rendered ready for the overlay text.
static void LoadPreviewFont(App& app) {
    s_fontValid = false;
    s_fontName.clear();
    auto lower = [](std::string v) {
        for (char& c : v) c = (char)std::tolower((unsigned char)c);
        return v;
    };
    std::vector<std::string> prefixes;
    for (const auto& a : s_hud.asset_strings)
        if (!a.empty() && a[0] != '~') prefixes.push_back(lower(a));
    prefixes.push_back("hud");
    prefixes.push_back("4x6");

    for (const auto& prefix : prefixes) {
        for (const auto& sess : app.sessions) {
            for (const auto& e : sess.entries) {
                std::string name = lower(e.name);
                if (name.size() < 4 || name.substr(name.size() - 4) != ".fnt")
                    continue;
                if (name.rfind(prefix, 0) != 0) continue;
                auto bytes = fx::ealib_extract(sess.data.data(), sess.data.size(),
                                               e, true);
                if (bytes.empty()) continue;
                fx::FntFile fnt = fx::fnt_parse(bytes.data(), bytes.size());
                if (!fnt.valid) continue;
                fx::CodeSection cs = fx::pe_code_section(bytes.data(), bytes.size());
                if (!cs.data) continue;
                fx::fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);
                s_font      = std::move(fnt);
                s_fontValid = true;
                s_fontName  = e.name;
                return;
            }
        }
    }
}

static void RenderHudPreview() {
    const int W = 512, H = 384;
    fx_render::fa::Surface surface(W, H);
    surface.Clear(0);
    fx_render::fa::Palette pal;             // entry 0 stays black
    pal.entries[1] = {0, 63, 0};            // stand-in HUD green (see gui.md)

    fx_render::fa::Raster raster(surface);
    fxg::OverlayText text(s_fontValid ? &s_font : nullptr);
    fxg::DrawHudPreview(raster, s_hud, s_state, text, 1);

    fx_render::Image img;
    pal.Present(surface, img);
    s_previewTex.Release();
    s_previewTex = platform::UploadTexture(img.pixels.data(), W, H);
}

void DrawHudEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_hud = fx::hud_parse(ed.data.data(), ed.data.size());
        if (s_hud.valid) LoadPreviewFont(app);
        s_previewDirty = true;
    }

    if (!s_hud.valid) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse HUD file.");
        return;
    }

    ImGui::TextDisabled("HUD overlay DLL  |  %zu asset(s)  |  %zu param(s)",
                        s_hud.asset_strings.size(), s_hud.params.size());
    ImGui::Separator();

    // ---- In-game-style preview (#283) ----
    if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool ch = false;
        ImGui::PushItemWidth(140);
        ch |= ImGui::SliderInt("Heading", &s_state.heading_deg, 0, 359);
        ImGui::SameLine();
        ch |= ImGui::SliderInt("Speed", &s_state.speed_kt, 0, 1200);
        ImGui::SameLine();
        ch |= ImGui::SliderInt("Altitude", &s_state.altitude_ft, 0, 40000);
        ImGui::PopItemWidth();
        ch |= ImGui::Checkbox("Gear",  &s_state.gear);  ImGui::SameLine();
        ch |= ImGui::Checkbox("Flap",  &s_state.flap);  ImGui::SameLine();
        ch |= ImGui::Checkbox("Brake", &s_state.brake); ImGui::SameLine();
        ch |= ImGui::Checkbox("Hook",  &s_state.hook);  ImGui::SameLine();
        ch |= ImGui::Checkbox("Warning", &s_state.warning);
        if (ch) s_previewDirty = true;

        if (s_previewDirty) {
            RenderHudPreview();
            s_previewDirty = false;
        }
        if (s_previewTex.id) {
            float availW = ImGui::GetContentRegionAvail().x;
            float scale  = availW > 16 ? std::min(1.5f, availW / 512.0f) : 1.0f;
            ImGui::Image((ImTextureID)(intptr_t)s_previewTex.id,
                         ImVec2(512 * scale, 384 * scale));
        }
        ImGui::TextDisabled(
            "Symbology positions from this file's gauge params; flight state "
            "simulated. Font: %s. Colour is a stand-in (docs/gui.md).",
            s_fontValid ? s_fontName.c_str() : "built-in 4x6");
    }

    // ---- Asset strings ----
    if (ImGui::CollapsingHeader("Asset References", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,2));
        if (ImGui::BeginTable("##hudassets", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            auto AssetRow = [](const char* type, const std::string& name) {
                if (name.empty()) return;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", type);
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(name.c_str());
            };
            AssetRow("Icon A", s_hud.icon_a);
            AssetRow("Icon B", s_hud.icon_b);
            AssetRow("Icon C", s_hud.icon_c);
            AssetRow("Icon D", s_hud.icon_d);
            for (const auto& s : s_hud.asset_strings) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Asset");
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(s.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    // ---- Gauge params ----
    if (ImGui::CollapsingHeader("Gauge Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,2));
        if (ImGui::BeginTable("##hudparams", 3,
                ImGuiTableFlags_ScrollY       |
                ImGuiTableFlags_RowBg         |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable,
                ImVec2(0, 0))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Gauge", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();

            for (const auto& p : s_hud.params) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(p.gauge.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(p.field.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%d", (int)p.value);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
}
