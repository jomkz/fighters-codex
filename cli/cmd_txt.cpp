#include "fx/txt.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_txt() {
    puts("Usage:");
    puts("  fx txt info <file.TXT>          # kind + directive structure summary");
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

static int cmd_txt_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto doc = fx::txt_read(data.data(), data.size());
    const char* kind = "plain text (no directives)";
    switch (fx::txt_classify(doc)) {
        case fx::TxtKind::CampaignDescription: kind = "campaign description"; break;
        case fx::TxtKind::UiTemplate:          kind = "UI layout template"; break;
        case fx::TxtKind::PlainText:           break;
    }
    printf("File: %s (%zu bytes, %zu lines)\n", path, data.size(), doc.lines.size());
    printf("Kind: %s\n", kind);
    printf("Directives: %zu section(s), %zu page break(s), %zu button(s), %zu picture(s)\n",
           fx::txt_count(doc, ".section"), fx::txt_count(doc, ".page"),
           fx::txt_count(doc, ".button"), fx::txt_count(doc, ".picture"));

    auto out = fx::txt_write(doc);
    printf("Round-trip: %s\n",
           (out.size() == data.size() &&
            memcmp(out.data(), data.data(), data.size()) == 0)
               ? "byte-identical" : "MISMATCH (report this)");
    return 0;
}

int cmd_txt(int argc, char** argv) {
    if (argc < 3) { usage_txt(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_txt_info(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_txt();
    return 1;
}
