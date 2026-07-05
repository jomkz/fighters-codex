#include "fx/ssf.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static void usage_ssf() {
    puts("Usage:");
    puts("  fx ssf info <file.SSF>          # keyword summary + round-trip check");
    puts("  fx ssf dump <file.SSF>          # every statement with its arguments");
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

static int cmd_ssf_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto doc = fx::ssf_read(data.data(), data.size());
    printf("File: %s (%zu bytes, %zu lines, %zu statements)\n", path,
           data.size(), doc.text.lines.size(), doc.statements.size());
    std::map<std::string, int> counts;
    for (auto& st : doc.statements) counts[st.keyword]++;
    for (auto& kv : counts) printf("  %-18s %d\n", kv.first.c_str(), kv.second);

    auto out = fx::ssf_write(doc);
    printf("Round-trip: %s\n",
           (out.size() == data.size() &&
            memcmp(out.data(), data.data(), data.size()) == 0)
               ? "byte-identical" : "MISMATCH (report this)");
    return 0;
}

static int cmd_ssf_dump(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto doc = fx::ssf_read(data.data(), data.size());
    for (auto& st : doc.statements) {
        printf("%4zu  %s", st.line + 1, st.keyword.c_str());
        for (auto& a : st.args) printf("  \"%s\"", a.c_str());
        printf("\n");
    }
    printf("\n%zu statement(s)\n", doc.statements.size());
    return 0;
}

int cmd_ssf(int argc, char** argv) {
    if (argc < 3) { usage_ssf(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_ssf_info(argv[2]);
    if (strcmp(argv[1], "dump") == 0) return cmd_ssf_dump(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_ssf();
    return 1;
}
