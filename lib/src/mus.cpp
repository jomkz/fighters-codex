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
        if (op == 0xFD && p + 3 < end) {  // FD <u24> — loop / jump
            e.value = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                      ((uint32_t)p[3] << 16);
            p += 4;
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
