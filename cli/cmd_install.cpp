#include "fx/install.h"
#include "fx/rtpatch.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;
namespace fs = std::filesystem;

namespace {

struct Args {
    std::vector<std::string> discs;
    std::string              dest;
    std::string              patch;   // 1.02F updater (fae102.exe), if patching
    InstallOptions           opt;
    bool                     verify = false;
    bool                     json   = false;
    bool                     ok     = true;
};

Args parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; i++) {
        const char* s = argv[i];
        if (strcmp(s, "-d") == 0 && i + 1 < argc)             a.dest = argv[++i];
        else if (strcmp(s, "--patch") == 0 && i + 1 < argc)   a.patch = argv[++i];
        else if (strcmp(s, "--full") == 0)                    a.opt.full = true;
        else if (strcmp(s, "--minimal") == 0)                 a.opt.full = false;
        else if (strcmp(s, "--overwrite") == 0)               a.opt.overwrite = true;
        else if (strcmp(s, "--no-cd-resident") == 0)          a.opt.cd_resident = false;
        else if (strcmp(s, "--any-media") == 0)               a.opt.allow_unknown_media = true;
        else if (strcmp(s, "--verify") == 0)                  a.verify = true;
        else if (strcmp(s, "--json") == 0)                    a.json = true;
        else if (s[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", s);
            a.ok = false;
        } else {
            a.discs.push_back(s);
        }
    }
    if (a.discs.empty()) {
        fprintf(stderr, "No disc directory given\n");
        a.ok = false;
    }
    return a;
}

const char* status_name(InstallStatus s) {
    switch (s) {
        case InstallStatus::Copy:         return "copy";
        case InstallStatus::KeepExisting: return "keep";
        case InstallStatus::SkipSysfile:  return "skip";
    }
    return "?";
}

// Under --json, stdout carries the plan and nothing else — fxe's first-run parses
// it — so the scan banner goes to stderr alongside the diagnostics.
std::vector<DiscSource> scan_all(const Args& a) {
    std::vector<DiscSource> discs;
    FILE* out = a.json ? stderr : stdout;
    for (const auto& root : a.discs) {
        DiscSource d = install_scan(root);
        if (d.disc) fprintf(out, "disc %d: %s\n", d.disc, root.c_str());
        else {
            fprintf(out, "disc ?: %s\n", root.c_str());
            fprintf(stderr, "  not an FA disc (no SETUP.ESA + SETUP.SSF, no LIBs)\n");
        }
        discs.push_back(std::move(d));
    }
    return discs;
}

// JSON: what fxe's first-run reads. Strings here are file names and notes, so
// escaping quotes and backslashes covers it.
void json_string(const std::string& s) {
    putchar('"');
    for (char c : s) {
        if (c == '"' || c == '\\') printf("\\%c", c);
        else if ((unsigned char)c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

void print_json(const InstallPlan& plan, const std::vector<DiscSource>& discs) {
    printf("{\n  \"build\": \"%s\",\n", install_build_name(plan.build));
    printf("  \"script\": ");    json_string(plan.script);
    printf(",\n  \"app_name\": "); json_string(plan.app_name);
    printf(",\n  \"default_path\": "); json_string(plan.default_path);
    printf(",\n  \"bytes\": %llu,\n", (unsigned long long)plan.bytes);

    printf("  \"items\": [\n");
    for (size_t i = 0; i < plan.items.size(); i++) {
        const InstallItem& it = plan.items[i];
        printf("    {\"dest\": ");   json_string(it.dest);
        printf(", \"status\": \"%s\"", status_name(it.status));
        printf(", \"origin\": \"%s\"",
               it.origin == InstallOrigin::Archive ? "archive" : "loose");
        printf(", \"disc\": ");
        json_string(it.disc < discs.size() ? discs[it.disc].root : std::string());
        printf(", \"source\": "); json_string(it.source);
        printf(", \"bytes\": %llu", (unsigned long long)it.bytes);
        if (!it.note.empty()) { printf(", \"note\": "); json_string(it.note); }
        printf("}%s\n", i + 1 < plan.items.size() ? "," : "");
    }
    printf("  ],\n  \"directives\": [\n");
    for (size_t i = 0; i < plan.directives.size(); i++) {
        const InstallDirective& d = plan.directives[i];
        printf("    {\"script\": ");  json_string(d.script);
        printf(", \"keyword\": ");    json_string(d.keyword);
        printf(", \"honored\": %s",   d.honored ? "true" : "false");
        if (!d.note.empty()) { printf(", \"note\": "); json_string(d.note); }
        printf("}%s\n", i + 1 < plan.directives.size() ? "," : "");
    }
    printf("  ],\n  \"errors\": [\n");
    for (size_t i = 0; i < plan.errors.size(); i++) {
        printf("    "); json_string(plan.errors[i]);
        printf("%s\n", i + 1 < plan.errors.size() ? "," : "");
    }
    printf("  ]\n}\n");
}

void print_plan(const InstallPlan& plan) {
    printf("\nmedia:   %s", install_build_name(plan.build));
    if (plan.build == MediaBuild::V100F)
        printf("  (the 1.02F patch is not applied; the symbol database describes 1.02F)");
    printf("\nscript:  %s\n", plan.script.c_str());
    if (!plan.default_path.empty())
        printf("default: %s\n", plan.default_path.c_str());

    size_t copy = 0, keep = 0, skip = 0;
    printf("\n%-6s  %-24s  %10s  %s\n", "Action", "File", "Bytes", "Source");
    for (const auto& it : plan.items) {
        printf("%-6s  %-24s  %10llu  %s\n", status_name(it.status), it.dest.c_str(),
               (unsigned long long)it.bytes,
               it.origin == InstallOrigin::Archive ? it.label.c_str() : "(loose)");
        if (!it.note.empty()) printf("        %s\n", it.note.c_str());
        switch (it.status) {
            case InstallStatus::Copy:         copy++; break;
            case InstallStatus::KeepExisting: keep++; break;
            case InstallStatus::SkipSysfile:  skip++; break;
        }
    }

    printf("\n%zu to copy (%llu bytes), %zu kept, %zu skipped\n", copy,
           (unsigned long long)plan.bytes, keep, skip);

    size_t unhonored = 0;
    for (const auto& d : plan.directives) if (!d.honored) unhonored++;
    if (unhonored) {
        printf("\n%zu script directive(s) not acted on:\n", unhonored);
        for (const auto& d : plan.directives) {
            if (d.honored) continue;
            printf("  %-16s %s:%zu  %s\n", d.keyword.c_str(), d.script.c_str(),
                   d.line + 1, d.note.c_str());
        }
    }
    for (const auto& e : plan.errors) fprintf(stderr, "\nerror: %s\n", e.c_str());
}

void progress(const InstallItem& item, uint64_t done, uint64_t total, void*) {
    const int pct = total ? (int)((done * 100) / total) : 100;
    printf("  [%3d%%] %s (%llu bytes)\n", pct, item.dest.c_str(),
           (unsigned long long)item.bytes);
    fflush(stdout);
}

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

// Case-insensitive lookup of an installed file (the installer upper-cases names,
// but a mounted source may not, so match either way).
std::string find_installed(const std::string& dest, const std::string& name) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dest, ec)) {
        if (!e.is_regular_file()) continue;
        std::string fn = e.path().filename().string();
        if (fn.size() == name.size() &&
            std::equal(fn.begin(), fn.end(), name.begin(), [](char x, char y) {
                return toupper((unsigned char)x) == toupper((unsigned char)y);
            }))
            return e.path().string();
    }
    return "";
}

// Apply the RTPatch payload in `patch_file` to the just-installed 1.00F tree,
// in place → 1.02F. Each file is checked against the patch's source checksum
// first, so a tree that is already 1.02F (or otherwise not the expected 1.00F)
// is left untouched. Returns false on a hard error (a produced file that does
// not match its declared size, or a failed write).
bool apply_patch(const std::string& patch_file, const std::string& dest, FILE* out) {
    auto exe = read_file(patch_file);
    if (exe.empty()) {
        fprintf(stderr, "error: cannot read patch %s\n", patch_file.c_str());
        return false;
    }
    size_t off = exe.size();
    for (size_t i = 0; i + 8 < exe.size(); ++i) {
        if (exe[i] == 'K' && exe[i + 1] == '*') {
            if (!rtp_read(exe.data() + i, exe.size() - i).records.empty()) { off = i; break; }
        }
    }
    if (off >= exe.size()) {
        fprintf(stderr, "error: no RTPatch container found in %s\n", patch_file.c_str());
        return false;
    }
    const uint8_t* pd = exe.data() + off;
    size_t ps = exe.size() - off;
    RtpPatch p = rtp_read(pd, ps);

    fprintf(out, "\napplying the 1.02F patch (%zu file(s))...\n", p.records.size());
    int patched = 0, skipped = 0, failed = 0;
    for (const auto& rec : p.records) {
        std::string path = find_installed(dest, rec.name);
        if (path.empty()) {
            fprintf(out, "  skip  %-12s not in the install\n", rec.name.c_str());
            skipped++;
            continue;
        }
        auto src = read_file(path);
        if (rtp_checksum(src.data(), src.size(), 31) != rec.src_w1 ||
            rtp_checksum(src.data(), src.size(), 30) != rec.src_w2) {
            fprintf(out, "  skip  %-12s not the 1.00F source (already patched?)\n", rec.name.c_str());
            skipped++;
            continue;
        }
        auto res = rtp_reconstruct(pd, ps, rec, src.data(), src.size(), false);
        if (res.size() != rec.dst_size || !write_file(path, res)) {
            fprintf(stderr, "  FAIL  %-12s could not reconstruct\n", rec.name.c_str());
            failed++;
            continue;
        }
        fprintf(out, "  ok    %-12s %u bytes\n", rec.name.c_str(), rec.dst_size);
        patched++;
    }
    fprintf(out, "patched %d file(s) to 1.02F", patched);
    if (skipped) fprintf(out, ", %d left as-is", skipped);
    if (failed)  fprintf(out, ", %d failed", failed);
    fprintf(out, "\n");
    return failed == 0;
}

int run(int argc, char** argv, bool execute, bool verify_only) {
    Args a = parse(argc, argv);
    if (!a.ok) return 1;
    if ((execute || verify_only) && a.dest.empty()) {
        fprintf(stderr, "No destination: -d <dir>\n");
        return 1;
    }

    const auto discs = scan_all(a);

    // What is already in the destination drives the clobber guard — except when
    // verifying, which asks a different question: does this tree hold what a
    // fresh install from these discs would write? Planning it against the files
    // that are already there would mark every one of them KeepExisting and
    // verify nothing at all.
    const auto existing = (verify_only || a.dest.empty())
                              ? std::vector<std::string>()
                              : install_list_dir(a.dest);
    const InstallPlan plan = install_plan(discs, existing, a.opt);

    if (a.json) print_json(plan, discs);
    else        print_plan(plan);
    if (!plan.errors.empty()) return 1;
    if (!execute && !verify_only) return 0;

    FILE* out = a.json ? stderr : stdout;
    std::vector<std::string> errors;
    if (execute) {
        fprintf(out, "\ninstalling to %s\n", a.dest.c_str());
        if (!install_execute(discs, plan, a.dest, a.json ? nullptr : progress, nullptr,
                             &errors)) {
            for (const auto& e : errors) fprintf(stderr, "error: %s\n", e.c_str());
            return 1;
        }
        fprintf(out, "\ninstalled %llu bytes to %s (build %s)\n",
                (unsigned long long)plan.bytes, a.dest.c_str(),
                install_build_name(plan.build));
    }
    // Verify against the disc first — the freshly written tree is still 1.00F,
    // which is what the disc holds — then patch it up to 1.02F.
    if (verify_only || (execute && a.verify)) {
        fprintf(out, "verifying against the disc...\n");
        if (!install_verify(discs, plan, a.dest, &errors)) {
            for (const auto& e : errors) fprintf(stderr, "error: %s\n", e.c_str());
            return 1;
        }
        fprintf(out, "verified: every installed byte matches the disc\n");
    }
    // Chain the 1.02F updater: the discs write 1.00F, but the reconstruction
    // database describes 1.02F, so --patch brings the install up to it. The
    // multiplayer msapi.dll is delivered as a NEW record the container walk does
    // not yet reach (RTP.md #54); the four modified game files — the executable,
    // its symbols, and the two content archives — are patched in place.
    if (execute && !a.patch.empty() && !apply_patch(a.patch, a.dest, out)) return 1;
    return 0;
}

} // namespace

int cmd_install(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
                "Usage: fx install <plan|run|verify> <disc-dir> [disc-dir ...] -d <dest>\n"
                "         [--full|--minimal] [--verify] [--overwrite] [--patch <fae102.exe>]\n"
                "         [--no-cd-resident] [--any-media] [--json]\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "plan")   == 0) return run(argc - 1, argv + 1, false, false);
    if (strcmp(sub, "run")    == 0) return run(argc - 1, argv + 1, true, false);
    if (strcmp(sub, "verify") == 0) return run(argc - 1, argv + 1, false, true);
    fprintf(stderr, "Unknown install subcommand: %s\n", sub);
    return 1;
}
