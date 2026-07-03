#include "lib_browser.h"
#include "../app.h"
#include "../util.h"
#include "imgui.h"
#include <filesystem>
#include <string>
#include <cstring>

namespace fs = std::filesystem;

using fxg::ci_contains;

static const char* EntryTypeLabel(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot) return "DAT";
    const char* e = dot + 1;
    // Entity / BRF types
    if (fxg::ci_equal(e,"PT"))  return "Aircraft";
    if (fxg::ci_equal(e,"OT"))  return "Object";
    if (fxg::ci_equal(e,"NT"))  return "NPC";
    if (fxg::ci_equal(e,"JT"))  return "Ordnance";
    if (fxg::ci_equal(e,"SEE")) return "Seeker";
    if (fxg::ci_equal(e,"ECM")) return "ECM";
    if (fxg::ci_equal(e,"GAS")) return "Gas";
    // Image / pixel data
    if (fxg::ci_equal(e,"PIC")) return "Image";
    if (fxg::ci_equal(e,"PAL")) return "Palette";
    if (fxg::ci_equal(e,"RAW")) return "Screenshot";
    if (fxg::ci_equal(e,"ICO")) return "Icon";
    // 3D geometry
    if (fxg::ci_equal(e,"SH"))  return "Shape";
    if (fxg::ci_equal(e,"T2"))  return "Terrain";
    // Audio
    if (fxg::ci_equal(e,"11K")||fxg::ci_equal(e,"5K")||
        fxg::ci_equal(e,"8K")||fxg::ci_equal(e,"22K")) return "Audio";
    if (fxg::ci_equal(e,"XMI")) return "MIDI";
    // Video / FMV
    if (fxg::ci_equal(e,"CB8")) return "FMV";
    if (fxg::ci_equal(e,"VDO")) return "Video";
    if (fxg::ci_equal(e,"FBC")) return "FrameIdx";
    // Animation
    if (fxg::ci_equal(e,"SEQ")) return "Sequence";
    // Mission data
    if (fxg::ci_equal(e,"M"))   return "Mission";
    if (fxg::ci_equal(e,"MM"))  return "Map";
    if (fxg::ci_equal(e,"MT"))  return "Briefing";
    // Entity metadata
    if (fxg::ci_equal(e,"INF")) return "TechInfo";
    // AI / behaviour scripts
    if (fxg::ci_equal(e,"AI"))  return "AI Script";
    if (fxg::ci_equal(e,"BI"))  return "AI Lib";
    // Overlay DLLs (Phar Lap PE)
    if (fxg::ci_equal(e,"HUD")) return "HUD";
    if (fxg::ci_equal(e,"FNT")) return "Font";
    if (fxg::ci_equal(e,"LAY")) return "Atmosphere";
    if (fxg::ci_equal(e,"CAM")) return "Campaign";
    if (fxg::ci_equal(e,"MC"))  return "Condition";
    if (fxg::ci_equal(e,"MNU")) return "Menu";
    if (fxg::ci_equal(e,"DLG")) return "Dialog";
    if (fxg::ci_equal(e,"HGR")) return "Hangar";
    if (fxg::ci_equal(e,"PTS")) return "Icon Data";
    if (fxg::ci_equal(e,"MUS")) return "Music";
    // Pilot save
    if (fxg::ci_equal(e,"P"))   return "Pilot";
    // Binary lookup / symbol tables
    if (fxg::ci_equal(e,"BIN")) return "Binary";
    if (fxg::ci_equal(e,"SMS")) return "Symbols";
    // Text / help / config
    if (fxg::ci_equal(e,"TXT")) return "Text";
    if (fxg::ci_equal(e,"WRI")) return "Document";
    if (fxg::ci_equal(e,"HLP")) return "Help";
    if (fxg::ci_equal(e,"CNT")) return "Help TOC";
    if (fxg::ci_equal(e,"INI")) return "Config";
    // Executable
    if (fxg::ci_equal(e,"EXE")) return "Executable";
    return "File";
}

static ImVec4 TypeColor(const char* label) {
    // Blue — player aircraft
    if (strcmp(label,"Aircraft")==0) return {0.4f,0.8f,1.0f,1};
    // Steel blue — other entity types (objects, NPCs)
    if (strcmp(label,"Object")==0||strcmp(label,"NPC")==0)
                                     return {0.55f,0.70f,0.90f,1};
    // Orange — weapons / seekers / countermeasures
    if (strcmp(label,"Ordnance")==0||strcmp(label,"Seeker")==0||
        strcmp(label,"ECM")==0||strcmp(label,"Gas")==0)
                                     return {1.0f,0.6f,0.3f,1};
    // Green — pixel data and image-derived content
    if (strcmp(label,"Image")==0||strcmp(label,"Palette")==0||
        strcmp(label,"Screenshot")==0||strcmp(label,"Icon")==0||
        strcmp(label,"Hangar")==0||strcmp(label,"Sequence")==0)
                                     return {0.6f,1.0f,0.6f,1};
    // Rose — 3D geometry
    if (strcmp(label,"Shape")==0)    return {1.0f,0.7f,0.7f,1};
    // Olive — world / environment
    if (strcmp(label,"Terrain")==0||strcmp(label,"Atmosphere")==0)
                                     return {0.7f,0.9f,0.5f,1};
    // Yellow — audio
    if (strcmp(label,"Audio")==0||strcmp(label,"MIDI")==0||
        strcmp(label,"Music")==0)    return {1.0f,0.9f,0.4f,1};
    // Purple — mission / campaign / map
    if (strcmp(label,"Mission")==0||strcmp(label,"Map")==0||
        strcmp(label,"Briefing")==0||strcmp(label,"Campaign")==0)
                                     return {0.9f,0.6f,1.0f,1};
    // Cyan — overlay DLLs (UI/cockpit/menu/condition)
    if (strcmp(label,"HUD")==0||strcmp(label,"Font")==0||
        strcmp(label,"Menu")==0||strcmp(label,"Dialog")==0||
        strcmp(label,"Icon Data")==0||strcmp(label,"Condition")==0)
                                     return {0.5f,0.85f,1.0f,1};
    // Amber — AI behaviour
    if (strcmp(label,"AI Script")==0||strcmp(label,"AI Lib")==0)
                                     return {1.0f,0.8f,0.5f,1};
    // Gold — tech info / encyclopedia entries
    if (strcmp(label,"TechInfo")==0) return {1.0f,0.88f,0.55f,1};
    // Lavender — pilot saves
    if (strcmp(label,"Pilot")==0)    return {0.8f,0.7f,1.0f,1};
    // Dim grey — video (FBC is index companion to VDO)
    if (strcmp(label,"FMV")==0||strcmp(label,"Video")==0||
        strcmp(label,"FrameIdx")==0) return {0.7f,0.7f,0.7f,1};
    // Tan — binary data tables / symbol maps
    if (strcmp(label,"Binary")==0||strcmp(label,"Symbols")==0)
                                     return {0.82f,0.72f,0.58f,1};
    // Warm off-white — text / help / documentation / config
    if (strcmp(label,"Text")==0||strcmp(label,"Document")==0||
        strcmp(label,"Help")==0||strcmp(label,"Help TOC")==0||
        strcmp(label,"Config")==0)   return {0.85f,0.82f,0.78f,1};
    // Default — neutral grey (File, DAT, Executable, unknown)
    return {0.75f,0.75f,0.75f,1};
}

static constexpr float kMinTableH = 60.0f;
static constexpr float kInitMaxH  = 300.0f;
static constexpr float kHandleH   = 4.0f;

void DrawLibBrowser(App& app) {
    if (app.sessions.empty()) {
        ImGui::TextDisabled("No files open.");
        return;
    }

    static char filter[64] = {};
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "Filter...", filter, sizeof(filter));
    ImGui::Separator();

    for (int si = 0; si < (int)app.sessions.size(); si++) {
        auto& sess = app.sessions[si];

        float rowH   = ImGui::GetTextLineHeightWithSpacing();
        float naturalH = (float)(sess.entries.size() + 1) * rowH + 8.0f;
        if (naturalH < kMinTableH) naturalH = kMinTableH;

        // Initialise height on first appearance.
        if (sess.tableHeight <= 0.0f) {
            sess.tableHeight = naturalH < kInitMaxH ? naturalH : kInitMaxH;
        }

        std::string libName = fs::path(sess.path).filename().string();
        if (sess.dirty) libName += " *";
        std::string nodeId = libName + "##lib" + std::to_string(si);

        bool isSelected = (app.selectedSession == si);
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isSelected) nodeFlags |= ImGuiTreeNodeFlags_Selected;

        if (sess.forceOpen >= 0) {
            ImGui::SetNextItemOpen(sess.forceOpen == 1, ImGuiCond_Always);
            sess.forceOpen = -1;
        }
        bool open = ImGui::TreeNodeEx(nodeId.c_str(), nodeFlags);

        // Select session on click.
        if (ImGui::IsItemClicked())
            app.selectedSession = si;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%zu entries", sess.path.c_str(),
                              sess.entries.size());

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Close")) {
                ImGui::EndPopup();
                if (open) ImGui::TreePop();
                app.CloseSession(si);
                return;
            }
            if (!sess.standalone)
                if (ImGui::MenuItem("Install as FA_0.LIB",
                                    nullptr, false, !app.installDir.empty()))
                    app.InstallToGame(si);
            ImGui::EndPopup();
        }

        if (!open) continue;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,2));
        // Unique table ID per session prevents ImGui from sharing scroll state
        // across different sessions when they are collapsed and re-expanded.
        std::string tableId = "##entries_" + std::to_string(si);
        if (ImGui::BeginTable(tableId.c_str(), 3,
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_RowBg   |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable,
                ImVec2(0, sess.tableHeight))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthFixed, 72);
            ImGui::TableSetupColumn("Size",  ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();

            for (int ei = 0; ei < (int)sess.entries.size(); ei++) {
                const auto& e = sess.entries[ei];
                const char* label = EntryTypeLabel(e.name);
                if (filter[0] != '\0') {
                    if (!ci_contains(e.name, filter) &&
                        !ci_contains(label, filter))
                        continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool selected = (app.editor.libIdx == si &&
                                 app.editor.entryIdx == ei);
                ImGui::PushID(ei);
                if (ImGui::Selectable(e.name, selected,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    app.OpenEntry(si, ei);
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(TypeColor(label), "%s", label);

                ImGui::TableSetColumnIndex(2);
                if (e.size < 1024)
                    ImGui::Text("%u B", e.size);
                else
                    ImGui::Text("%.1f K", e.size / 1024.0f);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        // Resize handle — drag to set height; double-click to fit content.
        ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Separator));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
        ImGui::Button(("##hr" + std::to_string(si)).c_str(), ImVec2(-1.0f, kHandleH));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemActive()) {
            sess.tableHeight += ImGui::GetIO().MouseDelta.y;
            // Only enforce min; let the user make the table as tall as they like.
            if (sess.tableHeight < kMinTableH) sess.tableHeight = kMinTableH;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            sess.tableHeight = naturalH; // fit to content on double-click
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        ImGui::TreePop();
    }

    if (ImGui::BeginPopupContextWindow("##BrowserCtx",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        bool any = !app.sessions.empty();
        if (ImGui::MenuItem("Expand All", nullptr, false, any))
            for (auto& s : app.sessions) s.forceOpen = 1;
        if (ImGui::MenuItem("Collapse All", nullptr, false, any))
            for (auto& s : app.sessions) s.forceOpen = 0;
        ImGui::EndPopup();
    }
}
