#include "ft/ai.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void usage_ai() {
    puts("Usage:");
    puts("  ft ai compile <file.AI> -o <file.BI>");
}

static int cmd_ai_compile(const char* src_path, const char* out_path) {
    FILE* f = fopen(src_path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", src_path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::string source((size_t)sz, '\0');
    fread(&source[0], 1, (size_t)sz, f);
    fclose(f);

    std::vector<ft::AiCompileError> errors;
    std::vector<uint8_t> bi = ft::ai_compile(source, errors);

    for (const auto& e : errors)
        fprintf(stderr, "%s:%d: error: %s\n", src_path, e.line, e.message.c_str());

    if (bi.empty()) {
        fprintf(stderr, "Compilation failed.\n");
        return 1;
    }

    FILE* out = fopen(out_path, "wb");
    if (!out) { fprintf(stderr, "Cannot write: %s\n", out_path); return 1; }
    fwrite(bi.data(), 1, bi.size(), out);
    fclose(out);

    printf("Wrote %zu bytes to %s\n", bi.size(), out_path);
    return 0;
}

int cmd_ai(int argc, char** argv) {
    if (argc < 2) { usage_ai(); return 1; }
    if (strcmp(argv[1], "compile") == 0) {
        if (argc < 5 || strcmp(argv[3], "-o") != 0) { usage_ai(); return 1; }
        return cmd_ai_compile(argv[2], argv[4]);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_ai();
    return 1;
}
