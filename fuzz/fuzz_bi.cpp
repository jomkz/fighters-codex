// Fuzz target: the BI compiled-AI-script disassembler (fx bi dump/decompile
// over untrusted bytes). bi_disasm walks the opcode stream emitting one record
// per instruction; a truncated/hostile stream is the surface under test.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fx/bi.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<fx::BiInstr> instrs = fx::bi_disasm(data, size);
    volatile uint32_t sink = 0;
    for (const auto& in : instrs) sink ^= in.offset ^ (uint32_t)in.text.size();
    (void)sink;
    return 0;
}
