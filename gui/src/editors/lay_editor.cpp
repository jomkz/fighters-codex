#include "lay_editor.h"
#include "../app.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/lay.h"
#include "fx_render/render.h"
#include "overlay_preview.h"
#include <algorithm>
#include <string>
#include <cstdio>

static fx::LayFile s_lay;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

// ---- In-game-style sky preview state (#283) ----
static GpuTexture s_skyTex;
static bool       s_skyDirty   = true;
static int        s_skyLayer   = 0;
static int        s_skyHorizon = 60;  // percent of frame height

static void RenderSkyPreview() {
    const int W = 320, H = 220;
    fx_render::fa::Surface surface(W, H);
    surface.Clear(0);
    fx_render::fa::Palette pal;
    if (s_skyLayer >= 0 && s_skyLayer < (int)s_lay.layers.size())
        fxg::DrawLaySky(surface, pal, s_lay.layers[(size_t)s_skyLayer],
                        H * s_skyHorizon / 100);

    fx_render::Image img;
    pal.Present(surface, img);
    s_skyTex.Release();
    s_skyTex = platform::UploadTexture(img.pixels.data(), W, H);
}

void DrawLayEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_lay = fx::lay_parse(ed.data.data(), ed.data.size());
        s_skyLayer = 0;
        s_skyDirty = true;
    }

    if (!s_lay.valid) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse LAY file.");
        return;
    }

    ImGui::TextDisabled("%d atmosphere layer(s)", (int)s_lay.layers.size());
    ImGui::Separator();

    // ---- In-game-style sky preview (#283) ----
    if (ImGui::CollapsingHeader("Sky Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool ch = false;
        ImGui::PushItemWidth(120);
        ch |= ImGui::SliderInt("Layer", &s_skyLayer, 0,
                               std::max(0, (int)s_lay.layers.size() - 1));
        ImGui::SameLine();
        ch |= ImGui::SliderInt("Horizon %", &s_skyHorizon, 10, 90);
        ImGui::PopItemWidth();
        if (ch) s_skyDirty = true;

        if (s_skyDirty) {
            RenderSkyPreview();
            s_skyDirty = false;
        }
        if (s_skyTex.id)
            ImGui::Image((ImTextureID)(intptr_t)s_skyTex.id, ImVec2(320, 220));

        // The documented per-angle band selection at level flight.
        int lvl = fxg::LaySelectLayer(s_lay, 0);
        if (lvl >= 0)
            ImGui::TextDisabled(
                "Gradients per this layer's ramps (GouraudHorizon banding); "
                "level-flight band selects layer %d. See docs/gui.md.", lvl);
        else
            ImGui::TextDisabled(
                "Gradients per this layer's ramps (GouraudHorizon banding). "
                "See docs/gui.md.");
    }

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
                char cid[24]; snprintf(cid, sizeof(cid), "##zg%d_%d", li, gi);
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
                char cid[24]; snprintf(cid, sizeof(cid), "##hg%d_%d", li, gi);
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
