#include "fx/mt.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_mt() {
    puts("Usage:");
    puts("  fx mt info <file.MT>            # mission id/title/type + round-trip check");
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

static int cmd_mt_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto doc = fx::txt_read(data.data(), data.size());
    auto info = fx::mt_info(doc);
    printf("File: %s (%zu bytes, %zu lines)\n", path, data.size(),
           doc.lines.size());
    printf("Mission: id \"%s\"  source \"%s\"\n", info.mission_id.c_str(),
           info.source_name.c_str());
    printf("Title: %s\n", info.title.c_str());
    printf("Type: %s\n", info.mission_type.c_str());
    // 1=header, 2=briefing, and the rest are the debrief outcomes. 346 of the 363 shipped
    // briefings carry 4 sections and 17 carry 5, so the debrief range is not fixed.
    if (info.sections >= 3)
        printf("Sections: %zu (1=header, 2=briefing, 3-%zu=debrief outcomes)\n",
               info.sections, info.sections);
    else
        printf("Sections: %zu\n", info.sections);

    auto out = fx::txt_write(doc);
    printf("Round-trip: %s\n",
           (out.size() == data.size() &&
            memcmp(out.data(), data.data(), data.size()) == 0)
               ? "byte-identical" : "MISMATCH (report this)");
    return 0;
}

int cmd_mt(int argc, char** argv) {
    if (argc < 3) { usage_mt(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_mt_info(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_mt();
    return 1;
}
