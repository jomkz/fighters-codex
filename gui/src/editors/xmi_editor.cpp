#include "xmi_editor.h"
#include "../app.h"
#include "imgui.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>

// XMI = IFF-based Miles Sound System Extended MIDI.
// Top-level: FORM <size> XDIR
//   INFO chunk: uint16 seq_count, uint16 timbre_count
//   FORM <size> XMID — one per sequence

struct XmiSeq {
    uint32_t offset;
    uint32_t size;
};

struct XmiMeta {
    bool     valid       = false;
    uint16_t seq_count   = 0;
    uint16_t timbre_count= 0;
    std::vector<XmiSeq> sequences;
};

static uint32_t RBE32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static uint16_t RBE16(const uint8_t* p) {
    return (uint16_t)((p[0]<<8)|p[1]);
}

static XmiMeta ParseXmi(const uint8_t* data, size_t size) {
    XmiMeta m;
    if (size < 12) return m;
    if (memcmp(data, "FORM", 4) != 0) return m;
    if (memcmp(data + 8, "XDIR", 4) != 0) return m;

    const uint8_t* p   = data + 12;
    const uint8_t* end = data + size;

    while (p + 8 <= end) {
        char tag[5] = {};
        memcpy(tag, p, 4);
        uint32_t csize = RBE32(p + 4);
        p += 8;
        if (p + csize > end) break;

        if (strcmp(tag, "INFO") == 0 && csize >= 4) {
            m.seq_count    = RBE16(p);
            m.timbre_count = RBE16(p + 2);
        } else if (strcmp(tag, "FORM") == 0 && csize >= 4 &&
                   memcmp(p, "XMID", 4) == 0) {
            XmiSeq s;
            s.offset = (uint32_t)(p - data);
            s.size   = csize;
            m.sequences.push_back(s);
        }
        // IFF chunks are padded to even size
        p += csize + (csize & 1);
    }

    m.valid = (m.seq_count > 0 || !m.sequences.empty());
    return m;
}

static XmiMeta s_xmi;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

void DrawXmiEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_xmi = ParseXmi(ed.data.data(), ed.data.size());
    }

    if (!s_xmi.valid) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "Cannot parse XMI header.");
        ImGui::TextDisabled("Expected IFF FORM/XDIR structure.");
        return;
    }

    ImGui::TextDisabled("XMI Extended MIDI  |  %u sequence(s)  |  %u timbre(s)  |  %zu bytes",
                        s_xmi.seq_count, s_xmi.timbre_count, ed.data.size());
    ImGui::TextDisabled("Playback not yet supported — structure view only.");
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 2));
    if (ImGui::BeginTable("##xmiseqs", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Seq", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Size",   ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)s_xmi.sequences.size(); i++) {
            const auto& seq = s_xmi.sequences[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i);
            ImGui::TableSetColumnIndex(1); ImGui::Text("0x%04X", seq.offset);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%u B", seq.size);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
