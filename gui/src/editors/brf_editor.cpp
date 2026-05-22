#include "brf_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/brf.h"
#include "fx/ot.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

// Per-field edit buffer. Rebuilt when the selected entry changes.
struct FieldBuf {
    char buf[128];
    bool changed;
};
static std::vector<FieldBuf> s_bufs;
static int s_lastLib   = -2;
static int s_lastEntry = -2;
static fx::BrfDoc s_doc;

static void RebuildBuffers(const fx::BrfDoc& doc) {
    s_bufs.resize(doc.fields.size());
    for (size_t i = 0; i < doc.fields.size(); i++) {
        strncpy_s(s_bufs[i].buf, doc.fields[i].value.c_str(), 127);
        s_bufs[i].changed = false;
    }
}

// Return (name, note) for field at index fi using the compound schema for ext.
// SEE/ECM/GAS are standalone (no OT_GENERAL prefix).
// OT/NT/PT/JT all begin with OT_GENERAL_FIELDS, then chain extension sections.
static std::pair<const char*, const char*> FieldLabel(const std::string& ext, int fi) {
    // Flat standalone schemas
    if (ext == "see") {
        if (fi < fx::SEE_COUNT) return { fx::SEE_FIELDS[fi].name, fx::SEE_FIELDS[fi].note };
        return { nullptr, nullptr };
    }
    if (ext == "ecm") {
        if (fi < fx::ECM_COUNT) return { fx::ECM_FIELDS[fi].name, fx::ECM_FIELDS[fi].note };
        return { nullptr, nullptr };
    }
    if (ext == "gas") {
        if (fi < fx::GAS_COUNT) return { fx::GAS_FIELDS[fi].name, fx::GAS_FIELDS[fi].note };
        return { nullptr, nullptr };
    }

    // OT / NT / PT / JT â€” all begin with the OT_GENERAL section
    if (fi < fx::OT_GENERAL_COUNT)
        return { fx::OT_GENERAL_FIELDS[fi].name, fx::OT_GENERAL_FIELDS[fi].note };

    int off = fi - fx::OT_GENERAL_COUNT;

    if (ext == "jt") {
        if (off < fx::JT_COUNT) return { fx::JT_FIELDS[off].name, fx::JT_FIELDS[off].note };
        return { nullptr, nullptr };
    }
    if (ext == "nt" || ext == "pt") {
        if (off < fx::NT_COUNT) return { fx::NT_FIELDS[off].name, fx::NT_FIELDS[off].note };
        off -= fx::NT_COUNT;
        if (ext == "pt" && off < fx::PT_COUNT)
            return { fx::PT_FIELDS[off].name, fx::PT_FIELDS[off].note };
        return { nullptr, nullptr };
    }
    // OT â€” only OT_GENERAL (already covered above)
    return { nullptr, nullptr };
}

void DrawBrfEditor(App& app) {
    auto& ed = app.editor;

    // Reload when selection changes
    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_doc = fx::brf_parse(ed.data.data(), ed.data.size());
        RebuildBuffers(s_doc);
    }

    if (s_doc.fields.empty()) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Parse error or empty BRF.");
        return;
    }

    bool anyChanged = false;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,3));
    if (ImGui::BeginTable("##brf", 3,
            ImGuiTableFlags_ScrollY   |
            ImGuiTableFlags_RowBg     |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Field",  ImGuiTableColumnFlags_WidthFixed,   160);
        ImGui::TableSetupColumn("Value",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Note",   ImGuiTableColumnFlags_WidthFixed,   200);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)s_doc.fields.size(); i++) {
            const auto& f = s_doc.fields[i];
            auto [fieldName, note] = FieldLabel(ed.ext, i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            // Show type prefix in dim color, then name if known
            ImGui::TextDisabled("%s", f.type.c_str());
            if (fieldName && fieldName[0]) {
                ImGui::SameLine();
                ImGui::Text("%s", fieldName);
            } else {
                ImGui::SameLine();
                ImGui::TextDisabled("[%d]", i);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(-1);
            bool edited = ImGui::InputText("##v", s_bufs[i].buf,
                                           sizeof(s_bufs[i].buf));
            if (edited) { s_bufs[i].changed = true; anyChanged = true; }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            if (note && note[0])
                ImGui::TextDisabled("%s", note);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    // Pointer tables (string lists)
    if (!s_doc.tables.empty()) {
        ImGui::Separator();
        ImGui::Text("String tables");
        for (auto& tbl : s_doc.tables) {
            if (ImGui::TreeNode(tbl.name.c_str())) {
                for (int si = 0; si < (int)tbl.strings.size(); si++) {
                    ImGui::BulletText("%s", tbl.strings[si].c_str());
                }
                ImGui::TreePop();
            }
        }
    }

    // Apply changes back into the doc's raw_lines so brf_serialize round-trips cleanly.
    if (anyChanged) {
        for (int i = 0; i < (int)s_doc.fields.size(); i++) {
            if (s_bufs[i].changed) {
                s_doc.fields[i].value = s_bufs[i].buf;
                s_bufs[i].changed = false;
            }
        }
        // Sync raw_lines: find data field lines and patch them.
        int fi = 0;
        for (auto& line : s_doc.raw_lines) {
            if (line.empty() || line[0] == ';' || line[0] == '[' ||
                line[0] == ':') continue;
            // Lines starting with whitespace are data fields
            if (line[0] == '\t' || line[0] == ' ') {
                if (fi < (int)s_doc.fields.size()) {
                    // Rebuild the line preserving leading whitespace and type token.
                    auto ws = line.find_first_not_of(" \t");
                    std::string indent = (ws != std::string::npos)
                                         ? line.substr(0, ws) : "\t";
                    line = indent + s_doc.fields[fi].type +
                           "\t" + s_doc.fields[fi].value;
                    fi++;
                }
            }
        }
        ed.data     = fx::brf_serialize(s_doc);
        ed.modified = true;
    }
}
