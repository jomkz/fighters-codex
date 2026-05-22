#include "fx/ealib.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace fx;

namespace fs = std::filesystem;

// Read an entire file into a vector
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

// Replace characters illegal in Windows filenames with '_'.
// The game uses '&' as a prefix for looping audio files; Windows rejects it.
static std::string sanitize_filename(const char* name) {
    std::string s = name;
    for (char& c : s) {
        if (c == '&' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|')
            c = '_';
    }
    return s;
}

static const char* flag_name(uint8_t flags) {
    switch (flags) {
        case 0: return "raw";
        case 1: return "lzss";
        case 3: return "pxpk";
        case 4: return "dcl";
        default: return "?";
    }
}

// lib ls <file.LIB>
static int cmd_ls(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft lib ls <file.LIB>\n"); return 1; }
    auto lib = read_file(argv[1]);
    if (lib.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    auto entries = ealib_read_dir(lib.data(), lib.size());
    if (entries.empty()) { fprintf(stderr, "Not a valid EALIB: %s\n", argv[1]); return 1; }

    printf("%-13s  %-5s  %8s\n", "Name", "Flags", "Size");
    printf("%-13s  %-5s  %8s\n", "-------------", "-----", "--------");
    for (auto& e : entries)
        printf("%-13s  %-5s  %8u\n", e.name, flag_name(e.flags), e.size);
    printf("\n%d file(s)\n", (int)entries.size());
    return 0;
}

// lib extract <file.LIB> <NAME> [NAME ...] [-o output_dir]
static int cmd_extract(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: ft lib extract <file.LIB> <NAME> [NAME ...] [-o dir]\n");
        return 1;
    }
    auto lib = read_file(argv[1]);
    if (lib.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    const char* out_path = ".";
    std::vector<const char*> names;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { out_path = argv[++i]; }
        else names.push_back(argv[i]);
    }
    if (names.empty()) { fprintf(stderr, "No file names specified\n"); return 1; }

    fs::create_directories(out_path);
    auto entries = ealib_read_dir(lib.data(), lib.size());
    if (entries.empty()) { fprintf(stderr, "Not a valid EALIB: %s\n", argv[1]); return 1; }

    int ok = 0, fail = 0;
    for (const char* name : names) {
        auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) {
            return _stricmp(e.name, name) == 0;
        });
        if (it == entries.end()) {
            fprintf(stderr, "  NOT FOUND: %s\n", name);
            fail++;
            continue;
        }
        auto data = ealib_extract(lib.data(), lib.size(), *it, true);
        if (data.empty() && it->size > 0) {
            fprintf(stderr, "  SKIP %s (flags=%d, unsupported)\n", it->name, it->flags);
            fail++;
            continue;
        }
        fs::path dest = fs::path(out_path) / sanitize_filename(it->name);
        if (write_file(dest.string().c_str(), data)) {
            printf("  %s -> %s (%zu bytes)\n", it->name, dest.string().c_str(), data.size());
            ok++;
        } else {
            fprintf(stderr, "  FAIL %s: write error\n", it->name);
            fail++;
        }
    }
    printf("\n%d extracted, %d failed\n", ok, fail);
    return fail ? 1 : 0;
}

// lib unpack <file.LIB> [output_dir]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft lib unpack <file.LIB> [output_dir]\n"); return 1; }
    auto lib = read_file(argv[1]);
    if (lib.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    fs::path out_dir = (argc >= 3) ? argv[2] : fs::path(argv[1]).stem();
    fs::create_directories(out_dir);

    auto entries = ealib_read_dir(lib.data(), lib.size());
    if (entries.empty()) { fprintf(stderr, "Not a valid EALIB: %s\n", argv[1]); return 1; }

    int ok = 0, fail = 0;
    for (auto& e : entries) {
        auto data = ealib_extract(lib.data(), lib.size(), e, true);
        if (data.empty() && e.size > 0) {
            fprintf(stderr, "  SKIP %s (flags=%d, unsupported)\n", e.name, e.flags);
            fail++;
            continue;
        }
        fs::path dest = out_dir / sanitize_filename(e.name);
        if (write_file(dest.string().c_str(), data)) {
            printf("  %s -> %s (%zu bytes)\n", e.name, dest.string().c_str(), data.size());
            ok++;
        } else {
            fprintf(stderr, "  FAIL %s: write error\n", e.name);
            fail++;
        }
    }
    printf("\n%d extracted, %d failed\n", ok, fail);
    return fail ? 1 : 0;
}

// lib pack <dir> <output.LIB>
static int cmd_pack(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "Usage: ft lib pack <dir> <output.LIB>\n"); return 1; }
    fs::path src_dir = argv[1];
    if (!fs::is_directory(src_dir)) { fprintf(stderr, "Not a directory: %s\n", argv[1]); return 1; }

    std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
    for (auto& entry : fs::directory_iterator(src_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.size() > 12) { fprintf(stderr, "  WARN: '%s' exceeds 12 chars, truncating\n", name.c_str()); name.resize(12); }
        auto data = read_file(entry.path().string().c_str());
        printf("  + %s (%zu bytes)\n", name.c_str(), data.size());
        files.push_back({ name, std::move(data) });
    }

    auto lib = ealib_build(files);
    if (!write_file(argv[2], lib)) { fprintf(stderr, "Cannot write: %s\n", argv[2]); return 1; }
    printf("\nWrote %s (%zu bytes, %d files)\n", argv[2], lib.size(), (int)files.size());
    return 0;
}

// lib patch <src.LIB> <name> <file> <output.LIB>
static int cmd_patch(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: ft lib patch <src.LIB> <name> <file> <output.LIB>\n");
        return 1;
    }
    auto lib = read_file(argv[1]);
    if (lib.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    auto patch_data = read_file(argv[3]);
    if (patch_data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[3]); return 1; }

    auto out_lib = ealib_patch(lib.data(), lib.size(), argv[2], patch_data);
    if (out_lib.empty()) { fprintf(stderr, "Patch failed (entry '%s' not found?)\n", argv[2]); return 1; }
    if (!write_file(argv[4], out_lib)) { fprintf(stderr, "Cannot write: %s\n", argv[4]); return 1; }

    printf("Patched '%s' -> %s (%zu bytes)\n", argv[2], argv[4], out_lib.size());
    return 0;
}

int cmd_lib(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft lib <ls|unpack|extract|pack|patch> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "ls")      == 0) return cmd_ls(argc - 1, argv + 1);
    if (strcmp(sub, "unpack")  == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "extract") == 0) return cmd_extract(argc - 1, argv + 1);
    if (strcmp(sub, "pack")    == 0) return cmd_pack(argc - 1, argv + 1);
    if (strcmp(sub, "patch")   == 0) return cmd_patch(argc - 1, argv + 1);
    fprintf(stderr, "Unknown lib subcommand: %s\n", sub);
    return 1;
}
