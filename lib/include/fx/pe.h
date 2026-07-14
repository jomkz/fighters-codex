#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

// One entry of the import table.
//
// FA's overlays IMPORT FROM THE GAME EXECUTABLE: every .DLG names `main.dll` and pulls in the
// engine's own drawing functions (`_DrawAction`, `_DrawListBox`, ...). So the import table is
// not incidental -- it says what a dialog is MADE OF, in the engine's own names, and every one
// of those names should be a symbol we have claimed in db/.
struct PeImport {
    std::string module;   // e.g. "main.dll", lowercased by the linker
    std::string name;     // e.g. "_DrawAction"; empty when imported by ordinal
    uint16_t    ordinal = 0;  // set only when `name` is empty
};

// Parse the import directory. Returns empty when the file has none, or is malformed --
// every offset in here comes from the file, so all arithmetic is 64-bit and bounds-checked.
std::vector<PeImport> pe_imports(const uint8_t* data, size_t size);

} // namespace fx
