#include "fx/pic.h"
#include "fx/pal.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace fx;

// stb_image_write for PNG output (implementation lives here)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// stb_image declarations only â€” implementation is compiled in pic.cpp
#include "stb_image.h"

namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

static bool write_file(const char* path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    return f.good();
}

// pic info <file.PIC>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx pic info <file.PIC>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    PicInfo info;
    if (!pic_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid PIC: %s\n", argv[1]);
        return 1;
    }

    const char* fmt_name = (info.format == 0) ? "Dense/Texture"
                         : (info.format == 1) ? "Sparse/Image"
                         : (info.format == 0xD8FF) ? "JPEG"
                         : "Unknown";
    printf("File:    %s\n", argv[1]);
    printf("Format:  0x%04X (%s)\n", info.format, fmt_name);
    if (info.format != 0xD8FF) {
        printf("Size:    %u x %u\n", info.width, info.height);
        printf("Pixels:  offset=%u  size=%u\n", info.pixels_offset, info.pixels_size);
        printf("Palette: offset=%u  size=%u (%u colors)\n",
               info.palette_offset, info.palette_size, info.palette_size / 3);
        if (info.format == 1)
            printf("Spans:   offset=%u  size=%u (%u spans)\n",
                   info.spans_offset, info.spans_size, info.spans_size / 10);
        if (info.format == 0)
            printf("RowHds:  offset=%u  size=%u\n", info.rowheads_offset, info.rowheads_size);
    }
    return 0;
}

// pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx pic unpack <file.PIC> [-p PALETTE.PAL] [-o out.png]\n");
        return 1;
    }
    const char* pic_path = argv[1];
    const char* pal_path = nullptr;
    const char* out_path = nullptr;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) pal_path = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    auto pic_data = read_file(pic_path);
    if (pic_data.empty()) { fprintf(stderr, "Cannot read: %s\n", pic_path); return 1; }

    Palette pal = {};
    if (pal_path) {
        auto pd = read_file(pal_path);
        pal = pal_load(pd.data(), pd.size());
    }

    auto rgba = pic_decode(pic_data.data(), pic_data.size(), pal_path ? &pal : nullptr);
    if (rgba.empty()) { fprintf(stderr, "Decode failed: %s\n", pic_path); return 1; }

    // Infer dimensions: for JPEG PICs the header format is 0xD8FF so we can't get w/h from it
    // after decoding. Use stb approach: re-peek.
    int w = 0, h = 0;
    PicInfo info;
    if (pic_info(pic_data.data(), pic_data.size(), &info) && info.format != 0xD8FF) {
        w = (int)info.width;
        h = (int)info.height;
    } else {
        // JPEG: decode again to get dimensions (stb_image gives us them)
        int ch;
        uint8_t* tmp = stbi_load_from_memory(pic_data.data(), (int)pic_data.size(), &w, &h, &ch, 4);
        if (tmp) stbi_image_free(tmp);
    }
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "Cannot determine dimensions: %s\n", pic_path);
        return 1;
    }

    // Default output path: replace .PIC with .png
    std::string out_str;
    if (out_path) {
        out_str = out_path;
    } else {
        out_str = fs::path(pic_path).replace_extension(".png").string();
    }

    if (!stbi_write_png(out_str.c_str(), w, h, 4, rgba.data(), w * 4)) {
        fprintf(stderr, "PNG write failed: %s\n", out_str.c_str());
        return 1;
    }

    printf("%s -> %s (%dx%d)\n", pic_path, out_str.c_str(), w, h);
    return 0;
}

// pic pack <file.png> [-p PALETTE.PAL] [-o output.PIC]
static int cmd_pack(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx pic pack <file.png> [-p PALETTE.PAL] [-o out.PIC]\n");
        return 1;
    }
    const char* png_path = argv[1];
    const char* pal_path = nullptr;
    const char* out_path = nullptr;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) pal_path = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    int w = 0, h = 0, ch = 0;
    uint8_t* rgba = stbi_load(png_path, &w, &h, &ch, 4);
    if (!rgba) { fprintf(stderr, "Cannot load image: %s\n", png_path); return 1; }

    Palette pal = {};
    if (pal_path) {
        auto pd = read_file(pal_path);
        pal = pal_load(pd.data(), pd.size());
    }

    auto pic_data = pic_encode(rgba, w, h, pal);
    stbi_image_free(rgba);
    if (pic_data.empty()) { fprintf(stderr, "Encode failed\n"); return 1; }

    std::string out_str;
    if (out_path) {
        out_str = out_path;
    } else {
        out_str = fs::path(png_path).replace_extension(".PIC").string();
    }

    if (!write_file(out_str.c_str(), pic_data)) {
        fprintf(stderr, "Cannot write: %s\n", out_str.c_str());
        return 1;
    }

    printf("%s -> %s (%zu bytes)\n", png_path, out_str.c_str(), pic_data.size());
    return 0;
}

int cmd_pic(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx pic <info|unpack|pack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "pack")   == 0) return cmd_pack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown pic subcommand: %s\n", sub);
    return 1;
}
