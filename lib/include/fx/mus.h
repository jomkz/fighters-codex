#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// MUS — Music Playlist Sequencer (.MUS). Each .MUS is a Win32 PE DLL whose
// CODE section holds the playlist bytecode; this disassembles that bytecode
// (docs/fa/formats/MUS.md § Bytecode Script Format). The FA/FB sub-opcode
// semantics are Miles-internal and untraced (#54); read-only (see #101).

namespace fx {

// One decoded sequencer instruction. `op` is the opcode byte; which fields
// carry meaning depends on it:
//   0xFF  playlist id  -> playlist_id
//   0xFA  setup/config -> sub, value (u32)
//   0xFB  play track   -> mode, track_idx, xmi (resolved filename)
//   0xFC  shuffle/loop  -> (a state-dispatch table is consumed; no operands)
//   0xFD  track list    -> tracks (a COUNT-prefixed list: FD <n> <n bytes>)
//   0xFE  conditional   -> value (32-bit game-state key)
struct MusOp {
    uint32_t    offset    = 0;   // byte offset of the opcode within the CODE section
    uint8_t     op        = 0;
    uint8_t     sub       = 0;   // FA
    uint8_t     mode      = 0;   // FB (and the running mode of a bare index — see mus.cpp)
    uint8_t     track_idx = 0;   // FB
    uint32_t    value     = 0;   // FA value / FE 32-bit key
    std::vector<uint8_t> tracks; // FD — the n track indices the count introduces
    std::string playlist_id;     // FF
    std::string xmi;             // FB — resolved XMI filename for track_idx
};

struct MusScript {
    bool valid = false;             // a PE CODE section was found
    std::vector<MusOp> ops;
    bool stopped_early = false;     // hit an unrecognized opcode byte
    uint8_t stop_byte = 0;          // the byte that stopped disassembly
};

// Map an XMI track index to its filename: index 1 -> "VALK01.XMI", otherwise
// "AIRnnn.XMI" (zero-padded). The index space is sparse; this names the slot
// whether or not the file exists (MUS.md § XMI Track Index Mapping).
std::string mus_xmi_name(uint8_t index);

// Disassemble the playlist bytecode from a .MUS DLL's CODE section. Returns
// MusScript{valid = false} when the file has no CODE section.
MusScript mus_disassemble(const uint8_t* data, size_t size);

} // namespace fx
