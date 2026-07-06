// Fuzz target: the FNT font codec above the PE reader fuzz_pe already
// covers — FONT struct parse, the x86 glyph interpreter (run writes, row
// advances, RET) walking hostile code bytes, and the repacker's body walk
// over whatever parsed; the path `fx fnt unpack` / `fx fnt pack` runs over
// untrusted bytes.
//
// Header-declared metrics bound the render (each glyph allocates
// width*font_height from the file's tables).

#include <cstddef>
#include <cstdint>

#include <fx/fnt.h>
#include <fx/pe.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::FntFile fnt = fx::fnt_parse(data, size);
    if (!fnt.valid) return 0;

    if (fnt.font_height > 128) return 0;
    for (int i = 0; i < 256; ++i)
        if (fnt.glyph_width[i] > 1024) return 0;

    fx::CodeSection cs = fx::pe_code_section(data, size);
    if (!cs.data) return 0;
    fx::fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);
    fx::fnt_repack(data, size, fnt.font_height, fnt.glyph_width, fnt.glyphs);
    return 0;
}
