#include "brf_editor.h"
#include "../app.h"
#include "../util.h"
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
        fxg::copy_str(s_bufs[i].buf, sizeof(s_bufs[i].buf), doc.fields[i].value);
        s_bufs[i].changed = false;
    }
}

// Return (name, note) for a field.
//
// The positional OT/NT/PT/JT chain that used to live here indexed tables transcribed from
// OpenFA (GPL) whose field order is not FA's -- so it mislabelled real files. It is gone.
//
// Names now come from the same two honest sources the library uses: the field's SECTION and
// BYTE OFFSET, which the self-describing BRF record declares itself, looked up against what
// this project actually recovered from the executable. An unrecovered field gets no name.
static std::pair<const char*, const char*> FieldLabel(const std::string& ext,
                                                      const fx::BrfField& f) {
    // SEE / ECM / GAS are flat records whose schemas came from this project's own specs.
    if (ext == "see") return { nullptr, nullptr };  // handled positionally below
    if (ext == "ecm") return { nullptr, nullptr };
    if (ext == "gas") return { nullptr, nullptr };
    return { fx::brf_field_name(f.section, f.offset), fx::brf_field_note(f.section, f.offset) };
}

static std::pair<const char*, const char*> FlatLabel(const std::string& ext, int fi) {
    if (ext == "see" && fi < fx::SEE_COUNT) return { fx::SEE_FIELDS[fi].name, fx::SEE_FIELDS[fi].note };
    if (ext == "ecm" && fi < fx::ECM_COUNT) return { fx::ECM_FIELDS[fi].name, fx::ECM_FIELDS[fi].note };
    if (ext == "gas" && fi < fx::GAS_COUNT) return { fx::GAS_FIELDS[fi].name, fx::GAS_FIELDS[fi].note };
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
            bool flat = (ed.ext == "see" || ed.ext == "ecm" || ed.ext == "gas");
            auto [fieldName, note] = flat ? FlatLabel(ed.ext, i)
                                          : FieldLabel(ed.ext, s_doc.fields[i]);

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

    // Labelled blocks (string tables, and the numeric ones: :hards, :env)
    if (!s_doc.blocks.empty()) {
        ImGui::Separator();
        ImGui::Text("Blocks");
        for (auto& tbl : s_doc.blocks) {
            if (ImGui::TreeNode(tbl.name.c_str())) {
                for (int si = 0; si < (int)tbl.strings.size(); si++) {
                    ImGui::BulletText("%s", tbl.strings[si].c_str());
                }
                ImGui::TreePop();
            }
        }
    }

    // Apply changes back into the doc's raw_lines so brf_serialize round-trips cleanly.
    //
    // This used to walk every indented line and rewrite it as `indent + type + '\t' + value`,
    // pairing lines with fields BY POSITION. That rewrote lines nobody had touched -- turning
    // each `    byte 1` into `    byte\t1` and dropping the file's own inline comments (the
    // `; utilProc` that NAMES the class proc) -- so a single edit stopped the file being
    // byte-identical. And the pairing only held because the fields the parser dropped
    // happened to be a suffix; it would have written values onto the wrong lines the moment
    // that stopped being true. Each field now knows exactly where its value token sits, so an
    // edit is a splice and nothing else on the line moves.
    if (anyChanged) {
        for (int i = 0; i < (int)s_doc.fields.size(); i++) {
            if (s_bufs[i].changed) {
                fx::brf_set_value(s_doc, s_doc.fields[i], s_bufs[i].buf);
                s_bufs[i].changed = false;
            }
        }
        ed.data     = fx::brf_serialize(s_doc);
        ed.modified = true;
    }
}
