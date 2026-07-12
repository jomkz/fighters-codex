#include "fx/esa.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

static bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    return f.good();
}

// esa ls <SETUP.ESA>
static int cmd_ls(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx esa ls <SETUP.ESA>\n"); return 1; }
    auto esa = read_file(argv[1]);
    if (esa.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    auto dir = esa_read_dir(esa.data(), esa.size());
    if (dir.empty()) { fprintf(stderr, "Not a valid ESA archive: %s\n", argv[1]); return 1; }

    printf("%-22s  %-23s  %-6s  %-4s  %10s  %10s\n",
           "Name", "Label", "Flags", "Mthd", "Usize", "Csize");
    for (auto& e : dir)
        printf("%-22s  %-23s  0x%04x  %-4s  %10u  %10u\n",
               e.name.c_str(), e.label.c_str(), e.flags, e.method.c_str(),
               e.usize, e.csize);
    printf("\n%zu file(s)\n", dir.size());
    return 0;
}

// esa info <SETUP.ESA>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx esa info <SETUP.ESA>\n"); return 1; }
    auto esa = read_file(argv[1]);
    if (esa.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    auto dir = esa_read_dir(esa.data(), esa.size());
    if (dir.empty()) { fprintf(stderr, "Not a valid ESA archive: %s\n", argv[1]); return 1; }

    size_t pkwa = 0, null = 0; uint64_t utotal = 0;
    for (auto& e : dir) { (e.method == "PKWA" ? pkwa : null)++; utotal += e.usize; }
    bool identical = esa_repack(esa.data(), esa.size()) == esa;

    printf("archive:     %s\n", argv[1]);
    printf("size:        %zu bytes\n", esa.size());
    printf("directory:   %zu bytes\n", esa_dir_size(esa.data(), esa.size()));
    printf("entries:     %zu  (%zu PKWA, %zu NULL)\n", dir.size(), pkwa, null);
    printf("uncompressed:%llu bytes\n", (unsigned long long)utotal);
    printf("repack:      %s\n", identical ? "byte-identical" : "NORMALISED (non-contiguous source)");
    return 0;
}

// esa extract <SETUP.ESA> <NAME> [NAME ...] [-o dir]
static int cmd_extract(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: fx esa extract <SETUP.ESA> <NAME> [NAME ...] [-o dir]\n");
        return 1;
    }
    auto esa = read_file(argv[1]);
    if (esa.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    std::string out_dir = ".";
    std::vector<const char*> names;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_dir = argv[++i];
        else names.push_back(argv[i]);
    }
    if (names.empty()) { fprintf(stderr, "No file names specified\n"); return 1; }

    auto dir = esa_read_dir(esa.data(), esa.size());
    if (dir.empty()) { fprintf(stderr, "Not a valid ESA archive: %s\n", argv[1]); return 1; }
    fs::create_directories(out_dir);

    int ok = 0, fail = 0;
    for (const char* name : names) {
        const EsaEntry* e = esa_find(dir, name);
        if (!e) { fprintf(stderr, "  NOT FOUND: %s\n", name); fail++; continue; }
        bool unsupported = false;
        auto data = esa_extract(esa.data(), esa.size(), *e, true, &unsupported);
        if (data.empty() && e->usize > 0) {
            fprintf(stderr, "  SKIP %s (%s, %s)\n", e->name.c_str(), e->method.c_str(),
                    unsupported ? "unsupported" : "decode error");
            fail++; continue;
        }
        std::string path = (fs::path(out_dir) / esa_safe_name(e->name)).string();
        if (!write_file(path, data)) { fprintf(stderr, "  WRITE FAIL: %s\n", path.c_str()); fail++; continue; }
        printf("  %s (%u bytes)\n", e->name.c_str(), e->usize);
        ok++;
    }
    printf("\n%d extracted, %d failed\n", ok, fail);
    return fail ? 1 : 0;
}

// esa unpack <SETUP.ESA> [-o dir]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx esa unpack <SETUP.ESA> [-o dir]\n"); return 1; }
    auto esa = read_file(argv[1]);
    if (esa.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    std::string out_dir = ".";
    for (int i = 2; i < argc; ++i)
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_dir = argv[++i];

    auto dir = esa_read_dir(esa.data(), esa.size());
    if (dir.empty()) { fprintf(stderr, "Not a valid ESA archive: %s\n", argv[1]); return 1; }
    fs::create_directories(out_dir);

    int ok = 0, fail = 0;
    for (auto& e : dir) {
        bool unsupported = false;
        auto data = esa_extract(esa.data(), esa.size(), e, true, &unsupported);
        if (data.empty() && e.usize > 0) {
            fprintf(stderr, "  SKIP %s (%s)\n", e.name.c_str(), e.method.c_str());
            fail++; continue;
        }
        std::string path = (fs::path(out_dir) / esa_safe_name(e.name)).string();
        if (!write_file(path, data)) { fail++; continue; }
        ok++;
    }
    printf("%d extracted, %d failed\n", ok, fail);
    return fail ? 1 : 0;
}

// esa repack <SETUP.ESA> <out.ESA>
static int cmd_repack(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "Usage: fx esa repack <SETUP.ESA> <out.ESA>\n"); return 1; }
    auto esa = read_file(argv[1]);
    if (esa.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    auto out = esa_repack(esa.data(), esa.size());
    if (out.empty()) { fprintf(stderr, "Not a valid ESA archive: %s\n", argv[1]); return 1; }
    if (!write_file(argv[2], out)) { fprintf(stderr, "Cannot write: %s\n", argv[2]); return 1; }
    printf("%s -> %s (%zu bytes, %s)\n", argv[1], argv[2], out.size(),
           out == esa ? "byte-identical" : "normalised");
    return 0;
}

// esa pack <out.ESA> <LABEL> <file> [<file> ...]
// Every file becomes a stored entry under LABEL, keyed by its basename. Enough
// to synthesise a fixture archive; the game's own PKWA compressor is not ours.
static int cmd_pack(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: fx esa pack <out.ESA> <LABEL> <file> [file ...]\n");
        return 1;
    }
    const char* out_path = argv[1];
    std::string label = argv[2];
    std::vector<EsaInput> files;
    for (int i = 3; i < argc; ++i) {
        auto data = read_file(argv[i]);
        if (data.empty() && fs::file_size(argv[i]) != 0) {
            fprintf(stderr, "Cannot read: %s\n", argv[i]); return 1;
        }
        files.push_back({fs::path(argv[i]).filename().string(), label, 0x211, 0, std::move(data)});
    }
    auto esa = esa_build(files);
    if (!write_file(out_path, esa)) { fprintf(stderr, "Cannot write: %s\n", out_path); return 1; }
    printf("%s: %zu entries, %zu bytes\n", out_path, files.size(), esa.size());
    return 0;
}

int cmd_esa(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx esa <ls|info|extract|unpack|repack|pack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "ls")      == 0) return cmd_ls(argc - 1, argv + 1);
    if (strcmp(sub, "info")    == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "extract") == 0) return cmd_extract(argc - 1, argv + 1);
    if (strcmp(sub, "unpack")  == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "repack")  == 0) return cmd_repack(argc - 1, argv + 1);
    if (strcmp(sub, "pack")    == 0) return cmd_pack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown esa subcommand: %s\n", sub);
    return 1;
}
