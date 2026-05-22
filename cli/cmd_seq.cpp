#include "fx/seq.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>

using namespace fx;
namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

static bool write_file(const char* path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    return f.good();
}

// seq dump <file.SEQ>
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft seq dump <file.SEQ>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    auto seq = seq_parse(data.data(), data.size());

    int ev_idx = 0;
    for (size_t i = 0; i < seq.lines.size(); ++i) {
        bool is_ev = (i < seq.is_event.size()) && seq.is_event[i];
        if (!is_ev) {
            ++ev_idx;
            continue;
        }
        const auto& ev = seq.events[i];
        // Print: ticks | cmd | args
        char tick_buf[32];
        if (ev.relative)
            snprintf(tick_buf, sizeof(tick_buf), "+%d", ev.ticks);
        else
            snprintf(tick_buf, sizeof(tick_buf), "%d", ev.ticks);

        printf("%-8s", tick_buf);
        if (ev.sync) printf("[sync] ");
        printf("%-12s", ev.command.c_str());
        for (auto& a : ev.args) printf(" %s", a.c_str());
        printf("\n");
        ++ev_idx;
    }
    return 0;
}

// seq unpack <file.SEQ> [-o out.txt]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft seq unpack <file.SEQ> [-o out.txt]\n"); return 1; }

    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];

    std::string out_path;
    if (dst) {
        out_path = dst;
    } else {
        out_path = fs::path(src).stem().string() + ".seq.txt";
    }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    auto seq = seq_parse(data.data(), data.size());
    auto out = seq_serialize(seq);

    if (!write_file(out_path.c_str(), out)) {
        fprintf(stderr, "Cannot write: %s\n", out_path.c_str());
        return 1;
    }
    printf("%s -> %s (%zu bytes)\n", src, out_path.c_str(), out.size());
    return 0;
}

// seq pack <in.txt> -o <out.SEQ>
static int cmd_pack(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "Usage: ft seq pack <in.txt> -o <out.SEQ>\n"); return 1; }

    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];
    if (!dst) { fprintf(stderr, "Missing -o output path\n"); return 1; }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    auto seq = seq_parse(data.data(), data.size());
    auto out = seq_serialize(seq);

    if (!write_file(dst, out)) { fprintf(stderr, "Cannot write: %s\n", dst); return 1; }
    printf("%s -> %s (%zu bytes)\n", src, dst, out.size());
    return 0;
}

int cmd_seq(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft seq <dump|unpack|pack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "dump")   == 0) return cmd_dump(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "pack")   == 0) return cmd_pack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown seq subcommand: %s\n", sub);
    return 1;
}
