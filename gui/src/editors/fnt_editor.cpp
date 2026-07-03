#include "fnt_editor.h"
#include "../app.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/fnt.h"
#include "fx/pe.h"
#include <vector>
#include <cstdio>
#include <cstring>

static fx::FntFile  s_fnt;
static GpuTexture   s_tex;
static int          s_lastLib   = -2;
static int          s_lastEntry = -2;

// Build an RGBA atlas of all rendered glyphs arranged in a 16Ã—16 grid.
// Each cell is cellWÃ—cellH pixels.  White glyph on dark background.
static GpuTexture BuildGlyphAtlas(const fx::FntFile& fnt) {
    // Determine cell size from the widest / tallest glyph.
    uint32_t cellW = 8, cellH = fnt.font_height ? fnt.font_height : 8;
    for (const auto& g : fnt.glyphs) {
        if (g.width  > cellW) cellW = g.width;
        if (g.height > cellH) cellH = g.height;
    }
    if (cellW < 1) cellW = 8;
    if (cellH < 1) cellH = 8;

    const int cols = 16, rows = 16;
    int atlasW = cols * (int)cellW;
    int atlasH = rows * (int)cellH;

    std::vector<uint8_t> rgba(atlasW * atlasH * 4, 0x20); // dark grey background
    // Make every 4th byte (alpha) fully opaque
    for (int i = 3; i < atlasW * atlasH * 4; i += 4)
        rgba[i] = 0xFF;

    // Blit each glyph into its cell
    for (const auto& g : fnt.glyphs) {
        if (g.pixels.empty()) continue;
        int col = g.ch % cols;
        int row = g.ch / cols;
        int ox  = col * (int)cellW;
        int oy  = row * (int)cellH;
        for (uint32_t py = 0; py < g.height && py < cellH; py++) {
            for (uint32_t px = 0; px < g.width && px < cellW; px++) {
                uint8_t v = g.pixels[py * g.width + px];
                if (!v) continue;  // transparent
                int x = ox + (int)px;
                int y = oy + (int)py;
                int idx = (y * atlasW + x) * 4;
                rgba[idx+0] = 0xFF; // R
                rgba[idx+1] = 0xFF; // G
                rgba[idx+2] = 0xFF; // B
                rgba[idx+3] = 0xFF; // A
            }
        }
    }

    return platform::UploadTexture(rgba.data(), atlasW, atlasH);
}

void DrawFntEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;

        s_tex.Release();
        s_fnt = fx::fnt_parse(ed.data.data(), ed.data.size());

        if (s_fnt.valid) {
            fx::CodeSection cs = fx::pe_code_section(ed.data.data(), ed.data.size());
            if (cs.data)
                fx::fnt_render_glyphs(s_fnt, cs.data, cs.size, cs.vma);
            s_tex = BuildGlyphAtlas(s_fnt);
        }
    }

    if (!s_fnt.valid) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse FNT file.");
        return;
    }

    int renderedCount = 0;
    for (const auto& g : s_fnt.glyphs)
        if (!g.pixels.empty()) renderedCount++;

    ImGui::TextDisabled("Font height: %u px  |  %d / 256 glyphs rendered",
                        s_fnt.font_height, renderedCount);
    ImGui::Separator();

    if (!s_tex.id) {
        ImGui::TextDisabled("(no glyph data to display)");
        return;
    }

    static float s_zoom = 2.0f;
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Zoom", &s_zoom, 1.0f, 4.0f, "%.0fx");

    float dispW = s_tex.width  * s_zoom;
    float dispH = s_tex.height * s_zoom;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::BeginChild("##fntScroll", ImGui::GetContentRegionAvail(), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)s_tex.id, ImVec2(dispW, dispH));

    // Tooltip showing character info on hover
    if (ImGui::IsItemHovered()) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        float fx = (mp.x - pos.x) / dispW;
        float fy = (mp.y - pos.y) / dispH;
        int col = (int)(fx * 16);
        int row = (int)(fy * 16);
        if (col >= 0 && col < 16 && row >= 0 && row < 16) {
            int ch = row * 16 + col;
            // Find glyph
            for (const auto& g : s_fnt.glyphs) {
                if (g.ch == ch) {
                    ImGui::SetTooltip("Char 0x%02X (%c)  %ux%u",
                        ch, (ch >= 32 && ch < 127) ? ch : '.',
                        g.width, g.height);
                    break;
                }
            }
        }
    }

    ImGui::EndChild();
}
