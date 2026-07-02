#include "fx/pe.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

static std::string xmi_name(uint8_t idx) {
    char buf[32];
    if (idx == 1) return "VALK01.XMI";
    snprintf(buf, sizeof(buf), "AIR%03d.XMI", (int)idx);
    return buf;
}

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

    fx::CodeSection cs = fx::pe_code_section(buf.data(), buf.size());
    if (!cs.data) { fprintf(stderr, "No CODE section in: %s\n", path); return 1; }

    puts("{");
    printf("  \"file\": \"%s\",\n", path);
    puts("  \"opcodes\": [");

    const uint8_t* p = cs.data;
    const uint8_t* end = cs.data + cs.size;
    bool first = true;

    while (p < end) {
        uint8_t op = *p;

        // Stop if we hit zero padding at end
        if (op == 0x00) break;

        // F9 is a section end marker â€” skip silently
        if (op == 0xF9) { ++p; continue; }

        if (!first) puts(",");
        first = false;
        printf("    ");

        if (op == 0xFF) {
            // FF <name\0> â€” playlist identifier
            ++p;
            std::string name;
            while (p < end && *p) name += (char)(*p++);
            if (p < end) ++p;  // consume NUL
            printf("{\"op\": \"FF\", \"playlist_id\": \"%s\"}", name.c_str());
            continue;
        }

        if (op == 0xFA && p + 5 < end) {
            // FA <sub> <u32> â€” setup/config
            uint8_t sub = p[1];
            uint32_t val = (uint32_t)(p[2]) | ((uint32_t)p[3] << 8) |
                           ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24);
            printf("{\"op\": \"FA\", \"sub\": \"0x%02X\", \"value\": %u}", sub, val);
            p += 6;
            continue;
        }

        if (op == 0xFB && p + 2 < end) {
            // FB <mode> <idx> [F9] â€” play track
            uint8_t mode = p[1];
            uint8_t idx  = p[2];
            std::string xmi = xmi_name(idx);
            if (p + 3 < end && p[3] == 0xF9) {
                printf("{\"op\": \"FB\", \"mode\": \"0x%02X\", \"track_idx\": %d, \"xmi\": \"%s\"}",
                       mode, (int)idx, xmi.c_str());
                p += 4;
            } else {
                printf("{\"op\": \"FB\", \"mode\": \"0x%02X\", \"track_idx\": %d, \"xmi\": \"%s\"}",
                       mode, (int)idx, xmi.c_str());
                p += 3;
            }
            continue;
        }

        if (op == 0xFC) {
            // FC â€” shuffle/loop marker; skip following dispatch table bytes
            printf("{\"op\": \"FC\"}");
            ++p;
            // Skip the dispatch table: 01 02 03 02 01 02 03 02 01 (9 bytes)
            if (p + 9 <= end &&
                p[0]==0x01 && p[1]==0x02 && p[2]==0x03 && p[3]==0x02 &&
                p[4]==0x01 && p[5]==0x02 && p[6]==0x03 && p[7]==0x02 && p[8]==0x01) {
                p += 9;
            }
            continue;
        }

        if (op == 0xFE && p + 4 < end) {
            // FE <u32> â€” conditional branch; same dispatch table may follow
            uint32_t val = (uint32_t)(p[1]) | ((uint32_t)p[2] << 8) |
                           ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
            printf("{\"op\": \"FE\", \"state\": \"0x%08X\"}", val);
            p += 5;
            // Skip optional dispatch table: 01 02 03 02 01 02 03 02 01 (9 bytes)
            if (p + 9 <= end &&
                p[0]==0x01 && p[1]==0x02 && p[2]==0x03 && p[3]==0x02 &&
                p[4]==0x01 && p[5]==0x02 && p[6]==0x03 && p[7]==0x02 && p[8]==0x01) {
                p += 9;
            }
            continue;
        }

        if (op == 0xFD && p + 3 < end) {
            // FD <u24> â€” loop/jump
            uint32_t val = (uint32_t)(p[1]) | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16);
            printf("{\"op\": \"FD\", \"target\": \"0x%06X\"}", val);
            p += 4;
            continue;
        }

        // Unrecognised byte â€” emit raw and stop
        printf("{\"op\": \"??\", \"byte\": \"0x%02X\"}", op);
        ++p;
        break;
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
