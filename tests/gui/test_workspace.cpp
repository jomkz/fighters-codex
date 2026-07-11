#include <catch2/catch_test_macros.hpp>
#include "workspace.h"
#include <fx/ealib.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

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

static MountSource loose_src(const char* path, const char* name) {
    MountSource s;
    s.path  = path;
    s.isLib = false;
    s.entries.push_back(mk_entry(name));
    return s;
}

// Write a synthetic raw LIB to disk from (name, 1-byte payload) pairs.
static void write_lib(const fs::path& p, std::initializer_list<const char*> names) {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
    for (const char* n : names) files.push_back({ n, { 0xAB } });
    auto bytes = fx::ealib_build(files);
    std::ofstream f(p, std::ios::binary);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
}

static void write_loose(const fs::path& p, const char* content) {
    std::ofstream f(p, std::ios::binary);
    f << content;
}

// ---------------------------------------------------------------------------
// workspace_build — pure aggregation
// ---------------------------------------------------------------------------

TEST_CASE("workspace_build merges disjoint sources into one namespace", "[gui][workspace]") {
    std::vector<MountSource> src;
    src.push_back(lib_src("FA_1.LIB", { "A10.PT", "A10.SH" }));
    src.push_back(lib_src("FA_2.LIB", { "F15.PT", "F15.SH", "F15.INF" }));

    Workspace ws = workspace_build(std::move(src), "/fake/root");

    CHECK(ws.mounted());
    CHECK(ws.libCount == 2);
    CHECK(ws.looseCount == 0);
    CHECK(ws.names.size() == 5);          // distinct names == total entries
    CHECK(ws.collisions.empty());
    // Sorted case-insensitively.
    CHECK(ws.names.front().name == std::string("A10.PT"));
    // find() resolves case-insensitively; misses return nullptr.
    REQUIRE(ws.find("f15.sh") != nullptr);
    CHECK(ws.find("f15.sh")->sourceIdx == 1);
    CHECK(ws.find("nope.xyz") == nullptr);
}

TEST_CASE("workspace_build: later-mounted LIB wins a name collision", "[gui][workspace]") {
    std::vector<MountSource> src;
    src.push_back(lib_src("FA_1.LIB", { "SHARED.PIC", "ONLY1.PIC" }));
    src.push_back(lib_src("FA_2.LIB", { "SHARED.PIC", "ONLY2.PIC" }));

    Workspace ws = workspace_build(std::move(src));

    CHECK(ws.names.size() == 3);          // SHARED collapses
    REQUIRE(ws.find("SHARED.PIC") != nullptr);
    CHECK(ws.find("SHARED.PIC")->sourceIdx == 1); // last mount wins
    REQUIRE(ws.collisions.size() == 1);
    CHECK(ws.collisions[0].name == std::string("SHARED.PIC"));
    CHECK(ws.collisions[0].winner == 1);
    REQUIRE(ws.collisions[0].shadowed.size() == 1);
    CHECK(ws.collisions[0].shadowed[0] == 0);
}

TEST_CASE("workspace_build: a LIB entry always outranks a loose file", "[gui][workspace]") {
    // Regardless of mount order, the LIB entry resolves and the loose file is shadowed.
    SECTION("loose mounted first") {
        std::vector<MountSource> src;
        src.push_back(loose_src("A10.PT", "A10.PT"));      // source 0, loose
        src.push_back(lib_src("FA_2.LIB", { "A10.PT" }));  // source 1, LIB
        Workspace ws = workspace_build(std::move(src));
        REQUIRE(ws.find("A10.PT") != nullptr);
        CHECK(ws.find("A10.PT")->sourceIdx == 1);          // the LIB
        REQUIRE(ws.collisions.size() == 1);
        CHECK(ws.collisions[0].winner == 1);
        CHECK(ws.collisions[0].shadowed == std::vector<int>{ 0 });
    }
    SECTION("LIB mounted first") {
        std::vector<MountSource> src;
        src.push_back(lib_src("FA_2.LIB", { "A10.PT" }));  // source 0, LIB
        src.push_back(loose_src("A10.PT", "A10.PT"));      // source 1, loose
        Workspace ws = workspace_build(std::move(src));
        REQUIRE(ws.find("A10.PT") != nullptr);
        CHECK(ws.find("A10.PT")->sourceIdx == 0);          // still the LIB
        REQUIRE(ws.collisions.size() == 1);
        CHECK(ws.collisions[0].winner == 0);
    }
}

TEST_CASE("workspace_build handles the empty workspace", "[gui][workspace]") {
    Workspace ws = workspace_build({}, "");
    CHECK_FALSE(ws.mounted());
    CHECK(ws.names.empty());
    CHECK(ws.collisions.empty());
    CHECK(ws.find("anything") == nullptr);
}

// ---------------------------------------------------------------------------
// workspace_scan — filesystem IO against synthetic archives
// ---------------------------------------------------------------------------

TEST_CASE("workspace_scan mounts LIBs and loose files from a directory", "[gui][workspace]") {
    fs::path root = fs::temp_directory_path() / "fxs_ws_scan_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    write_lib(root / "FA_1.LIB", { "A10.PT", "A10.SH" });
    write_lib(root / "fa_2.lib", { "F15.PT", "SHARED.PIC" });   // lowercase name
    write_loose(root / "EA.CFG", "loose");
    write_loose(root / "SHARED.PIC", "loose override attempt"); // shadowed by the LIB

    Workspace ws = workspace_scan(root.string());

    CHECK(ws.mounted());
    CHECK(ws.libCount == 2);        // both .LIB / .lib picked up (case-insensitive)
    CHECK(ws.looseCount == 2);      // EA.CFG + SHARED.PIC on disk

    // Every LIB source's entry count equals an independent ealib_read_dir.
    for (const auto& s : ws.sources) {
        if (!s.isLib) continue;
        std::ifstream f(s.path, std::ios::binary | std::ios::ate);
        REQUIRE(f);
        std::streamoff sz = f.tellg();
        std::vector<uint8_t> data((size_t)sz);
        f.seekg(0);
        f.read((char*)data.data(), sz);
        CHECK(s.entries.size() == fx::ealib_read_dir(data.data(), data.size()).size());
    }

    // The LIB's SHARED.PIC outranks the loose SHARED.PIC on disk.
    REQUIRE(ws.find("SHARED.PIC") != nullptr);
    CHECK(ws.sources[ws.find("SHARED.PIC")->sourceIdx].isLib);
    // Names from every source are present.
    CHECK(ws.find("A10.SH") != nullptr);
    CHECK(ws.find("F15.PT") != nullptr);
    CHECK(ws.find("EA.CFG") != nullptr);

    fs::remove_all(root, ec);
}

TEST_CASE("workspace_scan on a missing directory is unmounted", "[gui][workspace]") {
    Workspace ws = workspace_scan((fs::temp_directory_path() / "fxs_no_such_dir_xyz").string());
    CHECK_FALSE(ws.mounted());
}

// ---------------------------------------------------------------------------
// Real-install check (FX_FA_ROOT) — entry counts match per-LIB opens
// ---------------------------------------------------------------------------

TEST_CASE("workspace_scan indexes a real FA install", "[gui][workspace][fa-root]") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root) {
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    }

    Workspace ws = workspace_scan(root);
    REQUIRE(ws.mounted());
    CHECK(ws.libCount >= 1);
    CHECK(!ws.names.empty());

    // Each mounted LIB's entry count equals an independent ealib_read_dir open.
    for (const auto& s : ws.sources) {
        if (!s.isLib) continue;
        std::ifstream f(s.path, std::ios::binary | std::ios::ate);
        REQUIRE(f);
        std::streamoff sz = f.tellg();
        std::vector<uint8_t> data((size_t)sz);
        f.seekg(0);
        f.read((char*)data.data(), sz);
        CHECK(s.entries.size() == fx::ealib_read_dir(data.data(), data.size()).size());
    }
}
