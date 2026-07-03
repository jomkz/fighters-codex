#include "pic_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/pic.h"
#include "fx/pal.h"
#include "fx/ealib.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image.h"

#include <commdlg.h>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

static std::string Win32SaveFile(const wchar_t* filter, const wchar_t* defExt) {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrDefExt  = defExt;
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return {};
    int len = WideCharToMultiByte(CP_UTF8,0,buf,-1,nullptr,0,nullptr,nullptr);
    std::string s(len-1,0);
    WideCharToMultiByte(CP_UTF8,0,buf,-1,s.data(),len,nullptr,nullptr);
    return s;
}
static std::string Win32OpenFile(const wchar_t* filter, const wchar_t* title) {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = title;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return {};
    int len = WideCharToMultiByte(CP_UTF8,0,buf,-1,nullptr,0,nullptr,nullptr);
    std::string s(len-1,0);
    WideCharToMultiByte(CP_UTF8,0,buf,-1,s.data(),len,nullptr,nullptr);
    return s;
}

void DrawPicEditor(App& app) {
    auto& ed = app.editor;
    fx::PicInfo info;
    bool valid = fx::pic_info(ed.data.data(), ed.data.size(), &info);

    if (valid && info.format != 0xD8FF) {
        ImGui::Text("Format: %s  |  %u x %u",
            info.format == 0 ? "Dense" : "Sparse",
            info.width, info.height);
        ImGui::Text("Palette: %u colors  |  Pixels: %u bytes",
            info.palette_size / 3, info.pixels_size);
    } else if (valid) {
        ImGui::Text("Format: JPEG");
    } else {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse PIC header.");
    }

    ImGui::Separator();

    if (ImGui::Button("Export PNG...")) {
        std::string path = Win32SaveFile(
            L"PNG Image\0*.png\0All Files\0*.*\0", L"png");
        if (!path.empty()) {
            // Find PALETTE.PAL in any open session for correct colour rendering.
            fx::Palette sysPal = fx::pal_load(nullptr, 0);
            for (const auto& sess : app.sessions) {
                const fx::Entry* pal = fx::ealib_find(sess.entries, "PALETTE.PAL");
                if (!pal) continue;
                auto raw = fx::ealib_extract(sess.data.data(), sess.data.size(),
                                             *pal);
                if (!raw.empty()) sysPal = fx::pal_load(raw.data(), raw.size());
                break;
            }
            auto rgba = fx::pic_decode(ed.data.data(), ed.data.size(), &sysPal);
            if (!rgba.empty() && valid)
                stbi_write_png(path.c_str(), (int)info.width, (int)info.height,
                               4, rgba.data(), (int)info.width * 4);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Import PNG...")) {
        std::string path = Win32OpenFile(
            L"PNG Image\0*.png\0BMP Image\0*.bmp\0All Files\0*.*\0",
            L"Import Image");
        if (!path.empty()) {
            int w=0,h=0,ch=0;
            uint8_t* rgba = stbi_load(path.c_str(), &w, &h, &ch, 4);
            if (rgba) {
                fx::Palette pal = {};
                auto encoded = fx::pic_encode(rgba, w, h, pal);
                stbi_image_free(rgba);
                if (!encoded.empty()) {
                    ed.data     = std::move(encoded);
                    ed.modified = true;
                    app.statusMsg  = "Imported " + fs::path(path).filename().string();
                    app.statusKind = App::StatusKind::Info;
                }
            }
        }
    }

    ImGui::TextDisabled("Preview shown in the Preview panel.");
}
