#include "category_browser.h"
#include "lib_browser.h"
#include "../app.h"
#include "../util.h"
#include "../ui/icons.h"
#include "imgui.h"
#include <algorithm>
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

// The object categories browse as a thumbnail grid (#366): these are the
// buckets whose entries name a shape through the asset graph.
static bool GridCategory(fxg::Category cat) {
    return cat == fxg::Category::Aircraft || cat == fxg::Category::Vehicles ||
           cat == fxg::Category::Weapons;
}

// Thumbnail grid over the filtered nodes. Renders are requested only for
// cells the clipper makes visible, so a full install populates progressively;
// until (or unless) a shape resolves, the cell shows the category icon.
static void DrawThumbnailGrid(App& app, fxg::Category cat,
                              const std::vector<int>& shown) {
    const float fh     = ImGui::GetFontSize();
    const float cell   = fh * 7.0f;                    // thumbnail square edge
    const float labelH = ImGui::GetTextLineHeight();
    const ImVec2 cellSz(cell, cell + labelH + 4.0f);
    const ImGuiStyle& st = ImGui::GetStyle();

    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + st.ItemSpacing.x) /
                                 (cell + st.ItemSpacing.x)));
    int rows = ((int)shown.size() + cols - 1) / cols;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiListClipper clipper;
    clipper.Begin(rows, cellSz.y + st.ItemSpacing.y);
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            for (int col = 0; col < cols; ++col) {
                int i = row * cols + col;
                if (i >= (int)shown.size()) break;
                int node = shown[i];
                const std::string& name = app.workspace.names[node].name;

                if (col > 0) ImGui::SameLine();
                ImGui::PushID(node);
                ImVec2 pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##cell", app.selectedNode == node, 0, cellSz))
                    app.SelectObject(node); // scope the editors to its cluster (#365)
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", name.c_str());

                auto it = app.thumbTex.find(node);
                if (it != app.thumbTex.end() && it->second.id) {
                    dl->AddImage((ImTextureID)(intptr_t)it->second.id, pos,
                                 ImVec2(pos.x + cell, pos.y + cell));
                } else {
                    // Visible + unrendered: queue it (idempotent). Nodes the
                    // service reported shapeless stay on the icon placeholder.
                    if (app.thumbs.running() && !app.thumbMissing.count(node))
                        app.thumbs.Request(node);
                    if (s_iconTex[(int)cat].id) {
                        float is = cell * 0.4f;
                        ImVec2 ip(pos.x + (cell - is) * 0.5f,
                                  pos.y + (cell - is) * 0.5f);
                        dl->AddImage((ImTextureID)(intptr_t)s_iconTex[(int)cat].id,
                                     ip, ImVec2(ip.x + is, ip.y + is),
                                     ImVec2(0, 0), ImVec2(1, 1),
                                     ImGui::GetColorU32(ImGuiCol_TextDisabled));
                    }
                }

                // Name label under the image, clipped to the cell width.
                ImVec2 tp(pos.x + 2.0f, pos.y + cell + 2.0f);
                ImVec4 clip(pos.x, tp.y, pos.x + cell, tp.y + labelH);
                dl->AddText(nullptr, 0.0f, tp, ImGui::GetColorU32(ImGuiCol_Text),
                            name.c_str(), nullptr, 0.0f, &clip);
                ImGui::PopID();
            }
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
    if (GridCategory(cat))
        DrawThumbnailGrid(app, cat, shown);
    else {
        // Non-object categories stay a compact name list.
        ImGuiListClipper clipper;
        clipper.Begin((int)shown.size());
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                int node = shown[i];
                const std::string& name = app.workspace.names[node].name;
                ImGui::PushID(node);
                if (ImGui::Selectable(name.c_str(), app.selectedNode == node,
                                      ImGuiSelectableFlags_AllowDoubleClick))
                    app.SelectObject(node); // scope the editors to its cluster (#365)
                ImGui::PopID();
            }
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
