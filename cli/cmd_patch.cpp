#include "fx/rtpatch.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;
namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    return f.good();
}

// The .rtp container is an overlay in the patch .exe: find the "K*" header whose
// version and flags parse. The updater has exactly one.
size_t find_container(const std::vector<uint8_t>& exe) {
    for (size_t i = 0; i + 8 < exe.size(); ++i) {
        if (exe[i] == 'K' && exe[i + 1] == '*') {
            RtpPatch p = rtp_read(exe.data() + i, exe.size() - i);
            if (!p.records.empty()) return i;
        }
    }
    return exe.size();
}

const char* mode_name(RtpMode m) { return m == RtpMode::New ? "new" : "modify"; }

// fx patch inspect <patch.exe|.rtp>
int cmd_inspect(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx patch inspect <patch.exe>\n"); return 1; }
    auto exe = read_file(argv[1]);
    if (exe.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }
    size_t off = find_container(exe);
    if (off >= exe.size()) { fprintf(stderr, "No RTPatch container found in %s\n", argv[1]); return 1; }

    RtpPatch p = rtp_read(exe.data() + off, exe.size() - off);
    printf("container:  RTPatch v0x%04x%s @ offset 0x%zx\n", p.version,
           p.extra_mode ? " (extra mode)" : "", off);
    if (!p.specials.empty()) {
        printf("system files (installer-prompted):\n");
        for (auto& s : p.specials) printf("  %-14s  %s\n", s.first.c_str(), s.second.c_str());
    }
    printf("\n%-14s  %-6s  %12s  %12s  %s\n", "File", "Mode", "Source", "Target", "Source checksum");
    for (auto& r : p.records) {
        printf("%-14s  %-6s  %12u  %12u  w1=%08x w2=%08x\n", r.name.c_str(),
               mode_name(r.mode), r.src_size, r.dst_size, r.src_w1, r.src_w2);
    }
    printf("\n%zu record(s)\n", p.records.size());
    return 0;
}

// fx patch apply <patch.exe> --source <dir> --out <dir> [--file NAME] [--no-checksum]
int cmd_apply(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx patch apply <patch.exe> --source <dir> --out <dir>\n"
                        "         [--file NAME] [--no-checksum]\n");
        return 1;
    }
    std::string patch = argv[1], source, out;
    std::string only;
    bool verify = true;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) source = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
        else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) only = argv[++i];
        else if (strcmp(argv[i], "--no-checksum") == 0) verify = false;
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }
    if (source.empty() || out.empty()) {
        fprintf(stderr, "Both --source and --out are required\n");
        return 1;
    }
    auto exe = read_file(patch);
    if (exe.empty()) { fprintf(stderr, "Cannot read: %s\n", patch.c_str()); return 1; }
    size_t off = find_container(exe);
    if (off >= exe.size()) { fprintf(stderr, "No RTPatch container found in %s\n", patch.c_str()); return 1; }
    RtpPatch p = rtp_read(exe.data() + off, exe.size() - off);
    const uint8_t* pd = exe.data() + off;
    size_t ps = exe.size() - off;

    fs::create_directories(out);
    int done = 0, skipped = 0, failed = 0;

    // Case-insensitive lookup of the source file matching a record.
    auto find_src = [&](const std::string& name) -> std::string {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(source, ec)) {
            if (!e.is_regular_file()) continue;
            std::string fn = e.path().filename().string();
            if (fn.size() == name.size() &&
                std::equal(fn.begin(), fn.end(), name.begin(),
                           [](char a, char b) { return toupper((unsigned char)a) == toupper((unsigned char)b); }))
                return e.path().string();
        }
        return "";
    };

    for (auto& r : p.records) {
        if (!only.empty() && r.name != only) continue;
        std::vector<uint8_t> src;
        if (r.mode == RtpMode::Modify) {
            std::string sp = find_src(r.name);
            if (sp.empty()) {
                printf("  [skip]  %-14s  no source file in %s\n", r.name.c_str(), source.c_str());
                skipped++;
                continue;
            }
            src = read_file(sp);
            if (verify && (rtp_checksum(src.data(), src.size(), 31) != r.src_w1 ||
                           rtp_checksum(src.data(), src.size(), 30) != r.src_w2)) {
                printf("  [skip]  %-14s  source checksum mismatch (wrong version; "
                       "--no-checksum to force)\n", r.name.c_str());
                skipped++;
                continue;
            }
        }
        std::vector<uint8_t> result =
            rtp_reconstruct(pd, ps, r, src.empty() ? nullptr : src.data(), src.size(), false);
        if (result.size() != r.dst_size) {
            printf("  [FAIL]  %-14s  produced %zu bytes, expected %u\n", r.name.c_str(),
                   result.size(), r.dst_size);
            failed++;
            continue;
        }
        std::string dest = (fs::path(out) / r.name).string();
        if (!write_file(dest, result)) {
            printf("  [FAIL]  %-14s  cannot write %s\n", r.name.c_str(), dest.c_str());
            failed++;
            continue;
        }
        printf("  [ ok ]  %-14s  %u bytes\n", r.name.c_str(), r.dst_size);
        done++;
    }
    printf("\n%d patched, %d skipped, %d failed\n", done, skipped, failed);
    return failed ? 1 : 0;
}

} // namespace

int cmd_patch(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx patch <inspect|apply> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "inspect") == 0) return cmd_inspect(argc - 1, argv + 1);
    if (strcmp(sub, "apply") == 0) return cmd_apply(argc - 1, argv + 1);
    fprintf(stderr, "Unknown patch subcommand: %s\n", sub);
    return 1;
}
