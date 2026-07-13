#include "cb8_editor.h"
#include "../app.h"
#include "../palettes.h"
#include "../platform/dialogs.h"
#include "../platform/texture.h"
#include "imgui.h"
#include "fx/cb8.h"
#include "fx/pal.h"

// Implementation already compiled via pic_editor.cpp
#include "stb_image_write.h"

#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;

static fx::Cb8Decoder* s_dec        = nullptr;
static fx::Cb8Info     s_info       = {};
static GpuTexture      s_frameTex;
static uint32_t        s_curFrame   = 0;
static int             s_lastLib    = -2;
static int             s_lastEntry  = -2;

// Every CB8 frame carries its own 768-byte palette (engine-traced, #95):
// the preview renders true colour with no external palette at all.
static GpuTexture DecodeToTexture(const App&, uint32_t frameIdx) {
    if (!s_dec) return {};
    auto rgba = fx::cb8_decode_frame_rgba(s_dec, frameIdx);
    if (rgba.empty()) return {};
    return platform::UploadTexture(rgba.data(), (int)s_info.width, (int)s_info.height);
}

void DrawCb8Editor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib    = ed.libIdx;
        s_lastEntry  = ed.entryIdx;
        s_frameTex.Release();
        if (s_dec) { fx::cb8_close(s_dec); s_dec = nullptr; }
        s_curFrame = 0;

        if (fx::cb8_info(ed.data.data(), ed.data.size(), &s_info)) {
            s_dec = fx::cb8_open(ed.data.data(), ed.data.size());
            if (s_dec)
                s_frameTex = DecodeToTexture(app, 0);
        }
    }

    if (!s_dec) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot open CB8 decoder.");
        return;
    }

    ImGui::TextDisabled("%u x %u  |  %u frames  |  15 fps  |  per-frame embedded palette",
                        s_info.width, s_info.height, s_info.frame_count);
    ImGui::Separator();

    // Frame navigation — guard against zero-frame files (slider max must be >= min)
    if (s_info.frame_count > 0) {
        int frame   = (int)s_curFrame;
        bool changed = false;

        // Prev / Next buttons come first so the slider can fill remaining width
        if (ImGui::SmallButton("< Prev") && frame > 0)
            { --frame; changed = true; }
        ImGui::SameLine();
        if (ImGui::SmallButton("Next >") && (uint32_t)(frame + 1) < s_info.frame_count)
            { ++frame; changed = true; }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##frame", &frame, 0, (int)s_info.frame_count - 1, "Frame %d"))
            changed = true;

        if (changed && (uint32_t)frame != s_curFrame) {
            s_curFrame = (uint32_t)frame;
            s_frameTex.Release();
            s_frameTex = DecodeToTexture(app, s_curFrame);
        }
    }

    if (ImGui::Button("Export All Frames as PNG...")) {
        // Snapshot the CB8 bytes before the dialog opens — the continuation
        // runs frames later, when the selection may have changed. Frames
        // export in true colour through their embedded palettes.
        platform::ChooseFolderDialog(
            [&app, data = ed.data, info = s_info](std::string dir) {
                if (dir.empty()) return;
                int w = (int)info.width, h = (int)info.height;

                // Sequential export with a fresh decoder over the snapshot
                fx::Cb8Decoder* expDec = fx::cb8_open(data.data(), data.size());
                if (!expDec) return;
                for (uint32_t f = 0; f < info.frame_count; f++) {
                    auto rgba = fx::cb8_decode_frame_rgba(expDec, f);
                    if (rgba.empty()) continue;
                    char fname[64]; snprintf(fname, sizeof(fname), "frame_%04u.png", f);
                    std::string outPath = (fs::path(dir) / fname).string();
                    stbi_write_png(outPath.c_str(), w, h, 4, rgba.data(), w*4);
                }
                fx::cb8_close(expDec);
                app.statusMsg  = "Exported " + std::to_string(info.frame_count) + " frames to " + dir;
                app.statusKind = App::StatusKind::Info;
            });
    }

    ImGui::Separator();

    // Frame display
    if (s_frameTex.id && s_frameTex.width > 0) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float scale = avail.x / (float)s_frameTex.width;
        if (scale > 1.0f) scale = 1.0f;
        float dw = s_frameTex.width  * scale;
        float dh = s_frameTex.height * scale;
        ImGui::Image((ImTextureID)(intptr_t)s_frameTex.id, ImVec2(dw, dh));
    }
}
