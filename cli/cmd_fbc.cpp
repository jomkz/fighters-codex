#include "fx/fbc.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_fbc() {
    puts("Usage:");
    puts("  fx fbc info <file.FBC>          # frame count + totals");
    puts("  fx fbc ls   <file.FBC>          # per-frame size and VDO offset");
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

static int fbc_load(const char* path, std::vector<uint32_t>& sizes) {
    auto data = read_all(path);
    if (data.empty() && sizes.empty()) {
        FILE* f = fopen(path, "rb");
        if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
        fclose(f);
    }
    bool ok = false;
    sizes = fx::fbc_read(data.data(), data.size(), &ok);
    if (!ok) {
        fprintf(stderr, "Not a valid FBC (size %zu is not a multiple of 4): %s\n",
                data.size(), path);
        return 1;
    }
    return 0;
}

static int cmd_fbc_info(const char* path) {
    std::vector<uint32_t> sizes;
    if (fbc_load(path, sizes)) return 1;
    uint64_t total = fx::fbc_frame_offset(sizes, sizes.size());
    printf("FBC frame index: %zu frame(s)\n", sizes.size());
    printf("Frame data bytes: %llu\n",
           (unsigned long long)(total - fx::fbc_frame_offset(sizes, 0)));
    printf("Expected paired VDO size: %llu (816-byte header + frames)\n",
           (unsigned long long)total);
    return 0;
}

static int cmd_fbc_ls(const char* path) {
    std::vector<uint32_t> sizes;
    if (fbc_load(path, sizes)) return 1;
    printf("%-8s  %-12s  %s\n", "Frame", "Bytes", "VDO offset");
    uint64_t off = fx::fbc_frame_offset(sizes, 0);
    for (size_t i = 0; i < sizes.size(); i++) {
        printf("%-8zu  %-12u  0x%06llX\n", i, sizes[i], (unsigned long long)off);
        off += sizes[i];
    }
    printf("\n%zu frame(s)\n", sizes.size());
    return 0;
}

int cmd_fbc(int argc, char** argv) {
    if (argc < 3) { usage_fbc(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_fbc_info(argv[2]);
    if (strcmp(argv[1], "ls")   == 0) return cmd_fbc_ls(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_fbc();
    return 1;
}
