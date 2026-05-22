#include "cb8_editor.h"
#include "../app.h"
#include "imgui.h"
#include "ft/cb8.h"

// Implementation already compiled via pic_editor.cpp
#include "stb_image_write.h"

#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;

static ft::Cb8Decoder* s_dec       = nullptr;
static ft::Cb8Info     s_info      = {};
static GpuTexture      s_frameTex;
static uint32_t        s_curFrame  = 0;
static int             s_lastLib   = -2;
static int             s_lastEntry = -2;

// CB8 videos use an engine-internal palette that is not stored in any LIB file.
// PALETTE.PAL has palette index 1 = magenta, but CB8 frames are saturated with
// index 1 (sky/background), so applying PALETTE.PAL produces a garbled pink image.
// We display in greyscale (index → grey intensity), which is always spatially correct.
static GpuTexture DecodeToTexture(App& app, uint32_t frameIdx) {
    if (!s_dec) return {};
    auto indices = ft::cb8_decode_frame(s_dec, frameIdx);
    if (indices.empty()) return {};

    int w = (int)s_info.width;
    int h = (int)s_info.height;
    std::vector<uint8_t> rgba(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        uint8_t v = indices[i];   // greyscale: index maps to its own value as brightness
        rgba[i*4+0] = v;
        rgba[i*4+1] = v;
        rgba[i*4+2] = v;
        rgba[i*4+3] = 0xFF;
    }
    return app.UploadTexture(rgba.data(), w, h);
}

static std::string Win32ChooseFolder() {
    wchar_t buf[MAX_PATH] = {};
    BROWSEINFOW bi = {};
    bi.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    bi.lpszTitle = L"Export frames to folder";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return {};
    SHGetPathFromIDListW(pidl, buf);
    CoTaskMemFree(pidl);
    int len = WideCharToMultiByte(CP_UTF8,0,buf,-1,nullptr,0,nullptr,nullptr);
    std::string s(len-1,0);
    WideCharToMultiByte(CP_UTF8,0,buf,-1,s.data(),len,nullptr,nullptr);
    return s;
}

void DrawCb8Editor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_frameTex.Release();
        if (s_dec) { ft::cb8_close(s_dec); s_dec = nullptr; }
        s_curFrame = 0;

        if (ft::cb8_info(ed.data.data(), ed.data.size(), &s_info)) {
            s_dec = ft::cb8_open(ed.data.data(), ed.data.size());
            if (s_dec)
                s_frameTex = DecodeToTexture(app, 0);
        }
    }

    if (!s_dec) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot open CB8 decoder.");
        return;
    }

    ImGui::TextDisabled("%u x %u  |  %u frames  |  15 fps  |  greyscale (engine palette not in LIBs)",
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
        std::string dir = Win32ChooseFolder();
        if (!dir.empty()) {
            int w = (int)s_info.width, h = (int)s_info.height;
            std::vector<uint8_t> rgba(w * h * 4);

            // Sequential export with a fresh decoder
            ft::Cb8Decoder* expDec = ft::cb8_open(ed.data.data(), ed.data.size());
            if (expDec) {
                for (uint32_t f = 0; f < s_info.frame_count; f++) {
                    auto idx = ft::cb8_decode_frame(expDec, f);
                    if (idx.empty()) continue;
                    for (int i = 0; i < w*h; i++) {
                        uint8_t v = idx[i];   // greyscale
                        rgba[i*4+0] = v;
                        rgba[i*4+1] = v;
                        rgba[i*4+2] = v;
                        rgba[i*4+3] = 0xFF;
                    }
                    char fname[64]; snprintf(fname, sizeof(fname), "frame_%04u.png", f);
                    std::string outPath = dir + "\\" + fname;
                    stbi_write_png(outPath.c_str(), w, h, 4, rgba.data(), w*4);
                }
                ft::cb8_close(expDec);
                app.statusMsg  = "Exported " + std::to_string(s_info.frame_count) + " frames to " + dir;
                app.statusKind = App::StatusKind::Info;
            }
        }
    }

    ImGui::Separator();

    // Frame display
    if (s_frameTex.srv && s_frameTex.width > 0) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float scale = avail.x / (float)s_frameTex.width;
        if (scale > 1.0f) scale = 1.0f;
        float dw = s_frameTex.width  * scale;
        float dh = s_frameTex.height * scale;
        ImGui::Image((ImTextureID)(intptr_t)s_frameTex.srv, ImVec2(dw, dh));
    }
}
