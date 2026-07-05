// Fuzz target: the MZ/LE ("PL\0\0") section reader shared by the LAY, FNT,
// and MUS overlay parsers, plus VA→offset translation against the section
// it returns. The first and last section bytes are read through a volatile
// sink so ASan sees any out-of-bounds section pointer.

#include <cstddef>
#include <cstdint>

#include <fx/pe.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const fx::CodeSection cs = fx::pe_code_section(data, size);
    if (!cs.data) return 0;
    volatile uint8_t sink = 0;
    if (cs.size) {
        sink ^= cs.data[0];
        sink ^= cs.data[cs.size - 1];
    }
    (void)sink;
    fx::pe_va_to_offset(cs, cs.vma);
    fx::pe_va_to_offset(cs, cs.vma + (uint32_t)cs.size);
    fx::pe_va_to_offset(cs, 0);
    fx::pe_va_to_offset(cs, 0xFFFFFFFFu);
    return 0;
}
