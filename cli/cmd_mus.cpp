#include "fx/mus.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_mus() {
    puts("Usage:");
    puts("  fx mus dump <file.MUS>");
}

static int cmd_mus_dump(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    fx::MusScript script = fx::mus_disassemble(buf.data(), buf.size());
    if (!script.valid) { fprintf(stderr, "No CODE section in: %s\n", path); return 1; }

    puts("{");
    printf("  \"file\": \"%s\",\n", path);
    puts("  \"opcodes\": [");

    bool first = true;
    auto sep = [&]() { if (!first) puts(","); first = false; printf("    "); };

    for (const auto& e : script.ops) {
        sep();
        switch (e.op) {
        case 0xFF:
            printf("{\"op\": \"FF\", \"playlist_id\": \"%s\"}", e.playlist_id.c_str());
            break;
        case 0xFA:
            printf("{\"op\": \"FA\", \"sub\": \"0x%02X\", \"value\": %u}", e.sub, e.value);
            break;
        case 0xFB:
            printf("{\"op\": \"FB\", \"mode\": \"0x%02X\", \"track_idx\": %d, \"xmi\": \"%s\"}",
                   e.mode, (int)e.track_idx, e.xmi.c_str());
            break;
        case 0xFC:
            printf("{\"op\": \"FC\"}");
            break;
        case 0xFE:
            printf("{\"op\": \"FE\", \"state\": \"0x%08X\"}", e.value);
            break;
        case 0xFD:
            printf("{\"op\": \"FD\", \"target\": \"0x%06X\"}", e.value);
            break;
        default:
            break;
        }
    }
    if (script.stopped_early) {
        sep();
        printf("{\"op\": \"??\", \"byte\": \"0x%02X\"}", script.stop_byte);
    }

    if (!first) putchar('\n');
    puts("  ]");
    puts("}");
    return 0;
}

int cmd_mus(int argc, char** argv) {
    if (argc < 2) { usage_mus(); return 1; }
    if (strcmp(argv[1], "dump") == 0) {
        if (argc < 3) { usage_mus(); return 1; }
        return cmd_mus_dump(argv[2]);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_mus();
    return 1;
}
