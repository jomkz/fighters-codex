#include "fx/ai.h"
#include "fx/bi.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void usage_bi() {
    puts("Usage:");
    puts("  fx bi dump      <file.BI>");
    puts("  fx bi decompile <file.BI>");
}

static std::vector<uint8_t> read_file_bi(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz > 0 ? (size_t)sz : 0);
    if (!buf.empty() && fread(buf.data(), 1, buf.size(), f) != buf.size()) buf.clear();
    fclose(f);
    return buf;
}

static int cmd_bi_dump(const char* path) {
    std::vector<uint8_t> buf = read_file_bi(path);
    if (buf.empty()) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }

    auto instrs = fx::bi_disasm(buf.data(), buf.size());
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

static int cmd_bi_decompile(const char* path) {
    std::vector<uint8_t> buf = read_file_bi(path);
    if (buf.empty()) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }

    std::string src = fx::ai_decompile(buf.data(), buf.size());
    if (src.empty()) {
        fprintf(stderr,
                "Could not decompile: %s\n"
                "  'fx bi decompile' recovers source from fx-compiled BIs. The stock\n"
                "  game BIs use a different toolchain dialect (linked CALL_DIRECT); use\n"
                "  'fx bi dump' to disassemble those.\n",
                path);
        return 1;
    }
    fwrite(src.data(), 1, src.size(), stdout);
    return 0;
}

int cmd_bi(int argc, char** argv) {
    if (argc < 2) { usage_bi(); return 1; }
    if (strcmp(argv[1], "dump") == 0) {
        if (argc < 3) { usage_bi(); return 1; }
        return cmd_bi_dump(argv[2]);
    }
    if (strcmp(argv[1], "decompile") == 0) {
        if (argc < 3) { usage_bi(); return 1; }
        return cmd_bi_decompile(argv[2]);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_bi();
    return 1;
}
