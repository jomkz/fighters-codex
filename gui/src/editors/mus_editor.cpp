#include "mus_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/pe.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// MUS bytecode opcodes (from RE of MUSIC DLLs)
// Each opcode is a single byte followed by a fixed number of operand bytes.
static const char* OpMnemonic(uint8_t op) {
    switch (op) {
    case 0xFF: return "END";
    case 0xFA: return "PLAY_NOTE";
    case 0xFB: return "SET_TEMPO";
    case 0xFC: return "JUMP";
    case 0xFD: return "LOOP";
    case 0xFE: return "XMI_REF";
    default:   return nullptr;
    }
}
static int OpOperandBytes(uint8_t op) {
    switch (op) {
    case 0xFF: return 0;   // END â€” no operands
    case 0xFA: return 3;   // PLAY_NOTE  note vel dur(u8)
    case 0xFB: return 2;   // SET_TEMPO  bpm(u16)
    case 0xFC: return 4;   // JUMP       offset(i32)
    case 0xFD: return 2;   // LOOP       count(u16)
    case 0xFE: return 4;   // XMI_REF    va(u32) -> null-terminated filename in CODE
    default:   return -1;  // unknown â€” stop disassembly
    }
}

struct MusInsn {
    uint32_t    offset;
    uint8_t     opcode;
    std::string mnemonic;
    std::string operands;
};

static std::vector<MusInsn> s_insns;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

static void Disassemble(const uint8_t* data, size_t size) {
    s_insns.clear();

    fx::CodeSection cs = fx::pe_code_section(data, size);
    if (!cs.data || cs.size == 0) {
        // Not a PE â€” try walking raw bytes for simple embedded bytecode.
        cs.data = data;
        cs.size = size;
        cs.vma  = 0;
    }

    const uint8_t* p   = cs.data;
    const uint8_t* end = cs.data + cs.size;

    // Heuristic: scan forward to find the first known opcode byte.
    // MUS code section starts with the entry-point â€” skip any header bytes
    // by looking for the first 0xFA/0xFF/0xFB/0xFC/0xFD/0xFE.
    while (p < end) {
        uint8_t b = *p;
        if (OpMnemonic(b)) break;
        p++;
    }

    while (p < end) {
        uint8_t  op  = *p;
        uint32_t off = (uint32_t)(p - cs.data);
        const char* mn = OpMnemonic(op);
        if (!mn) break;  // unknown opcode â€” stop

        int opsz = OpOperandBytes(op);
        if (opsz < 0) break;
        if (p + 1 + opsz > end) break;

        MusInsn insn;
        insn.offset   = off;
        insn.opcode   = op;
        insn.mnemonic = mn;

        char buf[256] = {};
        switch (op) {
        case 0xFF: // END
            break;
        case 0xFA: { // PLAY_NOTE note vel dur
            uint8_t note = p[1], vel = p[2], dur = p[3];
            snprintf(buf, sizeof(buf), "note=%d  vel=%d  dur=%d", note, vel, dur);
            break;
        }
        case 0xFB: { // SET_TEMPO bpm (little-endian u16)
            uint16_t bpm = (uint16_t)(p[1] | (p[2] << 8));
            snprintf(buf, sizeof(buf), "bpm=%u", bpm);
            break;
        }
        case 0xFC: { // JUMP offset (little-endian i32)
            int32_t joff = (int32_t)(p[1] | (p[2]<<8) | (p[3]<<16) | (p[4]<<24));
            snprintf(buf, sizeof(buf), "offset=%+d (-> %u)", joff, (uint32_t)(off + 5 + joff));
            break;
        }
        case 0xFD: { // LOOP count (little-endian u16)
            uint16_t cnt = (uint16_t)(p[1] | (p[2] << 8));
            snprintf(buf, sizeof(buf), "count=%u", cnt);
            break;
        }
        case 0xFE: { // XMI_REF va (little-endian u32)
            uint32_t va = (uint32_t)(p[1] | (p[2]<<8) | (p[3]<<16) | (p[4]<<24));
            size_t strOff = fx::pe_va_to_offset(cs, va);
            if (strOff != (size_t)-1 && strOff < cs.size) {
                // Read null-terminated string
                const char* s = (const char*)cs.data + strOff;
                size_t maxLen = cs.size - strOff;
                snprintf(buf, sizeof(buf), "\"%.*s\"", (int)(maxLen < 60 ? maxLen : 60), s);
            } else {
                snprintf(buf, sizeof(buf), "va=$%08X", va);
            }
            break;
        }
        }
        insn.operands = buf;
        s_insns.push_back(insn);

        p += 1 + opsz;
        if (op == 0xFF) break; // END
    }
}

void DrawMusEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        Disassemble(ed.data.data(), ed.data.size());
    }

    ImGui::TextDisabled("%d instruction(s)  |  %zu bytes", (int)s_insns.size(), ed.data.size());
    ImGui::Separator();

    if (s_insns.empty()) {
        ImGui::TextColored({1,0.6f,0.3f,1}, "No MUS bytecode found.");
        ImGui::TextDisabled("(PE DLL with unrecognised entry point, or format variant)");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4,2));
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

        for (const auto& ins : s_insns) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%04X", ins.offset);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%02X", ins.opcode);
            ImGui::TableSetColumnIndex(2);
            // Colour by mnemonic
            ImVec4 col = {0.85f,0.82f,0.78f,1};
            if (ins.mnemonic == "PLAY_NOTE")  col = {1.0f,0.9f,0.4f,1};
            else if (ins.mnemonic == "SET_TEMPO") col = {0.5f,0.85f,1.0f,1};
            else if (ins.mnemonic == "JUMP")      col = {1.0f,0.6f,0.3f,1};
            else if (ins.mnemonic == "LOOP")      col = {0.9f,0.6f,1.0f,1};
            else if (ins.mnemonic == "XMI_REF")   col = {0.6f,1.0f,0.6f,1};
            else if (ins.mnemonic == "END")        col = {0.75f,0.75f,0.75f,1};
            ImGui::TextColored(col, "%s", ins.mnemonic.c_str());
            ImGui::TableSetColumnIndex(3);
            if (!ins.operands.empty())
                ImGui::TextUnformatted(ins.operands.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}
