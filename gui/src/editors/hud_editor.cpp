#include "hud_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/hud.h"
#include <string>
#include <cstdio>

static fx::HudFile s_hud;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

void DrawHudEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_hud = fx::hud_parse(ed.data.data(), ed.data.size());
    }

    if (!s_hud.valid) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse HUD file.");
        return;
    }

    ImGui::TextDisabled("HUD overlay DLL  |  %zu asset(s)  |  %zu param(s)",
                        s_hud.asset_strings.size(), s_hud.params.size());
    ImGui::Separator();

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
