#include "fx/mus.h"
#include "fx/pe.h"
#include <cstdio>
#include <utility>

namespace fx {

std::string mus_xmi_name(uint8_t index) {
    if (index == 1) return "VALK01.XMI";
    char buf[16];
    snprintf(buf, sizeof(buf), "AIR%03d.XMI", (int)index);
    return buf;
}

// The FC/FE state-machine dispatch table (MUS.md § Bytecode Script Format):
// the fixed run that follows those opcodes; skipped when present.
static bool dispatch_table_at(const uint8_t* p, const uint8_t* end) {
    static const uint8_t kTable[9] = {0x01, 0x02, 0x03, 0x02, 0x01,
                                      0x02, 0x03, 0x02, 0x01};
    if (p + 9 > end) return false;
    for (int i = 0; i < 9; i++)
        if (p[i] != kTable[i]) return false;
    return true;
}

MusScript mus_disassemble(const uint8_t* data, size_t size) {
    MusScript out;
    CodeSection cs = pe_code_section(data, size);
    if (!cs.data) return out;  // valid stays false
    out.valid = true;

    const uint8_t* p   = cs.data;
    const uint8_t* end = cs.data + cs.size;

    // The mode the last FB set. A later bare index reuses it (running status).
    uint8_t running_mode     = 0;
    bool    running_mode_set = false;

    while (p < end) {
        uint8_t op = *p;

        if (op == 0x00) break;            // zero padding at the tail
        if (op == 0xF9) { ++p; continue; } // section-end marker — skip

        MusOp e;
        e.offset = (uint32_t)(p - cs.data);
        e.op = op;

        if (op == 0xFF) {  // FF <name\0> — playlist identifier
            ++p;
            while (p < end && *p) e.playlist_id += (char)(*p++);
            if (p < end) ++p;  // consume NUL
            out.ops.push_back(std::move(e));
            continue;
        }
        if (op == 0xFA && p + 5 < end) {  // FA <sub> <u32>
            e.sub   = p[1];
            e.value = (uint32_t)p[2] | ((uint32_t)p[3] << 8) |
                      ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24);
            p += 6;
            out.ops.push_back(std::move(e));
            continue;
        }
        if (op == 0xFB && p + 2 < end) {  // FB <mode> <idx> [F9]
            e.mode      = p[1];
            e.track_idx = p[2];
            running_mode     = e.mode;
            running_mode_set = true;
            e.xmi       = mus_xmi_name(e.track_idx);
            p += (p + 3 < end && p[3] == 0xF9) ? 4 : 3;
            out.ops.push_back(std::move(e));
            continue;
        }
        if (op == 0xFC) {  // FC — shuffle/loop marker + dispatch table
            ++p;
            if (dispatch_table_at(p, end)) p += 9;
            out.ops.push_back(std::move(e));
            continue;
        }
        if (op == 0xFE && p + 4 < end) {  // FE <u32> — conditional branch
            e.value = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                      ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
            p += 5;
            if (dispatch_table_at(p, end)) p += 9;
            out.ops.push_back(std::move(e));
            continue;
        }
        if (op == 0xFD && p + 1 < end) {  // FD <n> <n track bytes> (#491)
            // NOT `FD <u24>`, which is what both this codec and MUS.md used to say. The
            // operand is a COUNT-PREFIXED LIST, and the real files show every length:
            //
            //   M_AIR    fd 02 07 1f          n=2
            //   M_EJECT  fd 03 0f 16 27       n=3
            //   M_SUCC   fd 05 03 0b 14 31 10 n=5
            //
            // Reading a fixed 3-byte operand consumed the count plus two items and then
            // landed mid-list, so M_EJECT and M_SUCC aborted on a "bad opcode" that was
            // really a track index. With the count honoured, every playlist disassembles
            // clean through to its dispatch table and zero padding.
            const uint8_t n = p[1];
            if (p + 2 + (size_t)n > end) { out.stopped_early = true; out.stop_byte = op; break; }
            e.tracks.assign(p + 2, p + 2 + n);
            p += 2 + (size_t)n;
            out.ops.push_back(std::move(e));
            continue;
        }
        if (op < 0xF9) {
            // A bare track index, under the mode the last FB set — MIDI-style running
            // status. M_AIR ends `fb 50 13 f9 | 17 f9 | 15 f9 | 06 | fe 11 …`: three more
            // tracks for mode 0x50 with the opcode elided. Without this the disassembly
            // stopped at `0x17` and lost the playlist's tail.
            if (!running_mode_set) { out.stopped_early = true; out.stop_byte = op; break; }
            e.op        = 0xFB;      // it IS a play-track instruction, just written short
            e.mode      = running_mode;
            e.track_idx = op;
            e.xmi       = mus_xmi_name(e.track_idx);
            p += (p + 1 < end && p[1] == 0xF9) ? 2 : 1;
            out.ops.push_back(std::move(e));
            continue;
        }

        // Unrecognized byte — record it and stop.
        out.stopped_early = true;
        out.stop_byte     = op;
        break;
    }

    return out;
}

} // namespace fx
