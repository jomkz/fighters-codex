#include "fx/pts.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_pts() {
    puts("Usage:");
    puts("  fx pts info <file.PTS>          # container check + referenced icon PIC");
}

static std::vector<uint8_t> read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf((size_t)sz);
    if (sz > 0 && fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) buf.clear();
    fclose(f);
    return buf;
}

static int cmd_pts_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto info = fx::pts_info(data.data(), data.size());
    printf("File: %s (%zu bytes)\n", path, data.size());
    if (!info.valid) {
        printf("Container: not a recognized MZ + \"PL\" screen-assets DLL\n");
        return 1;
    }
    printf("Container: MZ + Phar Lap \"PL\" DLL\n");
    printf("CODE section: %zu bytes at VMA 0x%X\n", info.code.size,
           info.code.vma);
    printf("Icon: %s\n", info.icon.empty() ? "(none found)" : info.icon.c_str());
    return 0;
}

int cmd_pts(int argc, char** argv) {
    if (argc < 3) { usage_pts(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_pts_info(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_pts();
    return 1;
}
