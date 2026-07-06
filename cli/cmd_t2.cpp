#include "fx/t2.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <filesystem>

using namespace fx;
namespace fs = std::filesystem;

// stb declarations only — implementations compiled in cmd_pic.cpp / pic.cpp
#include "stb_image_write.h"

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

static uint8_t max_leaf_elevation(const T2Map& map) {
    uint8_t max_elev = 0;
    for (const auto& r : map.leaves)
        if (r.elevation > max_elev) max_elev = r.elevation;
    return max_elev;
}

// t2 info <file.T2>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx t2 info <file.T2>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    T2Info info;
    T2Map map;
    if (!t2_info(data.data(), data.size(), &info) ||
        !t2_read(data.data(), data.size(), &map)) {
        fprintf(stderr, "Not a valid T2 terrain file: %s\n", argv[1]);
        return 1;
    }

    std::string stem = fs::path(argv[1]).stem().string();

    printf("Theater:    %s (%s)\n", stem.c_str(), map.theater.c_str());
    printf("Atlas:      %s\n", map.atlas_pic.c_str());
    printf("Grid:       %u x %u (%u tiles)\n", info.dim_x, info.dim_y, info.tile_count);
    printf("Leaves:     %u x %u (%u per tile side)\n",
           info.leaves_w, info.leaves_h, info.leaf_step);
    printf("Elevation:  leaf bands 0..%u\n", max_leaf_elevation(map));

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

// t2 dump <file.T2> [--leaves]
static int cmd_dump(int argc, char** argv) {
    bool leaves = false;
    const char* path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--leaves") == 0) leaves = true;
        else if (!path) path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "Usage: fx t2 dump <file.T2> [--leaves]\n");
        return 1;
    }
    auto data = read_file(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    T2Map map;
    if (!t2_read(data.data(), data.size(), &map)) {
        fprintf(stderr, "Not a valid T2 terrain file: %s\n", path);
        return 1;
    }

    uint32_t w = leaves ? map.leaves_w : map.tiles_w;
    uint32_t h = leaves ? map.leaves_h : map.tiles_h;
    printf("x,y,surface_class,elevation,texture_variant\n");
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            const T2Record& r = leaves ? map.leaf(x, y) : map.summary(x, y);
            printf("%u,%u,0x%02X,%u,%u\n",
                   x, y, r.surface_class, r.elevation, r.texture_variant);
        }
    }
    return 0;
}

// t2 heightmap <file.T2> <out.png>
static int cmd_heightmap(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: fx t2 heightmap <file.T2> <out.png>\n");
        return 1;
    }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    T2Map map;
    if (!t2_read(data.data(), data.size(), &map)) {
        fprintf(stderr, "Not a valid T2 terrain file: %s\n", argv[1]);
        return 1;
    }

    // One pixel per leaf; elevation bands normalized to the file's own
    // maximum so the relief is visible (bands run 0..16 across theaters).
    uint8_t max_elev = max_leaf_elevation(map);
    std::vector<uint8_t> gray((size_t)map.leaves_w * map.leaves_h, 0);
    for (size_t i = 0; i < map.leaves.size(); i++)
        gray[i] = max_elev ? (uint8_t)(map.leaves[i].elevation * 255 / max_elev)
                           : 0;

    if (!stbi_write_png(argv[2], (int)map.leaves_w, (int)map.leaves_h, 1,
                        gray.data(), (int)map.leaves_w)) {
        fprintf(stderr, "Cannot write: %s\n", argv[2]);
        return 1;
    }
    printf("Wrote %s (%u x %u, bands 0..%u)\n",
           argv[2], map.leaves_w, map.leaves_h, max_elev);
    return 0;
}

int cmd_t2(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx t2 <info|dump|heightmap> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "info") == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    if (strcmp(argv[1], "heightmap") == 0) return cmd_heightmap(argc - 1, argv + 1);
    fprintf(stderr, "Unknown t2 subcommand: %s\n", argv[1]);
    return 1;
}
