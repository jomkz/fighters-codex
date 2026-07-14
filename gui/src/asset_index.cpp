#include "asset_index.h"
#include "util.h"
#include "fx/ealib.h"
#include "fx/brf.h"
#include "fx/sh.h"
#include "fx/cam.h"
#include "fx/mission.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <unordered_map>

namespace fxg {

const char* category_name(Category c) {
    switch (c) {
        case Category::Aircraft:   return "Aircraft";
        case Category::Vehicles:   return "Vehicles";
        case Category::Weapons:    return "Weapons";
        case Category::Missions:   return "Missions";
        case Category::Campaigns:  return "Campaigns";
        case Category::Terrain:    return "Terrain";
        case Category::Audio:      return "Audio";
        case Category::ArtUI:      return "Art/UI";
        case Category::Unassigned: return "Unassigned";
        default:                   return "?";
    }
}

// Lowercased extension after the last '.', or "" if none.
static std::string ext_of(const std::string& name) {
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return {};
    std::string e = name.substr(dot + 1);
    for (char& c : e) c = ascii_lower(c);
    return e;
}

Category category_of(const std::string& name) {
    const std::string e = ext_of(name);
    if (e.empty()) return Category::Unassigned;
    auto is = [&](std::initializer_list<const char*> xs) {
        for (const char* x : xs) if (e == x) return true;
        return false;
    };
    if (is({"pt"}))                              return Category::Aircraft;
    if (is({"nt", "ot"}))                        return Category::Vehicles;
    if (is({"jt", "see", "ecm", "gas"}))         return Category::Weapons;
    if (is({"m", "mm", "mt", "mc"}))             return Category::Missions;
    if (is({"cam", "p"}))                        return Category::Campaigns;
    if (is({"t2", "lay"}))                       return Category::Terrain;
    if (is({"11k", "5k", "8k", "22k", "xmi", "mus"})) return Category::Audio;
    if (is({"pic", "pal", "raw", "ico", "sh", "hud", "fnt", "dlg", "mnu",
            "pts", "hgr", "seq", "inf", "ai", "bi", "cb8", "vdo", "fbc",
            "txt", "wri", "hlp", "cnt", "ini", "bin", "sms", "exe"}))
        return Category::ArtUI;
    return Category::Unassigned;
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    std::streamoff sz = f.tellg();
    if (sz <= 0) return {};
    std::vector<uint8_t> data((size_t)sz);
    f.seekg(0);
    f.read((char*)data.data(), sz);
    return data;
}

// Which extensions carry outgoing references worth parsing.
static bool ref_bearing(const std::string& e) {
    return e == "pt" || e == "ot" || e == "nt" || e == "jt" || e == "see" ||
           e == "ecm" || e == "gas" || e == "sh" || e == "cam" ||
           e == "m" || e == "mm";
}

AssetIndex asset_index_build(const Workspace& ws, const IndexProgressFn& progress,
                             const IndexCancel* cancel) {
    AssetIndex idx;
    const int n = (int)ws.names.size();
    idx.nodes.resize(n);
    idx.byCategory.resize(kCategoryCount);

    // Seed base categories and a case-insensitive name -> node lookup.
    std::vector<Category> base(n, Category::Unassigned);
    std::unordered_map<std::string, int> byName;
    byName.reserve(n * 2);
    for (int k = 0; k < n; ++k) {
        base[k] = category_of(ws.names[k].name);
        idx.nodes[k].categories = (uint16_t)(1u << (int)base[k]);
        std::string key = ws.names[k].name;
        for (char& c : key) c = ascii_lower(c);
        byName.emplace(std::move(key), k);
    }

    auto resolve = [&](const std::string& raw) -> int {
        if (raw.empty()) return -1;
        std::string key = raw;
        for (char& c : key) c = ascii_lower(c);
        auto it = byName.find(key);
        if (it != byName.end()) return it->second;
        // Refs without an extension: probe the two graph-relevant kinds.
        if (key.find('.') == std::string::npos) {
            auto s = byName.find(key + ".sh");
            if (s != byName.end()) return s->second;
            auto p = byName.find(key + ".pic");
            if (p != byName.end()) return p->second;
        }
        return -1;
    };

    // Group nodes by their source so each LIB file is read exactly once.
    std::vector<std::vector<int>> sourceNodes(ws.sources.size());
    for (int k = 0; k < n; ++k)
        sourceNodes[ws.names[k].sourceIdx].push_back(k);

    auto add_edge = [&](int from, const std::string& ref) {
        int to = resolve(ref);
        if (to >= 0 && to != from) idx.nodes[from].refs.push_back(to);
    };

    const int srcTotal = (int)ws.sources.size();
    for (int si = 0; si < srcTotal; ++si) {
        if (cancel && cancel->stop.load()) return idx; // built stays false
        const MountSource& src = ws.sources[si];
        if (progress) progress(si, srcTotal, src.path);

        // Skip sources with no ref-bearing entries — avoids reading the file.
        bool any = false;
        for (int k : sourceNodes[si])
            if (ref_bearing(ext_of(ws.names[k].name))) { any = true; break; }
        if (!any) continue;

        std::vector<uint8_t> buf = read_file(src.path);
        if (buf.empty()) continue;

        for (int k : sourceNodes[si]) {
            const std::string& name = ws.names[k].name;
            const std::string e = ext_of(name);
            if (!ref_bearing(e)) continue;

            std::vector<uint8_t> rec;
            if (src.isLib) {
                bool unsup = false;
                rec = fx::ealib_extract(buf.data(), buf.size(),
                                        src.entries[ws.names[k].entryIdx], true, &unsup);
            } else {
                rec = buf; // a loose file is its own record
            }
            if (rec.empty()) continue;

            if (e == "sh") {
                fx::ShInfo info = fx::sh_parse_info(rec.data(), rec.size());
                for (const std::string& tex : info.textures) add_edge(k, tex);
                for (char v : {'a', 'b', 'c', 'd', 's'})
                    add_edge(k, fx::sh_variant_name(name, v));
            } else if (e == "cam") {
                for (const std::string& s : fx::cam_strings(rec.data(), rec.size())) {
                    int to = resolve(s + ".M");
                    if (to >= 0 && to != k) idx.nodes[k].refs.push_back(to);
                }
            } else if (e == "m" || e == "mm") {
                fx::MissionInfo mi = fx::mission_parse_info(rec.data(), rec.size());
                add_edge(k, mi.map_file);
                add_edge(k, mi.layer_file);
                fx::MissionObjects mo = fx::mission_parse_objects(rec.data(), rec.size());
                for (const fx::MissionObj& o : mo.objects) add_edge(k, o.type_file);
            } else { // BRF entity records: pt/ot/nt/jt/see/ecm/gas
                fx::BrfDoc doc = fx::brf_parse(rec.data(), rec.size());
                for (const fx::BrfBlock& b : doc.blocks)
                    for (const std::string& s : b.strings) add_edge(k, s);
            }
        }
    }

    // Deduplicate each node's edge list.
    for (auto& node : idx.nodes) {
        std::sort(node.refs.begin(), node.refs.end());
        node.refs.erase(std::unique(node.refs.begin(), node.refs.end()), node.refs.end());
    }

    // Propagate an object's category (Aircraft/Vehicles/Weapons) along the graph
    // into the art assets it reaches — shapes, their textures and wreck siblings
    // — so an object and its visual cluster share a category. Propagation only
    // crosses into Art/UI-base nodes, so it never bleeds one object type into
    // another (a PT that lists a JT weapon does not make the weapon "Aircraft").
    auto is_object_root = [](Category c) {
        return c == Category::Aircraft || c == Category::Vehicles || c == Category::Weapons;
    };
    for (int r = 0; r < n; ++r) {
        if (!is_object_root(base[r])) continue;
        const uint16_t bit = (uint16_t)(1u << (int)base[r]);
        std::vector<char> seen(n, 0);
        std::deque<int> q;
        auto enqueue = [&](int j) {
            if (j >= 0 && !seen[j] && base[j] == Category::ArtUI) { seen[j] = 1; q.push_back(j); }
        };
        for (int j : idx.nodes[r].refs) enqueue(j);
        while (!q.empty()) {
            int j = q.front(); q.pop_front();
            idx.nodes[j].categories |= bit;
            for (int m : idx.nodes[j].refs) enqueue(m);
        }
    }

    // Bucket every node under each category bit it carries. Every node keeps its
    // seeded base bit, so each appears in >= 1 bucket (Unassigned included).
    for (int k = 0; k < n; ++k)
        for (int c = 0; c < kCategoryCount; ++c)
            if (idx.nodes[k].categories & (1u << c))
                idx.byCategory[c].push_back(k);

    idx.built = true;
    if (progress) progress(srcTotal, srcTotal, ws.root);
    return idx;
}

std::vector<int> asset_cluster(const AssetIndex& idx, const Workspace& ws, int root) {
    std::vector<int> out;
    const int n = (int)idx.nodes.size();
    if (root < 0 || root >= n || n != (int)ws.names.size()) return out;

    // BFS from the root; a reached node joins the cluster, but only the root
    // and Art/UI-base nodes expand further (see the header for the rationale).
    std::vector<char> seen(n, 0);
    std::deque<int> q{root};
    seen[root] = 1;
    while (!q.empty()) {
        int k = q.front(); q.pop_front();
        out.push_back(k);
        if (k != root && category_of(ws.names[k].name) != Category::ArtUI) continue;
        for (int j : idx.nodes[k].refs)
            if (!seen[j]) { seen[j] = 1; q.push_back(j); }
    }

    std::sort(out.begin() + 1, out.end(), [&](int a, int b) {
        const std::string& na = ws.names[a].name;
        const std::string& nb = ws.names[b].name;
        Category ca = category_of(na), cb = category_of(nb);
        if (ca != cb) return (int)ca < (int)cb;
        std::string ea = ext_of(na), eb = ext_of(nb);
        if (ea != eb) return ea < eb;
        return na < nb;
    });
    return out;
}

} // namespace fxg
