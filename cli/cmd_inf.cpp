#include "fx/inf.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>

using namespace fx;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

static void json_str(const std::string& s) {
    putchar('"');
    for (char c : s) {
        switch (c) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n",  stdout); break;
        case '\r': fputs("\\r",  stdout); break;
        default:   putchar(c);            break;
        }
    }
    putchar('"');
}

// fx inf dump <file.INF>
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx inf dump <file.INF>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    InfFile inf = inf_parse(data.data(), data.size());
    if (!inf.valid) { fprintf(stderr, "Parse failed: %s\n", argv[1]); return 1; }

    bool has_stats = !inf.stats.empty();

    printf("{\n");
    printf("  \"file\": \"%s\",\n", argv[1]);

    printf("  \"sections\": [\n");
    for (size_t i = 0; i < inf.sections.size(); i++) {
        const auto& s = inf.sections[i];
        bool last = (i + 1 == inf.sections.size());
        printf("    {\"directive\": ");
        json_str(s.directive);
        printf(", \"text\": ");
        json_str(s.text);
        printf("}%s\n", last ? "" : ",");
    }
    printf("  ]%s\n", has_stats ? "," : "");

    if (has_stats) {
        printf("  \"stats\": {\n");
        size_t n = 0;
        for (const auto& kv : inf.stats) {
            bool last = (++n == inf.stats.size());
            printf("    ");
            json_str(kv.first);
            printf(": ");
            json_str(kv.second);
            printf("%s\n", last ? "" : ",");
        }
        printf("  }\n");
    }

    printf("}\n");
    return 0;
}

int cmd_inf(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx inf <dump> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    fprintf(stderr, "Unknown inf subcommand: %s\n", argv[1]);
    return 1;
}
