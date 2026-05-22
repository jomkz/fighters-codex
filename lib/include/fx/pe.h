#pragma once
#include <cstddef>
#include <cstdint>

// Minimal PE/LE section reader shared by FA overlay parsers.
// FA DLLs use Phar Lap LE format ("PL\0\0" signature) with one CODE section.

namespace fx {

struct CodeSection {
    const uint8_t* data;  // pointer into the raw file buffer; null on failure
    size_t         size;
    uint32_t       vma;   // virtual base address of this section (0x1000 for all FA overlays)
};

// Returns the first raw section from an MZ/PE or MZ/LE binary.
// Needed by LAY, FNT, and MUS parsers that resolve VA pointers within the section.
CodeSection pe_code_section(const uint8_t* data, size_t size);

// Convert a virtual address to a byte offset within the CODE section.
// Returns (size_t)-1 if the address is outside the section.
inline size_t pe_va_to_offset(const CodeSection& cs, uint32_t va) {
    if (va < cs.vma) return (size_t)-1;
    size_t off = va - cs.vma;
    return (off < cs.size) ? off : (size_t)-1;
}

} // namespace fx
