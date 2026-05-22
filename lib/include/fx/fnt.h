#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fx {

struct FntGlyph {
    uint8_t  ch;
    uint32_t width;
    uint32_t height;
    // rendered bitmap: width Ã— height bytes, 0x00=transparent 0xFF=set
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
// After this call, fnt.glyphs is populated.
void fnt_render_glyphs(FntFile& fnt, const uint8_t* cs_data, size_t cs_size, uint32_t cs_vma);

} // namespace fx
