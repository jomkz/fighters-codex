#include "lay_editor.h"
#include "../app.h"
#include "imgui.h"
#include "ft/lay.h"
#include <string>
#include <cstdio>

static ft::LayFile s_lay;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

void DrawLayEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_lay = ft::lay_parse(ed.data.data(), ed.data.size());
    }

    if (!s_lay.valid) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse LAY file.");
        return;
    }

    ImGui::TextDisabled("%d atmosphere layer(s)", (int)s_lay.layers.size());
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));

    for (int li = 0; li < (int)s_lay.layers.size(); li++) {
        const auto& lay = s_lay.layers[li];

        char hdr[64];
        snprintf(hdr, sizeof(hdr), "Layer %d  alt %d..%d ft", li,
                 (int)lay.alt_min, (int)lay.alt_max);

        if (ImGui::CollapsingHeader(hdr)) {
            // Summary table
            if (ImGui::BeginTable(("##laytbl" + std::to_string(li)).c_str(), 2,
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0, 0))) {
                ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 160);
                ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

                auto Row = [](const char* k, const char* v) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", k);
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(v);
                };
                char buf[128];

                snprintf(buf, sizeof(buf), "%d ft", (int)lay.sel_alt_min);  Row("Sel alt min",    buf);
                snprintf(buf, sizeof(buf), "%d ft", (int)lay.sel_alt_max);  Row("Sel alt max",    buf);
                snprintf(buf, sizeof(buf), "%d ft", (int)lay.fog_alt_low);  Row("Fog alt low",    buf);
                snprintf(buf, sizeof(buf), "%d ft", (int)lay.fog_alt_high); Row("Fog alt high",   buf);
                snprintf(buf, sizeof(buf), "%d",    (int)lay.vis_lo);       Row("Visibility lo",  buf);
                snprintf(buf, sizeof(buf), "%d",    (int)lay.vis_hi);       Row("Visibility hi",  buf);
                snprintf(buf, sizeof(buf), "%u",    lay.fog_density);       Row("Fog density",    buf);
                snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                         lay.base_rgb[0], lay.base_rgb[1], lay.base_rgb[2]);
                Row("Base color", buf);
                if (!lay.cloud_pic.empty()) Row("Cloud PIC",   lay.cloud_pic.c_str());
                if (!lay.sky_pic.empty())   Row("Sky PIC",     lay.sky_pic.c_str());
                snprintf(buf, sizeof(buf), "%u", (unsigned)lay.visibility); Row("Visibility idx", buf);

                ImGui::EndTable();
            }

            // Zenith gradient strip
            ImGui::TextDisabled("Zenith gradient (31 steps):");
            ImGui::SameLine();
            for (int gi = 0; gi < 31; gi++) {
                ImVec4 col(lay.zenith_grad[gi].r / 255.0f,
                           lay.zenith_grad[gi].g / 255.0f,
                           lay.zenith_grad[gi].b / 255.0f, 1.0f);
                char cid[16]; snprintf(cid, sizeof(cid), "##zg%d_%d", li, gi);
                ImGui::ColorButton(cid, col,
                    ImGuiColorEditFlags_NoTooltip |
                    ImGuiColorEditFlags_NoPicker  |
                    ImGuiColorEditFlags_NoBorder,
                    ImVec2(10, 14));
                if (gi < 30) ImGui::SameLine(0, 1);
            }

            // Horizon gradient strip
            ImGui::TextDisabled("Horizon gradient (32 steps):");
            ImGui::SameLine();
            for (int gi = 0; gi < 32; gi++) {
                ImVec4 col(lay.horizon_grad[gi].r / 255.0f,
                           lay.horizon_grad[gi].g / 255.0f,
                           lay.horizon_grad[gi].b / 255.0f, 1.0f);
                char cid[16]; snprintf(cid, sizeof(cid), "##hg%d_%d", li, gi);
                ImGui::ColorButton(cid, col,
                    ImGuiColorEditFlags_NoTooltip |
                    ImGuiColorEditFlags_NoPicker  |
                    ImGuiColorEditFlags_NoBorder,
                    ImVec2(10, 14));
                if (gi < 31) ImGui::SameLine(0, 1);
            }

            ImGui::Spacing();
        }
    }
    ImGui::PopStyleVar();
}
