#include "fx/fnt.h"
#include "fx/pe.h"
#include <cstring>

namespace fx {

static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}
static void w32le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
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

// Interpret x86 glyph function code to produce a pixel bitmap. Vocabulary
// (complete across all 15 install fonts — #97):
//   03 F9          ADD EDI, ECX        advance one row
//   88 07          MOV [EDI], AL       1 pixel  at column 0
//   88 47 NN       MOV [EDI+NN], AL    1 pixel  at column NN
//   66 89 07       MOV [EDI], AX       2 pixels at column 0
//   66 89 47 NN    MOV [EDI+NN], AX    2 pixels at column NN
//   89 07          MOV [EDI], EAX      4 pixels at column 0
//   89 47 NN       MOV [EDI+NN], EAX   4 pixels at column NN
//   C3             RET                 end of glyph
// (G_Print primes AL/AX/EAX with the colour replicated per byte, so a run
// write paints a uniform horizontal run.)
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

    auto set_run = [&](int row, uint32_t col, uint32_t len) {
        if ((uint32_t)row >= height) return;
        for (uint32_t k = 0; k < len; ++k) {
            if (col + k < width)
                g.pixels[(size_t)row * width + col + k] = 0xFF;
        }
    };

    int row = 0;
    const size_t MAX_INSNS = 8192;
    size_t pc = fn_off;
    for (size_t n = 0; n < MAX_INSNS && pc < cs_size; ++n) {
        const uint8_t op0 = cs_data[pc];
        if (op0 == 0xC3) break;  // RET

        if (op0 == 0x03 && pc + 1 < cs_size && cs_data[pc + 1] == 0xF9) {
            ++row;
            pc += 2;
        } else if (op0 == 0x88 && pc + 1 < cs_size && cs_data[pc + 1] == 0x07) {
            set_run(row, 0, 1);
            pc += 2;
        } else if (op0 == 0x88 && pc + 2 < cs_size && cs_data[pc + 1] == 0x47) {
            set_run(row, cs_data[pc + 2], 1);
            pc += 3;
        } else if (op0 == 0x66 && pc + 2 < cs_size && cs_data[pc + 1] == 0x89 &&
                   cs_data[pc + 2] == 0x07) {
            set_run(row, 0, 2);
            pc += 3;
        } else if (op0 == 0x66 && pc + 3 < cs_size && cs_data[pc + 1] == 0x89 &&
                   cs_data[pc + 2] == 0x47) {
            set_run(row, cs_data[pc + 3], 2);
            pc += 4;
        } else if (op0 == 0x89 && pc + 1 < cs_size && cs_data[pc + 1] == 0x07) {
            set_run(row, 0, 4);
            pc += 2;
        } else if (op0 == 0x89 && pc + 2 < cs_size && cs_data[pc + 1] == 0x47) {
            set_run(row, cs_data[pc + 2], 4);
            pc += 3;
        } else {
            break;  // unknown byte — stop (never hit on the install corpus)
        }
    }
    return g;
}

void fnt_render_glyphs(FntFile& fnt, const uint8_t* cs_data, size_t cs_size, uint32_t cs_vma) {
    fnt.glyphs.clear();
    for (int i = 0; i < 256; ++i) {
        uint32_t va = fnt.glyph_fn_va[i];
        uint32_t w  = fnt.glyph_width[i];
        FntGlyph g = va == 0
            ? FntGlyph{(uint8_t)i, w, fnt.font_height, {}}
            : render_glyph((uint8_t)i, va, w, fnt.font_height, cs_data, cs_size, cs_vma);
        if (g.pixels.empty()) g.pixels.assign((size_t)w * fnt.font_height, 0);
        fnt.glyphs.push_back(std::move(g));
    }
}

std::vector<uint8_t> fnt_emit_glyph(const uint8_t* pixels, uint32_t width,
                                    uint32_t height) {
    std::vector<uint8_t> out;
    bool any = false;
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        if (pixels[i]) { any = true; break; }
    }
    if (!any) {
        out.push_back(0xC3);
        return out;
    }
    for (uint32_t row = 0; row < height; ++row) {
        uint32_t col = 0;
        while (col < width) {
            if (!pixels[(size_t)row * width + col]) { ++col; continue; }
            uint32_t run = 1;
            while (col + run < width && pixels[(size_t)row * width + col + run]) ++run;
            uint32_t c = col, left = run;
            while (left) {
                if (left >= 4) {
                    if (c == 0) { out.push_back(0x89); out.push_back(0x07); }
                    else { out.push_back(0x89); out.push_back(0x47); out.push_back((uint8_t)c); }
                    c += 4; left -= 4;
                } else if (left >= 2) {
                    if (c == 0) { out.push_back(0x66); out.push_back(0x89); out.push_back(0x07); }
                    else { out.push_back(0x66); out.push_back(0x89); out.push_back(0x47); out.push_back((uint8_t)c); }
                    c += 2; left -= 2;
                } else {
                    if (c == 0) { out.push_back(0x88); out.push_back(0x07); }
                    else { out.push_back(0x88); out.push_back(0x47); out.push_back((uint8_t)c); }
                    c += 1; left -= 1;
                }
            }
            col += run;
        }
        out.push_back(0x03);  // ADD EDI, ECX
        out.push_back(0xF9);
    }
    out.push_back(0xC3);
    return out;
}

std::vector<uint8_t> fnt_repack(const uint8_t* orig, size_t orig_size,
                                uint32_t height,
                                const uint32_t widths[256],
                                const std::vector<FntGlyph>& glyphs) {
    if (glyphs.size() != 256) return {};
    CodeSection cs = pe_code_section(orig, orig_size);
    const size_t kStruct = 4 + 256 * 4 + 256 * 4;  // 2052 bytes
    if (!cs.data || cs.size < kStruct) return {};

    // The original body region: bodies start right after the FONT struct
    // and run to the maximum body end; everything past that (tail tables,
    // section padding) carries over verbatim at its original offset.
    FntFile of = fnt_parse(orig, orig_size);
    if (!of.valid) return {};
    size_t body_base = kStruct;
    size_t tail_off = body_base;
    for (int i = 0; i < 256; ++i) {
        if (of.glyph_fn_va[i] < cs.vma) return {};
        size_t off = of.glyph_fn_va[i] - cs.vma;
        // walk to RET with the known vocabulary to find this body's end
        size_t pc = off;
        bool ret_seen = false;
        while (pc < cs.size) {
            const uint8_t b = cs.data[pc];
            if (b == 0xC3) { ++pc; ret_seen = true; break; }
            // The displacement disambiguation reads pc+1/pc+2; a body
            // truncated at the section end must not read past it (fuzz_fnt).
            if (b == 0x03) pc += 2;
            else if (b == 0x88) pc += (pc + 1 < cs.size && cs.data[pc + 1] == 0x47) ? 3 : 2;
            else if (b == 0x66) pc += (pc + 2 < cs.size && cs.data[pc + 2] == 0x47) ? 4 : 3;
            else if (b == 0x89) pc += (pc + 1 < cs.size && cs.data[pc + 1] == 0x47) ? 3 : 2;
            else return {};
        }
        if (!ret_seen) return {};  // body ran off the section without a RET
        if (pc > tail_off) tail_off = pc;
    }

    // Re-emit all 256 bodies in character order at the body base.
    std::vector<uint8_t> code;
    uint32_t new_va[256];
    for (int i = 0; i < 256; ++i) {
        const FntGlyph& g = glyphs[(size_t)i];
        new_va[i] = (uint32_t)(cs.vma + body_base + code.size());
        auto body = g.pixels.empty()
            ? std::vector<uint8_t>{0xC3}
            : fnt_emit_glyph(g.pixels.data(), g.width, g.height);
        code.insert(code.end(), body.begin(), body.end());
    }
    if (body_base + code.size() > tail_off) return {};  // overruns the region

    std::vector<uint8_t> out(orig, orig + orig_size);
    uint8_t* ocs = out.data() + (cs.data - orig);
    w32le(ocs, height);
    for (int i = 0; i < 256; ++i) w32le(ocs + 4 + i * 4, new_va[i]);
    for (int i = 0; i < 256; ++i) w32le(ocs + 4 + 1024 + i * 4, widths[i]);
    memcpy(ocs + body_base, code.data(), code.size());
    // Pad any shrinkage up to the tail with RETs (harmless, unreferenced).
    for (size_t p = body_base + code.size(); p < tail_off; ++p) ocs[p] = 0xC3;
    return out;
}

} // namespace fx
