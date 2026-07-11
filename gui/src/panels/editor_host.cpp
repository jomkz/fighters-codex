#include "editor_host.h"
#include "../app.h"
#include "../editors/brf_editor.h"
#include "../editors/pic_editor.h"
#include "../editors/audio_editor.h"
#include "../editors/mission_editor.h"
#include "../editors/seq_editor.h"
#include "../editors/inf_editor.h"
#include "../editors/plt_editor.h"
#include "../editors/raw_viewer.h"
#include "../editors/sh_editor.h"
#include "../editors/txt_editor.h"
#include "../editors/bin_editor.h"
#include "../editors/lay_editor.h"
#include "../editors/hud_editor.h"
#include "../editors/mus_editor.h"
#include "../editors/fnt_editor.h"
#include "../editors/cb8_editor.h"
#include "../editors/ai_editor.h"
#include "../editors/xmi_editor.h"
#include "../editors/vdo_editor.h"
#include "../editors/cam_editor.h"
#include "../editors/pal_editor.h"
#include "imgui.h"

// Object scope (#365): while an object is selected in a category browser, its
// file cluster renders as a compact strip of chips above the editor — the
// entity record first, then shapes, textures, sounds. Clicking a chip opens
// that file through the workspace pipeline without dropping the scope.
static void DrawClusterStrip(App& app) {
    if (app.clusterRoot < 0 || app.cluster.empty() || !app.workspace.mounted())
        return;

    const std::string& rootName = app.workspace.names[app.clusterRoot].name;
    ImGui::TextDisabled("%s cluster — %zu files", rootName.c_str(),
                        app.cluster.size());

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    const float wrapX = ImGui::GetContentRegionMax().x;
    bool first = true;
    for (int node : app.cluster) {
        if (node < 0 || node >= (int)app.workspace.names.size()) continue;
        const std::string& name = app.workspace.names[node].name;

        // Wrap chips into rows within the panel width.
        const float w = ImGui::CalcTextSize(name.c_str()).x +
                        ImGui::GetStyle().FramePadding.x * 2.0f;
        if (!first) {
            ImGui::SameLine();
            if (ImGui::GetCursorPosX() + w > wrapX) ImGui::NewLine();
        }
        first = false;

        const bool current = (node == app.selectedNode);
        if (!current)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushID(node);
        if (ImGui::SmallButton(name.c_str()))
            app.OpenWorkspaceEntry(node);
        ImGui::PopID();
        if (!current) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", fxg::category_name(
                fxg::category_of(name)));
    }
    ImGui::PopStyleVar();
    ImGui::Separator();
}

void DrawEditorHost(App& app) {
    DrawClusterStrip(app);

    if (app.editor.kind == EditorKind::None) {
        ImGui::TextDisabled("Select a record in the LIB Browser to edit.");
        return;
    }

    // Record title bar
    if (app.editor.libIdx >= 0 && app.editor.libIdx < (int)app.sessions.size()) {
        const auto& e = app.sessions[app.editor.libIdx]
                            .entries[app.editor.entryIdx];
        ImGui::Text("%s", e.name);
        if (app.editor.modified) {
            ImGui::SameLine();
            ImGui::TextColored({1,0.8f,0,1}, "(modified)");
            ImGui::SameLine();
            if (ImGui::SmallButton("Commit")) {
                // Editors write back to editor.data when saving;
                // CommitEntry patches it into the session.
                app.CommitEntry(app.editor.data);
            }
        }
        ImGui::Separator();
    }

    switch (app.editor.kind) {
    case EditorKind::Brf:     DrawBrfEditor(app);     break;
    case EditorKind::Pic:     DrawPicEditor(app);     break;
    case EditorKind::Audio:   DrawAudioEditor(app);   break;
    case EditorKind::Mission: DrawMissionEditor(app); break;
    case EditorKind::Seq:     DrawSeqEditor(app);     break;
    case EditorKind::Inf:     DrawInfEditor(app);     break;
    case EditorKind::Plt:     DrawPltEditor(app);     break;
    case EditorKind::Raw:     DrawRawViewer(app);     break;
    case EditorKind::Sh:      DrawShEditor(app);      break;
    case EditorKind::Txt:     DrawTxtEditor(app);     break;
    case EditorKind::Bin:     DrawBinEditor(app);     break;
    case EditorKind::Lay:     DrawLayEditor(app);     break;
    case EditorKind::Hud:     DrawHudEditor(app);     break;
    case EditorKind::Mus:     DrawMusEditor(app);     break;
    case EditorKind::Fnt:     DrawFntEditor(app);     break;
    case EditorKind::Cb8:     DrawCb8Editor(app);     break;
    case EditorKind::Ai:      DrawAiEditor(app);      break;
    case EditorKind::Xmi:     DrawXmiEditor(app);     break;
    case EditorKind::Vdo:     DrawVdoEditor(app);     break;
    case EditorKind::Cam:     DrawCamEditor(app);     break;
    case EditorKind::Pal:     DrawPalEditor(app);     break;
    default:
        ImGui::TextDisabled("No editor for .%s files.", app.editor.ext.c_str());
        break;
    }

}
