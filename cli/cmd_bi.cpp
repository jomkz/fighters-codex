#include "ft/bi.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_bi() {
    puts("Usage:");
    puts("  ft bi dump <file.BI>");
}

static int cmd_bi_dump(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf((size_t)sz);
    fread(buf.data(), 1, (size_t)sz, f);
    fclose(f);

    auto instrs = ft::bi_disasm(buf.data(), buf.size());
    if (instrs.empty()) {
        fprintf(stderr, "No CODE section or empty bytecode in: %s\n", path);
        return 1;
    }

    for (const auto& instr : instrs) {
        // Label annotations (those ending with ':') are printed without offset
        if (!instr.text.empty() && instr.text.back() == ':')
            printf("%s\n", instr.text.c_str());
        else
            printf("  %04X  %s\n", instr.offset, instr.text.c_str());
    }
    return 0;
}

int cmd_bi(int argc, char** argv) {
    if (argc < 2) { usage_bi(); return 1; }
    if (strcmp(argv[1], "dump") == 0) {
        if (argc < 3) { usage_bi(); return 1; }
        return cmd_bi_dump(argv[2]);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_bi();
    return 1;
}
