#include "fx/hud.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// fx hud dump <file.HUD>
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx hud dump <file.HUD>\n"); return 1; }
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

// fx hud set <file.HUD> <gauge.field=value | icon_a..icon_d=LABEL ...> [-o out.HUD]
static int cmd_set(int argc, char** argv) {
    const char* src = nullptr;
    const char* dst = nullptr;
    std::vector<const char*> edits;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) dst = argv[++i];
        else if (!src) src = argv[i];
        else edits.push_back(argv[i]);
    }
    if (!src || edits.empty()) {
        fprintf(stderr,
                "Usage: fx hud set <file.HUD> <gauge.field=value ...> [-o out.HUD]\n"
                "       keys: gauge.field (see fx hud dump) or icon_a..icon_d\n");
        return 1;
    }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }
    HudFile hud = hud_parse(data.data(), data.size());
    if (!hud.valid) { fprintf(stderr, "Not a valid HUD file: %s\n", src); return 1; }

    for (const char* e : edits) {
        const char* eq = strchr(e, '=');
        if (!eq || eq == e) { fprintf(stderr, "Bad edit (want key=value): %s\n", e); return 1; }
        std::string key(e, (size_t)(eq - e));
        std::string val(eq + 1);

        if (key.rfind("icon_", 0) == 0 && key.size() == 6 &&
            key[5] >= 'a' && key[5] <= 'd') {
            (key[5] == 'a' ? hud.icon_a : key[5] == 'b' ? hud.icon_b
             : key[5] == 'c' ? hud.icon_c : hud.icon_d) = val;
            continue;
        }
        size_t dot = key.find('.');
        if (dot == std::string::npos) { fprintf(stderr, "Unknown key: %s\n", key.c_str()); return 1; }
        std::string gauge = key.substr(0, dot), field = key.substr(dot + 1);
        bool found = false;
        for (auto& p : hud.params) {
            if (p.gauge == gauge && p.field == field) {
                // Not (int16_t): the tape gauges are 32-bit, and truncating here would
                // reintroduce the very corruption the widened codec fixes (#491).
                p.value = (int32_t)strtol(val.c_str(), nullptr, 10);
                found = true;
                break;
            }
        }
        if (!found) { fprintf(stderr, "Unknown key: %s\n", key.c_str()); return 1; }
    }

    auto out = hud_repack(data.data(), data.size(), hud);
    if (out.empty()) {
        fprintf(stderr, "Repack failed (out-of-range value or oversized icon label?)\n");
        return 1;
    }
    std::string out_path = dst ? dst : (std::string(src) + ".out");
    std::ofstream ofs(out_path, std::ios::binary);
    if (!ofs || !ofs.write((const char*)out.data(), (std::streamsize)out.size())) {
        fprintf(stderr, "Cannot write: %s\n", out_path.c_str());
        return 1;
    }
    printf("%s -> %s (%zu bytes, %zu edit(s))\n", src, out_path.c_str(),
           out.size(), edits.size());
    return 0;
}

int cmd_hud(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx hud <dump|set> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    if (strcmp(argv[1], "set") == 0) return cmd_set(argc - 1, argv + 1);
    fprintf(stderr, "Unknown hud subcommand: %s\n", argv[1]);
    return 1;
}
