#include "mus_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/mus.h"
#include <cstdio>
#include <string>

// Mnemonic + operand rendering for a decoded MUS op. The disassembly itself
// lives in fx_lib (fx::mus_disassemble); this is presentation only.
static const char* MusMnemonic(uint8_t op) {
    switch (op) {
    case 0xFF: return "PLAYLIST";  // playlist identifier string
    case 0xFA: return "SETUP";     // setup/config
    case 0xFB: return "PLAY";      // play XMI track
    case 0xFC: return "SHUFFLE";   // shuffle/loop marker
    case 0xFD: return "JUMP";      // loop / jump
    case 0xFE: return "BRANCH";    // conditional branch
    default:   return "??";
    }
}

static std::string MusOperands(const fx::MusOp& e) {
    char buf[128] = {};
    switch (e.op) {
    case 0xFF: snprintf(buf, sizeof(buf), "id=\"%s\"", e.playlist_id.c_str()); break;
    case 0xFA: snprintf(buf, sizeof(buf), "sub=0x%02X  value=%u", e.sub, e.value); break;
    case 0xFB: snprintf(buf, sizeof(buf), "mode=0x%02X  track=%d  (%s)",
                        e.mode, (int)e.track_idx, e.xmi.c_str()); break;
    case 0xFC: break;
    case 0xFD: snprintf(buf, sizeof(buf), "target=0x%06X", e.value); break;
    case 0xFE: snprintf(buf, sizeof(buf), "state=0x%08X", e.value); break;
    default:   break;
    }
    return buf;
}

static fx::MusScript s_script;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

void DrawMusEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_script    = fx::mus_disassemble(ed.data.data(), ed.data.size());
    }

    ImGui::TextDisabled("%d instruction(s)  |  %zu bytes",
                        (int)s_script.ops.size(), ed.data.size());
    ImGui::Separator();

    if (!s_script.valid || s_script.ops.empty()) {
        ImGui::TextColored({1, 0.6f, 0.3f, 1}, "No MUS bytecode found.");
        ImGui::TextDisabled("(PE DLL with no CODE section, or unrecognised entry point)");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 2));
    if (ImGui::BeginTable("##mus", 4,
            ImGuiTableFlags_ScrollY       |
            ImGuiTableFlags_RowBg         |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable,
            ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Offset",   ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Opcode",   ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Operands", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& e : s_script.ops) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%04X", e.offset);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%02X", e.op);
            ImGui::TableSetColumnIndex(2);
            const char* mn = MusMnemonic(e.op);
            ImVec4 col = {0.85f, 0.82f, 0.78f, 1};
            switch (e.op) {
            case 0xFF: col = {0.75f, 0.75f, 0.75f, 1}; break;  // PLAYLIST
            case 0xFA: col = {0.5f,  0.85f, 1.0f,  1}; break;  // SETUP
            case 0xFB: col = {0.6f,  1.0f,  0.6f,  1}; break;  // PLAY
            case 0xFC: col = {0.9f,  0.6f,  1.0f,  1}; break;  // SHUFFLE
            case 0xFD: col = {1.0f,  0.6f,  0.3f,  1}; break;  // JUMP
            case 0xFE: col = {1.0f,  0.9f,  0.4f,  1}; break;  // BRANCH
            }
            ImGui::TextColored(col, "%s", mn);
            ImGui::TableSetColumnIndex(3);
            std::string ops = MusOperands(e);
            if (!ops.empty()) ImGui::TextUnformatted(ops.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    if (s_script.stopped_early)
        ImGui::TextDisabled("stopped at unrecognised byte 0x%02X", s_script.stop_byte);
}
