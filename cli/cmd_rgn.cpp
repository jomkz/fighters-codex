#include "fx/rgn.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_rgn() {
    puts("Usage:");
    puts("  fx rgn info <file.RGN>          # record count + round-trip check");
    puts("  fx rgn dump <file.RGN>          # per-record name and rectangle");
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

static int rgn_load(const char* path, fx::RgnFile& rgn,
                    std::vector<uint8_t>& data) {
    data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }
    if (!fx::rgn_read(data.data(), data.size(), rgn)) {
        fprintf(stderr,
                "Not a valid RGN (size must be 4 + 40 x count): %s (%zu bytes)\n",
                path, data.size());
        return 1;
    }
    return 0;
}

static int cmd_rgn_info(const char* path) {
    fx::RgnFile rgn;
    std::vector<uint8_t> data;
    if (rgn_load(path, rgn, data)) return 1;

    size_t rects = 0;
    for (auto& r : rgn.records)
        if (r.vertex_count == 4) rects++;
    printf("RGN region map: %zu record(s), %zu rectangular\n",
           rgn.records.size(), rects);

    auto out = fx::rgn_write(rgn);
    printf("Round-trip: %s\n",
           (out.size() == data.size() &&
            memcmp(out.data(), data.data(), data.size()) == 0)
               ? "byte-identical" : "MISMATCH (report this)");
    return 0;
}

static int cmd_rgn_dump(const char* path) {
    fx::RgnFile rgn;
    std::vector<uint8_t> data;
    if (rgn_load(path, rgn, data)) return 1;

    printf("%-6s %-8s %s\n", "Name", "Vertices", "Coordinates");
    for (auto& r : rgn.records) {
        printf("%-6s %-8u", fx::rgn_name(r).c_str(), r.vertex_count);
        for (uint32_t k = 0; k < 8 && k < r.vertex_count * 2; k += 2)
            printf(" (%u,%u)", r.xy[k], r.xy[k + 1]);
        printf("\n");
    }
    printf("\n%zu record(s)\n", rgn.records.size());
    return 0;
}

int cmd_rgn(int argc, char** argv) {
    if (argc < 3) { usage_rgn(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_rgn_info(argv[2]);
    if (strcmp(argv[1], "dump") == 0) return cmd_rgn_dump(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_rgn();
    return 1;
}
