#include "fx/raw.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace fx;

// stb declarations only — implementations compiled in cmd_pic.cpp / pic.cpp
#include "stb_image.h"
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

// raw info <file.RAW>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx raw info <file.RAW>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    RawInfo info;
    if (!raw_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid RAW screenshot: %s\n", argv[1]);
        return 1;
    }

    printf("File:   %s\n", argv[1]);
    printf("Size:   %u x %u\n", info.width, info.height);
    printf("Bytes:  %zu\n", data.size());
    return 0;
}

// raw unpack <file.RAW> [-o output.png]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx raw unpack <file.RAW> [-o out.png]\n");
        return 1;
    }
    const char* raw_path = argv[1];
    const char* out_path = nullptr;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    auto data = read_file(raw_path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", raw_path); return 1; }

    RawInfo info;
    if (!raw_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid RAW screenshot: %s\n", raw_path);
        return 1;
    }

    auto rgba = raw_decode(data.data(), data.size());
    if (rgba.empty()) { fprintf(stderr, "Decode failed: %s\n", raw_path); return 1; }

    std::string out_str = out_path
        ? out_path
        : fs::path(raw_path).replace_extension(".png").string();

    if (!stbi_write_png(out_str.c_str(), (int)info.width, (int)info.height, 4,
                        rgba.data(), (int)info.width * 4)) {
        fprintf(stderr, "PNG write failed: %s\n", out_str.c_str());
        return 1;
    }

    printf("%s -> %s (%ux%u)\n", raw_path, out_str.c_str(), info.width, info.height);
    return 0;
}

// raw pack <file.png> [-o output.RAW] — PNG -> RAW screenshot (#96). The
// image must use at most 256 distinct colours (the palette is rebuilt).
static int cmd_pack(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx raw pack <file.png> [-o out.RAW]\n");
        return 1;
    }
    const char* png_path = argv[1];
    const char* out_path = nullptr;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    int w = 0, h = 0, ch = 0;
    uint8_t* rgba = stbi_load(png_path, &w, &h, &ch, 4);
    if (!rgba) { fprintf(stderr, "Cannot load image: %s\n", png_path); return 1; }
    auto out = raw_encode(rgba, w, h);
    stbi_image_free(rgba);
    if (out.empty()) {
        fprintf(stderr, "Encode failed (image exceeds 256 distinct colours?): %s\n", png_path);
        return 1;
    }

    std::string out_str = out_path
        ? out_path
        : fs::path(png_path).replace_extension(".RAW").string();
    std::ofstream f(out_str, std::ios::binary);
    if (!f || !f.write((const char*)out.data(), (std::streamsize)out.size())) {
        fprintf(stderr, "Cannot write: %s\n", out_str.c_str());
        return 1;
    }
    printf("%s -> %s (%dx%d, %zu bytes)\n", png_path, out_str.c_str(), w, h, out.size());
    return 0;
}

int cmd_raw(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx raw <info|unpack|pack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "pack")   == 0) return cmd_pack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown raw subcommand: %s\n", sub);
    return 1;
}
