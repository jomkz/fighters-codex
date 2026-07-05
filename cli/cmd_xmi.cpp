#include "fx/xmi.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void usage_xmi() {
    puts("Usage:");
    puts("  fx xmi info   <file.XMI>                  # sequences, timbres, chunks");
    puts("  fx xmi export <file.XMI> [-s N] -o out.mid   # sequence N (default 0) -> Standard MIDI");
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

static bool write_all(const char* path, const std::vector<uint8_t>& data) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    return ok;
}

static int cmd_xmi_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    auto f = fx::xmi_parse(data.data(), data.size());
    if (!f.valid) {
        fprintf(stderr, "Not an XMI (missing FORM/XDIR): %s\n", path);
        return 1;
    }
    printf("XMI: %u sequence(s) declared, %zu parsed\n",
           f.seq_count, f.sequences.size());
    for (size_t i = 0; i < f.sequences.size(); i++) {
        const auto& seq = f.sequences[i];
        printf("  seq %zu: %u timbre(s),", i, seq.timbres);
        for (const auto& c : seq.chunks) printf(" %s(%u)", c.tag.c_str(), c.size);
        printf("\n");
    }
    return 0;
}

static int cmd_xmi_export(int argc, char** argv) {
    const char* in = argv[0];
    const char* out = nullptr;
    size_t seq = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) seq = (size_t)atoi(argv[++i]);
    }
    if (!out) { fprintf(stderr, "Missing -o <output.mid>\n"); return 1; }

    auto data = read_all(in);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", in); return 1; }

    auto smf = fx::xmi_to_smf(data.data(), data.size(), seq);
    if (smf.empty()) {
        fprintf(stderr, "Export failed (no sequence %zu or no EVNT chunk): %s\n",
                seq, in);
        return 1;
    }
    if (!write_all(out, smf)) { fprintf(stderr, "Cannot write: %s\n", out); return 1; }
    printf("Wrote %s (%zu bytes) from %s sequence %zu\n", out, smf.size(), in, seq);
    return 0;
}

int cmd_xmi(int argc, char** argv) {
    if (argc < 3) { usage_xmi(); return 1; }
    if (strcmp(argv[1], "info")   == 0) return cmd_xmi_info(argv[2]);
    if (strcmp(argv[1], "export") == 0) return cmd_xmi_export(argc - 2, argv + 2);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_xmi();
    return 1;
}
