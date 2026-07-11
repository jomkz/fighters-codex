#include "category_browser.h"
#include "lib_browser.h"
#include "../app.h"
#include "../util.h"
#include "../ui/icons.h"
#include "imgui.h"
#include <string>
#include <vector>

using fxs::icons::Id;
namespace ic = fxs::icons;

// Left-panel views == the icon set: eight categories then Archives (raw picker).
static const char* kViewLabel[ic::Count] = {
    "Aircraft", "Vehicles", "Weapons", "Missions",
    "Campaigns", "Terrain", "Audio", "Art/UI", "Archives",
};

// Icon textures are uploaded once (per DPI) and tinted with the theme foreground.
static platform::GpuTexture s_iconTex[ic::Count];
static int                   s_iconPx = -1;

static void EnsureIcons(int px) {
    if (px == s_iconPx) return;
    for (auto& t : s_iconTex) t.Release();
    for (int i = 0; i < ic::Count; ++i)
        s_iconTex[i] = ic::LoadIcon((Id)i, px);
    s_iconPx = px;
}

static void DrawIconBar(App& app) {
    const float fh  = ImGui::GetFontSize();
    const int   px  = (int)(fh * 1.5f);           // ~24px at default DPI
    EnsureIcons(px);
    const ImVec2 btn(fh * 1.6f, fh * 1.6f);
    const ImVec4 fg = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    const float  avail = ImGui::GetContentRegionAvail().x;
    float x = 0.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
    for (int i = 0; i < ic::Count; ++i) {
        const float step = btn.x + ImGui::GetStyle().ItemSpacing.x + 6.0f;
        if (i > 0 && x + step <= avail) ImGui::SameLine();
        x = (i > 0 && x + step <= avail) ? x + step : step;

        const bool active = ((int)app.leftView == i);
        const ImVec4 bg = active ? accent : ImVec4(0, 0, 0, 0);
        ImGui::PushID(i);
        bool clicked;
        if (s_iconTex[i].id) {
            clicked = ImGui::ImageButton("##icon", (ImTextureID)(intptr_t)s_iconTex[i].id,
                                         btn, ImVec2(0, 0), ImVec2(1, 1), bg, fg);
        } else {
            // No GL context (e.g. headless without a backing texture): text fallback.
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, accent);
            clicked = ImGui::Button(kViewLabel[i], ImVec2(btn.x + 6, btn.y));
            if (active) ImGui::PopStyleColor();
        }
        ImGui::PopID();
        if (clicked) app.leftView = (Id)i;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kViewLabel[i]);
    }
    ImGui::PopStyleVar();
}

static void DrawCategoryBrowser(App& app, fxg::Category cat) {
    if (!app.assetIndex.built) {
        ImGui::Spacing();
        if (app.workspace.mounted())
            ImGui::TextWrapped("Indexing the workspace...");
        else if (!app.installDir.empty()) {
            ImGui::TextWrapped("No workspace mounted.");
            if (ImGui::Button("Mount FA Workspace")) app.MountWorkspace();
        } else {
            ImGui::TextWrapped("Set the FA install directory in Preferences, then "
                               "File > Mount FA Workspace. Or open a LIB from the "
                               "Archives tab.");
        }
        return;
    }

    static char filter[64] = {};

    // The browser lists the category's *named objects* — entries whose primary
    // type is this category (an aircraft's skin PICs and shapes are reached
    // through its cluster and live under Art/UI, so they don't crowd the list).
    static std::vector<int> shown;
    shown.clear();
    for (int node : app.assetIndex.byCategory[(int)cat]) {
        const std::string& name = app.workspace.names[node].name;
        if (fxg::category_of(name) != cat) continue;
        if (filter[0] && !fxg::ci_contains(name.c_str(), filter)) continue;
        shown.push_back(node);
    }

    ImGui::Text("%s (%zu)", fxg::category_name(cat), shown.size());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##catfilter", "Filter...", filter, sizeof(filter));
    ImGui::Separator();

    ImGui::BeginChild("##catlist");
    ImGuiListClipper clipper;
    clipper.Begin((int)shown.size());
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            int node = shown[i];
            const std::string& name = app.workspace.names[node].name;
            ImGui::PushID(node);
            if (ImGui::Selectable(name.c_str(), app.selectedNode == node,
                                  ImGuiSelectableFlags_AllowDoubleClick))
                app.OpenWorkspaceEntry(node);
            ImGui::PopID();
        }
    ImGui::EndChild();
}

void DrawLeftPanel(App& app) {
    DrawIconBar(app);
    ImGui::Separator();
    if (app.leftView == Id::Archives)
        DrawLibBrowser(app);              // unchanged raw per-LIB picker
    else
        DrawCategoryBrowser(app, (fxg::Category)(int)app.leftView);
}
