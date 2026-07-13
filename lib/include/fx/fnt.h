#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fx {

struct FntGlyph {
    uint8_t  ch;
    uint32_t width;
    uint32_t height;
    // rendered bitmap: width × height bytes, 0x00=transparent 0xFF=set
    std::vector<uint8_t> pixels;
};

struct FntFile {
    bool     valid = false;
    uint32_t font_height;
    uint32_t glyph_fn_va[256];
    uint32_t glyph_width[256];
    std::vector<FntGlyph> glyphs; // populated by fnt_render_glyphs()
};

// Parse FONT struct from raw PE DLL data.
FntFile fnt_parse(const uint8_t* data, size_t size);

// Execute x86 glyph functions to render each glyph into a pixel bitmap.
// After this call, fnt.glyphs holds all 256 characters. The instruction
// vocabulary is byte, word, and dword pixel-run writes
// (MOV [EDI(+n)], AL/AX/EAX), row advance (ADD EDI, ECX), and RET —
// confirmed complete across every glyph of all 15 install fonts (#97).
void fnt_render_glyphs(FntFile& fnt, const uint8_t* cs_data, size_t cs_size, uint32_t cs_vma);

// Emit one glyph body with the canonical encoding the original compiler
// used (proven byte-identical over all 3,840 install glyph bodies): per
// row, ascending pixel runs split greedily 4/2/1 (dword/word/byte writes,
// short [EDI] forms at column 0), then a row advance; an all-blank glyph
// is a lone RET with no advances.
std::vector<uint8_t> fnt_emit_glyph(const uint8_t* pixels, uint32_t width,
                                    uint32_t height);

// Rebuild a FNT DLL around edited glyphs (#97): the 256 bodies re-emit in
// character order at the original body base, the function-VA table is
// rebuilt, and `height`/`widths` replace the FONT struct fields; every
// other container byte (PE headers, $$DOSX, the post-code tail, padding)
// carries over verbatim. `glyphs` must hold all 256 characters in order.
// Returns empty if the re-emitted code would overrun the original region.
std::vector<uint8_t> fnt_repack(const uint8_t* orig, size_t orig_size,
                                uint32_t height,
                                const uint32_t widths[256],
                                const std::vector<FntGlyph>& glyphs);

} // namespace fx
