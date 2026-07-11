#include "workspace.h"
#include "util.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace fxg {

// Case-insensitive ordering used for the sorted namespace and binary lookup.
static bool ci_less(const std::string& a, const std::string& b) {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        char ca = ascii_lower(a[i]), cb = ascii_lower(b[i]);
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

static std::string lower_key(const std::string& s) {
    std::string k = s;
    for (char& c : k) c = ascii_lower(c);
    return k;
}

// A LIB entry outranks a loose file; within one kind the later mount wins.
// Higher tuple (isLib, sourceIdx) resolves.
static bool outranks(const MountSource& a, int ai, const MountSource& b, int bi) {
    if (a.isLib != b.isLib) return a.isLib; // LIB beats loose
    return ai > bi;                          // later mount wins
}

const WorkspaceEntry* Workspace::find(const std::string& name) const {
    auto it = std::lower_bound(names.begin(), names.end(), name,
        [](const WorkspaceEntry& e, const std::string& key) {
            return ci_less(e.name, key);
        });
    if (it != names.end() && fxg::ci_equal(it->name.c_str(), name.c_str()))
        return &*it;
    return nullptr;
}

Workspace workspace_build(std::vector<MountSource> sources, std::string root) {
    Workspace ws;
    ws.root    = std::move(root);
    ws.sources = std::move(sources);

    for (const auto& s : ws.sources)
        (s.isLib ? ws.libCount : ws.looseCount)++;

    // Fold every (source, entry) into one namespace, keeping the ranking winner
    // per case-insensitive name and remembering the sources it shadowed.
    struct Win { int src, entry; std::string name; std::vector<int> shadowed; };
    std::unordered_map<std::string, Win> best;
    best.reserve(4096);

    for (int si = 0; si < (int)ws.sources.size(); ++si) {
        const auto& src = ws.sources[si];
        for (int ei = 0; ei < (int)src.entries.size(); ++ei) {
            std::string name = src.entries[ei].name;
            if (name.empty()) continue;
            std::string key = lower_key(name);
            auto it = best.find(key);
            if (it == best.end()) {
                best.emplace(std::move(key), Win{si, ei, std::move(name), {}});
                continue;
            }
            Win& w = it->second;
            if (outranks(src, si, ws.sources[w.src], w.src)) {
                w.shadowed.push_back(w.src); // old winner is now shadowed
                w.src   = si;
                w.entry = ei;
                w.name  = std::move(name);
            } else {
                w.shadowed.push_back(si);    // challenger loses
            }
        }
    }

    ws.names.reserve(best.size());
    for (auto& kv : best) {
        Win& w = kv.second;
        ws.names.push_back(WorkspaceEntry{w.name, w.src, w.entry});
        if (!w.shadowed.empty()) {
            std::sort(w.shadowed.begin(), w.shadowed.end());
            ws.collisions.push_back(Collision{w.name, w.src, std::move(w.shadowed)});
        }
    }
    std::sort(ws.names.begin(), ws.names.end(),
        [](const WorkspaceEntry& a, const WorkspaceEntry& b) {
            return ci_less(a.name, b.name);
        });
    std::sort(ws.collisions.begin(), ws.collisions.end(),
        [](const Collision& a, const Collision& b) { return ci_less(a.name, b.name); });
    return ws;
}

Workspace workspace_scan(const std::string& root, const ProgressFn& progress) {
    // Enumerate the root directory, splitting LIBs from loose files. LIBs are
    // mounted in case-insensitive filename order for deterministic precedence.
    std::error_code ec;
    if (root.empty() || !fs::is_directory(root, ec))
        return Workspace{}; // invalid root -> unmounted (mounted() == false)

    std::vector<fs::path> libs, loose;
    for (const auto& de : fs::directory_iterator(root, ec)) {
        if (!de.is_regular_file(ec)) continue;
        const fs::path& p = de.path();
        std::string ext = p.extension().string();
        for (char& c : ext) c = ascii_lower(c);
        if (ext == ".lib") libs.push_back(p);
        else               loose.push_back(p);
    }
    auto byName = [](const fs::path& a, const fs::path& b) {
        return ci_less(a.filename().string(), b.filename().string());
    };
    std::sort(libs.begin(), libs.end(), byName);
    std::sort(loose.begin(), loose.end(), byName);

    // LIBs first, then loose files, so a loose file's higher source index never
    // lets it outrank a LIB entry (outranks() keys on isLib before the index).
    std::vector<fs::path> ordered = std::move(libs);
    ordered.insert(ordered.end(), loose.begin(), loose.end());
    const int libN = (int)(ordered.size() - loose.size());

    std::vector<MountSource> sources;
    sources.reserve(ordered.size());
    for (int i = 0; i < (int)ordered.size(); ++i) {
        const fs::path& p = ordered[i];
        if (progress) progress(i, (int)ordered.size(), p.string());

        MountSource src;
        src.path  = p.string();
        src.isLib = (i < libN);
        if (src.isLib) {
            std::ifstream f(p, std::ios::binary | std::ios::ate);
            if (!f) continue;
            std::streamoff sz = f.tellg();
            if (sz <= 0) continue;
            std::vector<uint8_t> data((size_t)sz);
            f.seekg(0);
            f.read((char*)data.data(), sz);
            src.entries = fx::ealib_read_dir(data.data(), data.size());
            if (src.entries.empty()) {
                // Not a parseable LIB — index it as a loose file instead.
                src.isLib = false;
            }
        }
        if (!src.isLib) {
            // Loose file: one synthetic entry (name + size), no payload read.
            std::uintmax_t sz = fs::file_size(p, ec);
            fx::Entry e = {};
            fxg::copy_str(e.name, sizeof(e.name),
                          fx::ealib_safe_name(p.filename().string().c_str()));
            e.flags  = 0;
            e.offset = 0;
            e.size   = ec ? 0u : (uint32_t)sz;
            src.entries.assign(1, e);
        }
        sources.push_back(std::move(src));
    }
    if (progress) progress((int)ordered.size(), (int)ordered.size(), root);
    return workspace_build(std::move(sources), root);
}

} // namespace fxg
