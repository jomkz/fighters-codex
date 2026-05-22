#include "sh_editor.h"
#include "../app.h"
#include "imgui.h"
#include "ft/sh.h"
#include <commdlg.h>
#include <fstream>
#include <string>

static int    s_shLastLib   = -2;
static int    s_shLastEntry = -2;
static ft::ShInfo s_shInfo  = {};
static ft::ShMesh s_shMesh  = {};

void DrawShEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_shLastLib || ed.entryIdx != s_shLastEntry) {
        s_shLastLib   = ed.libIdx;
        s_shLastEntry = ed.entryIdx;
        s_shInfo = ft::sh_parse_info(ed.data.data(), ed.data.size());
        s_shMesh = ft::sh_parse_mesh(ed.data.data(), ed.data.size());
    }

    ImGui::Text("Scale: %d  (%.1fx)",  s_shInfo.scale_raw, s_shInfo.scale);
    ImGui::Text("Vertices: %d   Faces: %d", s_shInfo.vert_count, s_shInfo.face_count);

    ImGui::SeparatorText("Bounding Box (feet)");
    ImGui::TextDisabled("  X  %.1f \xe2\x80\x93 %.1f", s_shInfo.bbox[0], s_shInfo.bbox[3]);
    ImGui::TextDisabled("  Y  %.1f \xe2\x80\x93 %.1f", s_shInfo.bbox[1], s_shInfo.bbox[4]);
    ImGui::TextDisabled("  Z  %.1f \xe2\x80\x93 %.1f", s_shInfo.bbox[2], s_shInfo.bbox[5]);

    if (!s_shInfo.textures.empty()) {
        ImGui::SeparatorText("Textures");
        for (auto& t : s_shInfo.textures)
            ImGui::TextDisabled("  %s", t.c_str());
    }

    ImGui::Separator();

    if (s_shInfo.vert_count == 0) {
        ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f},
            "x86-only geometry — no OBJ export available.");
    } else if (ImGui::Button("Export OBJ...")) {
        wchar_t buf[MAX_PATH] = {};
        OPENFILENAMEW ofn     = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
        ofn.lpstrFilter  = L"Wavefront OBJ\0*.obj\0All Files\0*.*\0";
        ofn.lpstrFile    = buf;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrDefExt  = L"obj";
        ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (GetSaveFileNameW(&ofn)) {
            int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
            std::string path(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, buf, -1, path.data(), len, nullptr, nullptr);
            std::ofstream f(path);
            if (f) {
                f << ft::sh_to_obj(s_shMesh);
                app.statusMsg  = "Exported to " + path;
                app.statusKind = App::StatusKind::Info;
            } else {
                app.statusMsg  = "Cannot write: " + path;
                app.statusKind = App::StatusKind::Error;
            }
        }
    }

    ImGui::TextDisabled("3D preview shown in the Preview panel.");
}
