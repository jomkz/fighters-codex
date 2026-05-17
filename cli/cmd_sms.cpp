#include "ft/sms.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace ft;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// sms dump <FA.SMS> [-o out.csv]
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft sms dump <FA.SMS> [-o out.csv]\n");
        return 1;
    }
    const char* sms_path = argv[1];
    const char* out_path = nullptr;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
    }

    auto data = read_file(sms_path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", sms_path); return 1; }

    auto syms = sms_parse(data.data(), data.size());
    if (syms.empty()) { fprintf(stderr, "No symbols parsed from: %s\n", sms_path); return 1; }

    // Sort by VA
    std::sort(syms.begin(), syms.end(),
              [](const SmsSymbol& a, const SmsSymbol& b){ return a.va < b.va; });

    FILE* out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) { fprintf(stderr, "Cannot write: %s\n", out_path); return 1; }
    }

    fprintf(out, "va,name\n");
    for (auto& s : syms)
        fprintf(out, "0x%08X,%s\n", s.va, s.name.c_str());

    if (out_path) {
        fclose(out);
        fprintf(stdout, "%s -> %s (%zu symbols)\n", sms_path, out_path, syms.size());
    } else {
        fprintf(stderr, "(%zu symbols)\n", syms.size());
    }
    return 0;
}

int cmd_sms(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft sms <dump> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    fprintf(stderr, "Unknown sms subcommand: %s\n", argv[1]);
    return 1;
}
