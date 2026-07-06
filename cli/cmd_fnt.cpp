#include "fx/fnt.h"
#include "fx/pe.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

// stb declarations only -- implementations compiled in cmd_pic.cpp / pic.cpp
#include "stb_image.h"
#include "stb_image_write.h"

namespace fs = std::filesystem;

static void write_png_cb(void* ctx, void* data, int size) {
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    const uint8_t* b = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), b, b + size);
}

static void usage_fnt() {
    puts("Usage:");
    puts("  fx fnt info   <file.FNT>");
    puts("  fx fnt unpack <file.FNT> [-o output_dir]");
    puts("  fx fnt pack   <orig.FNT> <dir> [-o out.FNT]");
}

static int cmd_fnt_info(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    fx::FntFile fnt = fx::fnt_parse(buf.data(), buf.size());
    if (!fnt.valid) { fprintf(stderr, "Failed to parse FNT: %s\n", path); return 1; }

    printf("font_height: %u\n", fnt.font_height);
    printf("%-6s %-8s %s\n", "ASCII", "Width", "FnVA");
    for (int i = 32; i < 127; ++i) {
        printf("  %-4d %-8u 0x%08X\n", i, fnt.glyph_width[i], fnt.glyph_fn_va[i]);
    }
    return 0;
}

static int cmd_fnt_unpack(int argc, char** argv) {
    const char* path = nullptr;
    const char* out_dir = ".";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_dir = argv[++i];
        else if (!path) path = argv[i];
    }
    if (!path) { usage_fnt(); return 1; }

    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    fx::FntFile fnt = fx::fnt_parse(buf.data(), buf.size());
    if (!fnt.valid) { fprintf(stderr, "Failed to parse FNT: %s\n", path); return 1; }

    fx::CodeSection cs = fx::pe_code_section(buf.data(), buf.size());
    if (!cs.data) { fprintf(stderr, "No CODE section in %s\n", path); return 1; }

    fx::fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);

    // Write metrics.csv
    {
        fs::path csv_path = fs::path(out_dir) / "metrics.csv";
        FILE* fc = fopen(csv_path.string().c_str(), "wb");  // binary: LF line endings on every platform
        if (!fc) { fprintf(stderr, "Cannot write: %s\n", csv_path.generic_string().c_str()); return 1; }
        fprintf(fc, "ascii,char,width,height\n");
        for (const fx::FntGlyph& g : fnt.glyphs) {
            char display = (g.ch >= 32 && g.ch < 127) ? (char)g.ch : '?';
            fprintf(fc, "%d,%c,%u,%u\n", (int)g.ch, display, g.width, g.height);
        }
        fclose(fc);
        printf("Wrote %s\n", csv_path.generic_string().c_str());
    }

    // Build glyph sheet: all printable glyphs in a grid
    // 16 columns, rows as needed; each cell = max_w x font_height pixels
    uint32_t max_w = 1;
    for (int i = 0; i < 256; ++i) if (fnt.glyph_width[i] > max_w) max_w = fnt.glyph_width[i];
    if (max_w == 0) max_w = 1;
    uint32_t fh = fnt.font_height;
    if (fh == 0) fh = 1;

    // Count printable glyphs (ASCII 33-126)
    std::vector<const fx::FntGlyph*> printable;
    for (const fx::FntGlyph& g : fnt.glyphs) {
        if (g.ch >= 33 && g.ch <= 126) printable.push_back(&g);
    }

    const int COLS = 16;
    int n_rows = ((int)printable.size() + COLS - 1) / COLS;
    int sheet_w = (int)max_w * COLS;
    int sheet_h = (int)fh * n_rows;
    if (sheet_w <= 0 || sheet_h <= 0) { printf("No printable glyphs.\n"); return 0; }

    std::vector<uint8_t> sheet((size_t)sheet_w * sheet_h, 0);

    for (int gi = 0; gi < (int)printable.size(); ++gi) {
        const fx::FntGlyph* g = printable[gi];
        int cell_col = gi % COLS;
        int cell_row = gi / COLS;
        int x0 = cell_col * (int)max_w;
        int y0 = cell_row * (int)fh;
        for (uint32_t row = 0; row < g->height && row < fh; ++row) {
            for (uint32_t col = 0; col < g->width && col < max_w; ++col) {
                sheet[(y0 + row) * sheet_w + x0 + col] =
                    g->pixels[row * g->width + col];
            }
        }
    }

    // Convert 1-channel mask to RGBA so pixels show as white on black
    std::vector<uint8_t> rgba((size_t)sheet_w * sheet_h * 4);
    for (int i = 0; i < sheet_w * sheet_h; ++i) {
        uint8_t v = sheet[i];
        rgba[i*4+0] = v; rgba[i*4+1] = v; rgba[i*4+2] = v; rgba[i*4+3] = 0xFF;
    }

    std::vector<uint8_t> png_buf;
    stbi_write_png_to_func(write_png_cb, &png_buf, sheet_w, sheet_h, 4, rgba.data(), sheet_w * 4);

    fs::path png_path = fs::path(out_dir) / "glyph_sheet.png";
    FILE* fp = fopen(png_path.string().c_str(), "wb");
    if (!fp) { fprintf(stderr, "Cannot write: %s\n", png_path.generic_string().c_str()); return 1; }
    fwrite(png_buf.data(), 1, png_buf.size(), fp);
    fclose(fp);
    printf("Wrote %s (%dx%d, %d glyphs, font_height=%u)\n",
           png_path.generic_string().c_str(), sheet_w, sheet_h, (int)printable.size(), fnt.font_height);
    return 0;
}

// fx fnt pack <orig.FNT> <dir> [-o out.FNT] — rebuild the font DLL from an
// unpack directory (glyph_sheet.png + metrics.csv). Printable glyphs
// (ASCII 33–126) come from the sheet cells; everything else carries over
// from the original. The glyph functions are recompiled to x86 with the
// original compiler's canonical encoding (#97): an unedited unpack→pack
// loop is byte-identical.
static int cmd_fnt_pack(int argc, char** argv) {
    const char* out_path = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { usage_fnt(); return 1; }

    FILE* f = fopen(pos[0], "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", pos[0]); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    fx::FntFile fnt = fx::fnt_parse(buf.data(), buf.size());
    if (!fnt.valid) { fprintf(stderr, "Failed to parse FNT: %s\n", pos[0]); return 1; }
    fx::CodeSection cs = fx::pe_code_section(buf.data(), buf.size());
    if (!cs.data) { fprintf(stderr, "No CODE section in %s\n", pos[0]); return 1; }
    fx::fnt_render_glyphs(fnt, cs.data, cs.size, cs.vma);

    // metrics.csv: ascii,char,width,height — width edits are honoured.
    uint32_t widths[256];
    memcpy(widths, fnt.glyph_width, sizeof(widths));
    uint32_t height = fnt.font_height;
    {
        fs::path csv = fs::path(pos[1]) / "metrics.csv";
        FILE* fc = fopen(csv.string().c_str(), "rb");
        if (!fc) { fprintf(stderr, "Cannot open: %s\n", csv.generic_string().c_str()); return 1; }
        char line[256];
        fgets(line, sizeof(line), fc);  // header
        while (fgets(line, sizeof(line), fc)) {
            int ascii = 0;
            unsigned w = 0, h = 0;
            if (sscanf(line, "%d,%*c,%u,%u", &ascii, &w, &h) == 3 ||
                sscanf(line, "%d,%*[^,],%u,%u", &ascii, &w, &h) == 3) {
                if (ascii >= 0 && ascii < 256) {
                    widths[ascii] = w;
                    height = h;
                }
            }
        }
        fclose(fc);
    }

    // glyph_sheet.png: 16 columns of max_w x height cells, printable glyphs
    // 33..126 in order (the exact inverse of unpack's layout).
    uint32_t max_w = 1;
    for (int i = 0; i < 256; ++i) if (widths[i] > max_w) max_w = widths[i];
    fs::path sheet_path = fs::path(pos[1]) / "glyph_sheet.png";
    int sw = 0, sh = 0, ch = 0;
    uint8_t* sheet = stbi_load(sheet_path.string().c_str(), &sw, &sh, &ch, 4);
    if (!sheet) { fprintf(stderr, "Cannot load: %s\n", sheet_path.generic_string().c_str()); return 1; }

    std::vector<fx::FntGlyph> glyphs(256);
    for (int i = 0; i < 256; ++i) {
        glyphs[(size_t)i] = fnt.glyphs[(size_t)i];  // carried unless on the sheet
        glyphs[(size_t)i].width = widths[i];
        glyphs[(size_t)i].height = height;
    }
    const int COLS = 16;
    for (int c = 33; c <= 126; ++c) {
        const int gi = c - 33;
        const int x0 = (gi % COLS) * (int)max_w;
        const int y0 = (gi / COLS) * (int)height;
        fx::FntGlyph& g = glyphs[(size_t)c];
        g.pixels.assign((size_t)g.width * height, 0);
        for (uint32_t row = 0; row < height; ++row) {
            for (uint32_t col = 0; col < g.width; ++col) {
                const int sx = x0 + (int)col, sy = y0 + (int)row;
                if (sx >= sw || sy >= sh) continue;
                if (sheet[((size_t)sy * sw + sx) * 4] > 127)
                    g.pixels[(size_t)row * g.width + col] = 0xFF;
            }
        }
    }
    stbi_image_free(sheet);

    auto out = fx::fnt_repack(buf.data(), buf.size(), height, widths, glyphs);
    if (out.empty()) {
        fprintf(stderr, "Pack failed (glyph code exceeds the original region?)\n");
        return 1;
    }
    std::string dst = out_path ? out_path
                               : fs::path(pos[0]).stem().string() + ".pack.FNT";
    std::ofstream ofs(dst, std::ios::binary);
    if (!ofs || !ofs.write((const char*)out.data(), (std::streamsize)out.size())) {
        fprintf(stderr, "Cannot write: %s\n", dst.c_str());
        return 1;
    }
    printf("%s + %s -> %s (%zu bytes)\n", pos[0], pos[1], dst.c_str(), out.size());
    return 0;
}

int cmd_fnt(int argc, char** argv) {
    if (argc < 2) { usage_fnt(); return 1; }
    const char* sub = argv[1];
    if (strcmp(sub, "info") == 0) {
        if (argc < 3) { usage_fnt(); return 1; }
        return cmd_fnt_info(argv[2]);
    }
    if (strcmp(sub, "unpack") == 0) {
        return cmd_fnt_unpack(argc - 1, argv + 1);
    }
    if (strcmp(sub, "pack") == 0) {
        return cmd_fnt_pack(argc - 1, argv + 1);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", sub);
    usage_fnt();
    return 1;
}
