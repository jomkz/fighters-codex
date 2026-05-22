#include "fx/fnt.h"
#include "fx/pe.h"
#include <cstring>

namespace fx {

static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

FntFile fnt_parse(const uint8_t* data, size_t size) {
    FntFile result{};
    CodeSection cs = pe_code_section(data, size);
    if (!cs.data || cs.size < 4 + 256*4 + 256*4) return result;

    const uint8_t* p = cs.data;
    result.font_height = u32le(p);
    p += 4;
    for (int i = 0; i < 256; ++i) { result.glyph_fn_va[i] = u32le(p); p += 4; }
    for (int i = 0; i < 256; ++i) { result.glyph_width[i] = u32le(p); p += 4; }

    result.valid = true;
    return result;
}

// Interpret x86 glyph function bytecode to produce a pixel bitmap.
// Instructions used:
//   03 F9          ADD EDI, ECX  â†’ advance one row
//   88 07          MOV [EDI], AL  â†’ write pixel at column 0
//   88 47 NN       MOV [EDI+NN], AL  â†’ write pixel at column NN
//   C3             RET  â†’ end of glyph
// We simulate a virtual framebuffer wide enough to hold the glyph.
static FntGlyph render_glyph(uint8_t ch, uint32_t va, uint32_t width,
                              uint32_t height,
                              const uint8_t* cs_data, size_t cs_size,
                              uint32_t cs_vma) {
    FntGlyph g{};
    g.ch = ch;
    g.width = width;
    g.height = height;
    g.pixels.assign((size_t)width * height, 0);

    size_t fn_off = (va >= cs_vma) ? (size_t)(va - cs_vma) : cs_size;
    if (fn_off >= cs_size) return g;

    // Virtual framebuffer: row Ã— col, one byte per pixel
    // EDI tracks absolute position: row * width + 0 (column offset added by MOV)
    int edi_row = 0;  // current row index (EDI = edi_row * width)
    const size_t MAX_INSNS = 4096;
    size_t pc = fn_off;

    for (size_t n = 0; n < MAX_INSNS && pc < cs_size; ++n) {
        uint8_t op0 = cs_data[pc];
        if (op0 == 0xC3) break;  // RET

        if (op0 == 0x03 && pc + 1 < cs_size && cs_data[pc + 1] == 0xF9) {
            // ADD EDI, ECX â€” advance one row
            ++edi_row;
            pc += 2;
            continue;
        }
        if (op0 == 0x88 && pc + 1 < cs_size && cs_data[pc + 1] == 0x07) {
            // MOV [EDI], AL â€” pixel at column 0 of current row
            if ((uint32_t)edi_row < height && width > 0)
                g.pixels[(size_t)edi_row * width + 0] = 0xFF;
            pc += 2;
            continue;
        }
        if (op0 == 0x88 && pc + 2 < cs_size && cs_data[pc + 1] == 0x47) {
            // MOV [EDI+NN], AL â€” pixel at column NN
            uint8_t col = cs_data[pc + 2];
            if ((uint32_t)edi_row < height && col < width)
                g.pixels[(size_t)edi_row * width + col] = 0xFF;
            pc += 3;
            continue;
        }
        // Unknown byte â€” stop
        break;
    }
    return g;
}

void fnt_render_glyphs(FntFile& fnt, const uint8_t* cs_data, size_t cs_size, uint32_t cs_vma) {
    fnt.glyphs.clear();
    for (int i = 0; i < 256; ++i) {
        uint32_t va = fnt.glyph_fn_va[i];
        uint32_t w  = fnt.glyph_width[i];
        if (va == 0) continue;
        FntGlyph g = render_glyph((uint8_t)i, va, w, fnt.font_height,
                                  cs_data, cs_size, cs_vma);
        fnt.glyphs.push_back(std::move(g));
    }
}

} // namespace fx
