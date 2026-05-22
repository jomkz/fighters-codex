#include "fx/mission.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>

using namespace fx;
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

// mission/mm  info  <file>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft mission info <file.M|.MM>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    auto info = mission_parse_info(data.data(), data.size());
    printf("File:       %s\n", argv[1]);
    if (!info.map_file.empty())   printf("Map:        %s\n", info.map_file.c_str());
    if (!info.layer_file.empty()) printf("Layer:      %s  (index %d)\n", info.layer_file.c_str(), info.layer_index);
    if (info.time_h >= 0)         printf("Time:       %02d:%02d\n", info.time_h, info.time_m);
    printf("Clouds:     %d\n", info.clouds);
    printf("Wind:       %d deg  %d kt\n", info.wind_dir, info.wind_speed);
    printf("Objects:    %d\n", info.obj_count);
    if (!info.screen_flags.empty()) {
        printf("Screens:    ");
        for (auto& s : info.screen_flags) printf("%s ", s.c_str());
        printf("\n");
    }
    return 0;
}

// mission/mm  unpack  <file>  [-o out.txt]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft mission unpack <file.M|.MM> [-o out.txt]\n"); return 1; }
    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    auto out = mission_roundtrip(data.data(), data.size());
    std::string out_path = dst ? dst : (fs::path(src).string() + ".txt");
    if (!write_file(out_path.c_str(), out)) { fprintf(stderr, "Cannot write: %s\n", out_path.c_str()); return 1; }
    printf("%s -> %s (%zu bytes)\n", src, out_path.c_str(), out.size());
    return 0;
}

// mission/mm  pack  <in.txt>  -o <out>
static int cmd_pack(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "Usage: ft mission pack <in.txt> -o <out.M|.MM>\n"); return 1; }
    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];
    if (!dst) { fprintf(stderr, "Missing -o\n"); return 1; }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    auto out = mission_roundtrip(data.data(), data.size());
    if (!write_file(dst, out)) { fprintf(stderr, "Cannot write: %s\n", dst); return 1; }
    printf("%s -> %s (%zu bytes)\n", src, dst, out.size());
    return 0;
}

int cmd_mission(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft mission <info|unpack|pack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "pack")   == 0) return cmd_pack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown mission subcommand: %s\n", sub);
    return 1;
}
