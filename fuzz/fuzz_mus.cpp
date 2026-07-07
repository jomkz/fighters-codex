// Fuzz target: the MUS music-playlist disassembler (mus_disassemble). It walks
// the MUS opcode stream (FF playlist / FA setup / FB play-track / FC shuffle /
// FD jump / FE branch) over untrusted bytes; the cross-TU call can't be elided.

#include <cstddef>
#include <cstdint>

#include <fx/mus.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::MusScript script = fx::mus_disassemble(data, size);
    (void)script;
    return 0;
}
