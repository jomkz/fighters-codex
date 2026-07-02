#include "fx/raw.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace fx;

// stb_image_write declarations only â€” implementation compiled in cmd_pic.cpp
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

int cmd_raw(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx raw <info|unpack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown raw subcommand: %s\n", sub);
    return 1;
}
