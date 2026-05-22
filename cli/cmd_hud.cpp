#include "fx/hud.h"
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

// ft hud dump <file.HUD>
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft hud dump <file.HUD>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    HudFile hud = hud_parse(data.data(), data.size());
    if (!hud.valid) {
        fprintf(stderr, "Not a valid HUD file: %s\n", argv[1]);
        return 1;
    }

    printf("{\n");
    printf("  \"file\": \"%s\",\n", argv[1]);

    printf("  \"asset_strings\": [");
    for (size_t i = 0; i < hud.asset_strings.size(); i++) {
        if (i) printf(", ");
        printf("\"%s\"", hud.asset_strings[i].c_str());
    }
    printf("],\n");

    printf("  \"advisory_icons\": {\n");
    printf("    \"A\": \"%s\",\n", hud.icon_a.c_str());
    printf("    \"B\": \"%s\",\n", hud.icon_b.c_str());
    printf("    \"C\": \"%s\",\n", hud.icon_c.c_str());
    printf("    \"D\": \"%s\"\n",  hud.icon_d.c_str());
    printf("  },\n");

    printf("  \"params\": [\n");
    for (size_t i = 0; i < hud.params.size(); i++) {
        const auto& p = hud.params[i];
        bool last = (i + 1 == hud.params.size());
        printf("    {\"gauge\": \"%s\", \"field\": \"%s\", \"value\": %d}%s\n",
               p.gauge.c_str(), p.field.c_str(), (int)p.value, last ? "" : ",");
    }
    printf("  ]\n");
    printf("}\n");
    return 0;
}

int cmd_hud(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft hud <dump> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    fprintf(stderr, "Unknown hud subcommand: %s\n", argv[1]);
    return 1;
}
