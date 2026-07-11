#include <catch2/catch_test_macros.hpp>
#include "asset_index.h"
#include "workspace.h"
#include <fx/ealib.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace fxg;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static fx::Entry mk_entry(const char* name, uint32_t size = 1) {
    fx::Entry e{};
    std::strncpy(e.name, name, sizeof(e.name) - 1);
    e.size = size;
    return e;
}

static MountSource lib_src(const char* path, std::initializer_list<const char*> names) {
    MountSource s;
    s.path  = path;
    s.isLib = true;
    for (const char* n : names) s.entries.push_back(mk_entry(n));
    return s;
}

// Node index for a name (case-insensitive), or -1.
static int node_of(const Workspace& ws, const std::string& name) {
    const WorkspaceEntry* e = ws.find(name);
    if (!e) return -1;
    return (int)(e - ws.names.data());
}

static bool refs_contains(const AssetIndex& idx, int from, int to) {
    if (from < 0 || to < 0) return false;
    const auto& r = idx.nodes[from].refs;
    return std::find(r.begin(), r.end(), to) != r.end();
}

// ---------------------------------------------------------------------------
// category_of — pure extension mapping
// ---------------------------------------------------------------------------

TEST_CASE("category_of maps extensions to the eight buckets", "[gui][asset-index]") {
    CHECK(category_of("A10.PT")     == Category::Aircraft);
    CHECK(category_of("KIEV.NT")    == Category::Vehicles);
    CHECK(category_of("BUNKER.OT")  == Category::Vehicles);
    CHECK(category_of("GAU8.JT")    == Category::Weapons);
    CHECK(category_of("A10R.SEE")   == Category::Weapons);
    CHECK(category_of("EXTRA10.M")  == Category::Missions);
    CHECK(category_of("DESERT.CAM") == Category::Campaigns);
    CHECK(category_of("EGYPT.T2")   == Category::Terrain);
    CHECK(category_of("JET1N.11K")  == Category::Audio);
    CHECK(category_of("MENU.XMI")   == Category::Audio);
    CHECK(category_of("_A10.PIC")   == Category::ArtUI);
    CHECK(category_of("A10.SH")     == Category::ArtUI);
}

TEST_CASE("category_of sends unknown and extension-less names to Unassigned", "[gui][asset-index]") {
    CHECK(category_of("MYSTERY.ZZZ") == Category::Unassigned);
    CHECK(category_of("READMEHERE")  == Category::Unassigned);
    CHECK(category_of("")            == Category::Unassigned);
}

// ---------------------------------------------------------------------------
// Invariant: every entry lands in >= 1 bucket (nothing hidden)
// ---------------------------------------------------------------------------

TEST_CASE("asset_index_build assigns every entry to at least one bucket", "[gui][asset-index]") {
    // Synthetic sources with fake paths: no file is read, so categorization runs
    // off names alone — enough to prove the invariant and the Unassigned bucket.
    std::vector<MountSource> src;
    src.push_back(lib_src("FA_2.LIB", {
        "A10.PT", "KIEV.NT", "GAU8.JT", "EXTRA10.M", "DESERT.CAM",
        "EGYPT.T2", "JET1N.11K", "_A10.PIC", "A10.SH", "MYSTERY.ZZZ" }));
    Workspace ws = workspace_build(std::move(src), "/fake");

    AssetIndex idx = asset_index_build(ws);
    REQUIRE(idx.built);
    REQUIRE(idx.nodes.size() == ws.names.size());

    // Every node appears in >= 1 category bucket.
    std::vector<char> seen(idx.nodes.size(), 0);
    for (int c = 0; c < kCategoryCount; ++c)
        for (int k : idx.byCategory[c]) seen[k] = 1;
    for (size_t k = 0; k < seen.size(); ++k)
        CHECK(seen[k] == 1);

    // The unknown extension is in Unassigned; the aircraft is not.
    CHECK(idx.has(node_of(ws, "MYSTERY.ZZZ"), Category::Unassigned));
    CHECK(idx.has(node_of(ws, "A10.PT"), Category::Aircraft));
    CHECK_FALSE(idx.has(node_of(ws, "A10.PT"), Category::Unassigned));
}

// ---------------------------------------------------------------------------
// Graph: a BRF entity record links to the shape it names, and the shape
// inherits the object's category (cluster propagation) — no real assets needed.
// ---------------------------------------------------------------------------

TEST_CASE("asset_index_build links a PT to its shape and propagates Aircraft", "[gui][asset-index]") {
    // A minimal BRF .PT whose one pointer table names FOO.SH.
    const char* pt =
        "[brent's_relocatable_format]\r\n"
        ":shape\r\n"
        "\tstring \"FOO.SH\"\r\n"
        "\tend\r\n";
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
        { "TEST.PT", std::vector<uint8_t>(pt, pt + std::strlen(pt)) },
        { "FOO.SH",  { 0x00 } },          // exists in the namespace; bytes unused here
        { "BARE.PIC", { 0x00 } },         // unreferenced art -> stays Art/UI only
    };
    fs::path root = fs::temp_directory_path() / "fxs_index_graph_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    {
        auto lib = fx::ealib_build(files);
        std::ofstream f(root / "TEST.LIB", std::ios::binary);
        f.write((const char*)lib.data(), (std::streamsize)lib.size());
    }

    Workspace ws = workspace_scan(root.string());
    AssetIndex idx = asset_index_build(ws);
    REQUIRE(idx.built);

    int pt_n  = node_of(ws, "TEST.PT");
    int sh_n  = node_of(ws, "FOO.SH");
    int pic_n = node_of(ws, "BARE.PIC");
    REQUIRE(pt_n >= 0);
    REQUIRE(sh_n >= 0);
    REQUIRE(pic_n >= 0);

    CHECK(refs_contains(idx, pt_n, sh_n));                 // PT -> SH edge
    CHECK(idx.has(pt_n, Category::Aircraft));              // PT seeds Aircraft
    CHECK(idx.has(sh_n, Category::Aircraft));              // propagated to the shape
    CHECK(idx.has(sh_n, Category::ArtUI));                 // keeps its base bucket too
    CHECK_FALSE(idx.has(pic_n, Category::Aircraft));       // unreferenced art untouched
    CHECK(idx.has(pic_n, Category::ArtUI));

    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// asset_cluster (#365): the object's file cluster, expanded through Art/UI
// nodes only — a referenced weapon stays a leaf, its own refs excluded.
// ---------------------------------------------------------------------------

TEST_CASE("asset_cluster collects an object's files without crossing into other objects", "[gui][asset-index]") {
    const char* pt =
        "[brent's_relocatable_format]\r\n"
        ":refs\r\n"
        "\tstring \"FOO.SH\"\r\n"
        "\tstring \"JET1N.11K\"\r\n"
        "\tstring \"GAU8.JT\"\r\n"
        "\tend\r\n";
    const char* jt =
        "[brent's_relocatable_format]\r\n"
        ":refs\r\n"
        "\tstring \"OTHER.SH\"\r\n"
        "\tend\r\n";
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
        { "TEST.PT",   std::vector<uint8_t>(pt, pt + std::strlen(pt)) },
        { "FOO.SH",    { 0x00 } },  // wreck sibling edge comes from the name
        { "FOO_A.SH",  { 0x00 } },
        { "JET1N.11K", { 0x00 } },
        { "GAU8.JT",   std::vector<uint8_t>(jt, jt + std::strlen(jt)) },
        { "OTHER.SH",  { 0x00 } },  // the weapon's own art — not in the cluster
    };
    fs::path root = fs::temp_directory_path() / "fxs_cluster_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    {
        auto lib = fx::ealib_build(files);
        std::ofstream f(root / "TEST.LIB", std::ios::binary);
        f.write((const char*)lib.data(), (std::streamsize)lib.size());
    }

    Workspace ws = workspace_scan(root.string());
    AssetIndex idx = asset_index_build(ws);
    REQUIRE(idx.built);

    std::vector<int> cluster = asset_cluster(idx, ws, node_of(ws, "TEST.PT"));

    // Root first, then leaves grouped by (base category, extension, name):
    // the weapon leaf, the sound, then the Art/UI shapes (wreck included via
    // the SH's variant edge — expanded because SH is Art/UI-base).
    std::vector<int> expect = {
        node_of(ws, "TEST.PT"), node_of(ws, "GAU8.JT"), node_of(ws, "JET1N.11K"),
        node_of(ws, "FOO.SH"),  node_of(ws, "FOO_A.SH"),
    };
    CHECK(cluster == expect);

    // GAU8.JT is a leaf: its own reference did not drag OTHER.SH in.
    CHECK(std::find(cluster.begin(), cluster.end(),
                    node_of(ws, "OTHER.SH")) == cluster.end());

    // Out-of-range roots yield an empty cluster.
    CHECK(asset_cluster(idx, ws, -1).empty());
    CHECK(asset_cluster(idx, ws, (int)ws.names.size()).empty());

    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// Real-install spot check (FX_FA_ROOT): the A10 cluster
// ---------------------------------------------------------------------------

TEST_CASE("asset_index_build clusters the A10 on a real install", "[gui][asset-index][fa-root]") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root) {
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    }

    Workspace ws = workspace_scan(root);
    REQUIRE(ws.mounted());
    AssetIndex idx = asset_index_build(ws);
    REQUIRE(idx.built);

    int pt   = node_of(ws, "A10.PT");
    int sh   = node_of(ws, "A10.SH");
    int pic  = node_of(ws, "_A10.PIC");
    int wreck = node_of(ws, "A10_A.SH");
    REQUIRE(pt >= 0);
    REQUIRE(sh >= 0);
    REQUIRE(pic >= 0);
    REQUIRE(wreck >= 0);

    // PT names its shape; the shape names its texture and wreck sibling.
    CHECK(refs_contains(idx, pt, sh));
    CHECK(refs_contains(idx, sh, pic));
    CHECK(refs_contains(idx, sh, wreck));

    // The whole visual cluster carries Aircraft (propagated from the PT).
    CHECK(idx.has(pt, Category::Aircraft));
    CHECK(idx.has(sh, Category::Aircraft));
    CHECK(idx.has(pic, Category::Aircraft));
    CHECK(idx.has(wreck, Category::Aircraft));

    // The object-scope acceptance path (#365): selecting the A10 must make its
    // shape, skin PIC and wreck sibling reachable through one file cluster.
    std::vector<int> cluster = asset_cluster(idx, ws, pt);
    REQUIRE_FALSE(cluster.empty());
    CHECK(cluster.front() == pt);
    for (int want : {sh, pic, wreck})
        CHECK(std::find(cluster.begin(), cluster.end(), want) != cluster.end());
}
