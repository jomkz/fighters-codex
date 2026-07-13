#include "sh_editor.h"
#include "../app.h"
#include "../platform/dialogs.h"
#include "imgui.h"
#include "fx/sh.h"
#include <fstream>
#include <string>

static int    s_shLastLib   = -2;
static int    s_shLastEntry = -2;
static fx::ShInfo s_shInfo  = {};
static fx::ShMesh s_shMesh  = {};

void DrawShEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_shLastLib || ed.entryIdx != s_shLastEntry) {
        s_shLastLib   = ed.libIdx;
        s_shLastEntry = ed.entryIdx;
        s_shInfo = fx::sh_parse_info(ed.data.data(), ed.data.size());
        s_shMesh = fx::sh_parse_mesh(ed.data.data(), ed.data.size());
    }

    ImGui::Text("Scale: %d  (%.1fx)",  s_shInfo.scale_raw, s_shInfo.scale);
    ImGui::Text("Vertices: %d   Faces: %d", s_shInfo.vert_count, s_shInfo.face_count);

    ImGui::SeparatorText("Bounding Box (feet)");
    ImGui::TextDisabled("  X  %.1f \xe2\x80\x93 %.1f", s_shInfo.bbox[0], s_shInfo.bbox[3]);
    ImGui::TextDisabled("  Y  %.1f \xe2\x80\x93 %.1f", s_shInfo.bbox[1], s_shInfo.bbox[4]);
    ImGui::TextDisabled("  Z  %.1f \xe2\x80\x93 %.1f", s_shInfo.bbox[2], s_shInfo.bbox[5]);

    if (!s_shInfo.textures.empty()) {
        ImGui::SeparatorText("Textures");
        for (auto& t : s_shInfo.textures) {
            // Cross-navigation (#365): a texture that resolves in the mounted
            // workspace opens in the PIC editor; SH names may omit ".PIC".
            const fxg::WorkspaceEntry* we = nullptr;
            if (app.workspace.mounted()) {
                we = app.workspace.find(t);
                if (!we && t.find('.') == std::string::npos)
                    we = app.workspace.find(t + ".PIC");
            }
            if (we) {
                if (ImGui::SmallButton(("  " + t).c_str()))
                    app.OpenWorkspaceEntry(
                        (int)(we - app.workspace.names.data()));
            } else {
                ImGui::TextDisabled("  %s", t.c_str());
            }
        }
    }

    ImGui::Separator();

    if (s_shInfo.vert_count == 0) {
        ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f},
            "x86-only geometry — no OBJ export available.");
    } else if (ImGui::Button("Export OBJ...")) {
        // Serialize before the dialog opens — the continuation runs frames
        // later, when the selection may have changed.
        platform::SaveFileDialog(
            {{"Wavefront OBJ", "obj;OBJ"}, {"All files", "*"}}, "obj", nullptr,
            [&app, obj = fx::sh_to_obj(s_shMesh)](std::string path) {
                if (path.empty()) return;
                std::ofstream f(path);
                if (f) {
                    f << obj;
                    app.statusMsg  = "Exported to " + path;
                    app.statusKind = App::StatusKind::Info;
                } else {
                    app.statusMsg  = "Cannot write: " + path;
                    app.statusKind = App::StatusKind::Error;
                }
            });
    }

    ImGui::TextDisabled("3D preview shown in the Preview panel.");
}
