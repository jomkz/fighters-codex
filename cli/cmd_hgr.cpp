#include "fx/hgr.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

static void usage_hgr() {
    puts("Usage:");
    puts("  fx hgr info    <file.HGR>       # container check + referenced PIC assets");
    puts("  fx hgr strings <file.HGR> [-n MIN]   # embedded strings");
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

static int cmd_hgr_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto info = fx::hgr_info(data.data(), data.size());
    printf("File: %s (%zu bytes)\n", path, data.size());
    if (!info.valid) {
        printf("Container: not a recognized MZ + \"PL\" hangar DLL\n");
        return 1;
    }
    printf("Container: MZ + Phar Lap \"PL\" DLL\n");
    printf("CODE section: %zu bytes at VMA 0x%X\n", info.code.size, info.code.vma);
    if (info.pics.empty()) {
        printf("PIC references: (none found)\n");
    } else {
        printf("PIC references:\n");
        for (auto& p : info.pics) printf("  %s\n", p.c_str());
    }
    return 0;
}

static int cmd_hgr_strings(int argc, char** argv) {
    size_t min_len = 3;
    const char* path = argv[0];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            min_len = (size_t)atoi(argv[++i]);
    }
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto strings = fx::hgr_strings(data.data(), data.size(), min_len);
    for (auto& s : strings) puts(s.c_str());
    fprintf(stderr, "\n%zu string(s)\n", strings.size());
    return 0;
}

int cmd_hgr(int argc, char** argv) {
    if (argc < 3) { usage_hgr(); return 1; }
    if (strcmp(argv[1], "info")    == 0) return cmd_hgr_info(argv[2]);
    if (strcmp(argv[1], "strings") == 0) return cmd_hgr_strings(argc - 2, argv + 2);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_hgr();
    return 1;
}
