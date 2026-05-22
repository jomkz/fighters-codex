#include "fx/brf.h"
#include "fx/ot.h"
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

// Determine the format name from file extension (lower-cased, no dot)
static std::string ext_format(const char* path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    return ext;
}

// ot/pt/nt/jt/see/ecm/gas  info  <file>
static int cmd_info(int argc, char** argv, const char* /*format_hint*/) {
    if (argc < 2) { fprintf(stderr, "Usage: ft <ot|pt|jt|...> info <file>\n"); return 1; }

    const char* src = argv[1];
    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    BrfDoc doc = brf_parse(data.data(), data.size());
    if (doc.fields.empty()) {
        fprintf(stderr, "Not a valid BRF file: %s\n", src);
        return 1;
    }

    std::string fmt = ext_format(src);
    printf("File: %s  (%zu fields, %zu tables)\n\n", src, doc.fields.size(), doc.tables.size());
    brf_print_info(doc, fmt.c_str());
    return 0;
}

// ot/pt/...  unpack  <file>  [-o out.txt]
// Since BRF is already plain text, unpack just copies the file.
static int cmd_unpack(int argc, char** argv, const char* /*format_hint*/) {
    if (argc < 2) { fprintf(stderr, "Usage: ft <ot|pt|...> unpack <file> [-o out.txt]\n"); return 1; }

    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    BrfDoc doc = brf_parse(data.data(), data.size());
    auto out = brf_serialize(doc);

    std::string out_path = dst ? dst : (fs::path(src).string() + ".txt");
    if (!write_file(out_path.c_str(), out)) {
        fprintf(stderr, "Cannot write: %s\n", out_path.c_str());
        return 1;
    }
    printf("%s -> %s (%zu bytes, %zu fields)\n", src, out_path.c_str(), out.size(), doc.fields.size());
    return 0;
}

// ot/pt/...  pack  <in.txt>  -o <out>
// Validates the file is parseable BRF, then writes it out.
static int cmd_pack(int argc, char** argv, const char* /*format_hint*/) {
    if (argc < 4) { fprintf(stderr, "Usage: ft <ot|pt|...> pack <in.txt> -o <out>\n"); return 1; }

    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];
    if (!dst) { fprintf(stderr, "Missing -o output path\n"); return 1; }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    BrfDoc doc = brf_parse(data.data(), data.size());
    if (doc.fields.empty()) {
        fprintf(stderr, "Not a valid BRF file: %s\n", src);
        return 1;
    }
    auto out = brf_serialize(doc);

    if (!write_file(dst, out)) { fprintf(stderr, "Cannot write: %s\n", dst); return 1; }
    printf("%s -> %s (%zu bytes, %zu fields)\n", src, dst, out.size(), doc.fields.size());
    return 0;
}

// Generic dispatcher: called with the command name (ot/pt/nt/jt/see/ecm/gas)
int cmd_ot(int argc, char** argv, const char* format) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft %s <info|unpack|pack> ...\n", format);
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1, format);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1, format);
    if (strcmp(sub, "pack")   == 0) return cmd_pack(argc - 1, argv + 1, format);
    fprintf(stderr, "Unknown %s subcommand: %s\n", format, sub);
    return 1;
}
