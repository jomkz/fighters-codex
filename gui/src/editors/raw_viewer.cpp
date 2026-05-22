#include "raw_viewer.h"
#include "../app.h"
#include "imgui.h"
#include "ft/raw.h"
#include "stb_image_write.h"
#include <commdlg.h>
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

static std::string SavePngDialog(const std::string& defaultPath) {
    fs::path def(defaultPath);
    std::wstring wdef = def.wstring();
    std::wstring initDir = def.parent_path().wstring();

    wchar_t buf[MAX_PATH] = {};
    if (wdef.size() < MAX_PATH)
        wcscpy_s(buf, wdef.c_str());

    OPENFILENAMEW ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFilter     = L"PNG Image\0*.png\0All Files\0*.*\0";
    ofn.lpstrFile       = buf;
    ofn.nMaxFile        = MAX_PATH;
    ofn.lpstrDefExt     = L"png";
    ofn.lpstrInitialDir = initDir.empty() ? nullptr : initDir.c_str();
    ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return {};

    int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, s.data(), len, nullptr, nullptr);
    return s;
}

void DrawRawViewer(App& app) {
    auto& ed = app.editor;
    ft::RawInfo info;
    if (!ft::raw_info(ed.data.data(), ed.data.size(), &info)) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Not a valid RAW screenshot.");
        return;
    }

    ImGui::Text("Resolution: %u x %u", info.width, info.height);
    ImGui::Text("File size:  %zu bytes", ed.data.size());
    ImGui::Separator();

    if (ImGui::Button("Export PNG...")) {
        // Build a default save path: same dir/name as source, .png extension
        std::string defaultPath;
        if (ed.libIdx >= 0 && ed.libIdx < (int)app.sessions.size()) {
            const auto& sess = app.sessions[ed.libIdx];
            if (sess.standalone) {
                defaultPath = fs::path(sess.path).replace_extension(".png").string();
            } else if (ed.entryIdx >= 0 && ed.entryIdx < (int)sess.entries.size()) {
                fs::path stem = fs::path(sess.entries[ed.entryIdx].name).stem();
                defaultPath = (fs::path(sess.path).parent_path() / (stem.string() + ".png")).string();
            }
        }

        std::string out = SavePngDialog(defaultPath);
        if (!out.empty()) {
            auto rgba = ft::raw_decode(ed.data.data(), ed.data.size());
            if (!rgba.empty()) {
                if (stbi_write_png(out.c_str(), (int)info.width, (int)info.height,
                                   4, rgba.data(), (int)info.width * 4)) {
                    app.statusMsg  = "Exported " + fs::path(out).filename().string();
                    app.statusKind = App::StatusKind::Info;
                } else {
                    app.statusMsg  = "PNG write failed: " + out;
                    app.statusKind = App::StatusKind::Error;
                }
            }
        }
    }

    ImGui::TextDisabled("Preview shown in the Preview panel.");
}
