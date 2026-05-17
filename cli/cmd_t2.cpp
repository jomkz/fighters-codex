#include "ft/t2.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <filesystem>

using namespace ft;
namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// t2 info <file.T2>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft t2 info <file.T2>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    T2Info info;
    if (!t2_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid T2 terrain file: %s\n", argv[1]);
        return 1;
    }

    std::string stem = fs::path(argv[1]).stem().string();

    printf("Theater:    %s\n", stem.c_str());
    printf("Grid:       %u x %u (%u tiles)\n", info.dim_x, info.dim_y, info.tile_count);

    // Surface class summary
    uint32_t water_count = 0, land_count = 0;
    for (auto& [cls, cnt] : info.surface_dist) {
        if (cls == 0xFF) water_count += cnt;
        else             land_count  += cnt;
    }
    uint32_t total = info.tile_count;
    printf("Surface:    water %u (%.1f%%)  land %u (%.1f%%)\n",
           water_count, total ? water_count * 100.0 / total : 0.0,
           land_count,  total ? land_count  * 100.0 / total : 0.0);

    // Top non-water classes
    printf("Land classes:\n");
    int shown = 0;
    for (auto& [cls, cnt] : info.surface_dist) {
        if (cls == 0xFF) continue;
        printf("  0x%02X  %u tiles (%.1f%%)\n",
               cls, cnt, total ? cnt * 100.0 / total : 0.0);
        if (++shown >= 8) break;
    }

    return 0;
}

int cmd_t2(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft t2 <info> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "info") == 0) return cmd_info(argc - 1, argv + 1);
    fprintf(stderr, "Unknown t2 subcommand: %s\n", argv[1]);
    return 1;
}
