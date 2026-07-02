#include "fx/pal.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace fx;

// stb_image_write declarations only -- implementation compiled in cmd_pic.cpp
#include "stb_image_write.h"

namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// fx pal info <file.PAL>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx pal info <file.PAL>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    if (data.size() < 768) {
        fprintf(stderr, "Too small for a palette (need 768 bytes): %s\n", argv[1]);
        return 1;
    }

    Palette pal = pal_load(data.data(), data.size());
    printf("File:   %s\n", argv[1]);
    printf("Colors: 256\n\n");
    printf("Idx   R    G    B    Hex\n");
    for (int i = 0; i < 256; i++) {
        printf("%3d  %3u  %3u  %3u  #%02X%02X%02X\n",
               i, pal.r[i], pal.g[i], pal.b[i],
               pal.r[i], pal.g[i], pal.b[i]);
    }
    return 0;
}

// fx pal dump <file.PAL> [-o out.png]
// Writes a 256x256 color swatch: 16x16 grid of 16x16 cells, one cell per palette entry.
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx pal dump <file.PAL> [-o out.png]\n"); return 1; }
    const char* pal_path = argv[1];
    const char* out_path = nullptr;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    auto data = read_file(pal_path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", pal_path); return 1; }
    if (data.size() < 768) {
        fprintf(stderr, "Too small for a palette (need 768 bytes): %s\n", pal_path);
        return 1;
    }

    Palette pal = pal_load(data.data(), data.size());

    const int CELL = 16;
    const int W = 16 * CELL, H = 16 * CELL;
    std::vector<uint8_t> pixels(W * H * 3);
    for (int i = 0; i < 256; i++) {
        int col = i % 16;
        int row = i / 16;
        for (int cy = 0; cy < CELL; cy++) {
            for (int cx = 0; cx < CELL; cx++) {
                int px = (row * CELL + cy) * W * 3 + (col * CELL + cx) * 3;
                pixels[px + 0] = pal.r[i];
                pixels[px + 1] = pal.g[i];
                pixels[px + 2] = pal.b[i];
            }
        }
    }

    std::string out_str = out_path
        ? out_path
        : fs::path(pal_path).replace_extension(".png").string();

    if (!stbi_write_png(out_str.c_str(), W, H, 3, pixels.data(), W * 3)) {
        fprintf(stderr, "PNG write failed: %s\n", out_str.c_str());
        return 1;
    }
    printf("%s -> %s (256x256 swatch, 16x16 grid)\n", pal_path, out_str.c_str());
    return 0;
}

int cmd_pal(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx pal <info|dump> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info") == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    fprintf(stderr, "Unknown pal subcommand: %s\n", sub);
    return 1;
}
