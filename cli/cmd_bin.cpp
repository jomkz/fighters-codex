#include "fx/bin.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void usage_bin() {
    puts("Usage:");
    puts("  fx bin info <file.BIN>          # kind (from the filename) + size check");
}

static int cmd_bin_info(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    fclose(f);

    // The bytes carry no structure; the name identifies the table (BIN.md).
    const char* base = path;
    for (const char* p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;

    fx::BinKind kind = fx::bin_classify(base);
    printf("Name: %s\n", base);
    printf("Kind: %s\n", fx::bin_kind_desc(kind));
    printf("Size: %zu bytes", size);
    size_t expected = fx::bin_expected_size(kind);
    if (expected != 0)
        printf(" (%s)", size == expected ? "matches the documented size"
                                         : "MISMATCH vs documented size");
    printf("\n");
    return (expected == 0 || size == expected) ? 0 : 1;
}

int cmd_bin(int argc, char** argv) {
    if (argc < 3) { usage_bin(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_bin_info(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_bin();
    return 1;
}
