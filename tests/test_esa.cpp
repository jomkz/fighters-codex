#include <catch2/catch_test_macros.hpp>
#include <fx/esa.h>
#include <cstring>
#include <string>

#include "support/fixture.h"

using namespace fx;

// ---------------------------------------------------------------------------
// Helpers — all fixtures are synthetic (policy: tests/fixtures/README.md); the
// one real byte sequence is the committed blast known-answer stream, reused so
// the PKWA path is exercised with zero game bytes.
// ---------------------------------------------------------------------------

static const char MAGIC[] = "ELECTRONIC_ARTS_ARCHIVE_FILE"; // 28 chars + NUL on disk

static void put_u32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)x);      v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)(x >> 16)); v.push_back((uint8_t)(x >> 24));
}
static void put_cstr(std::vector<uint8_t>& v, const std::string& s) {
    v.insert(v.end(), s.begin(), s.end()); v.push_back(0);
}

// A raw single-entry ESA with fully controllable fields — esa_build only writes
// NULL entries, so a PKWA or a deliberately-malformed archive is assembled here.
struct RawEntry {
    std::string name, label, method = "NULL";
    uint32_t flags = 0x211, usize = 0, mtime = 0, csize = 0;
    bool override_offset = false; uint32_t offset = 0;
    std::vector<uint8_t> payload;
};

static std::vector<uint8_t> make_esa(const std::vector<RawEntry>& entries) {
    std::vector<uint8_t> dir;
    dir.insert(dir.end(), (const uint8_t*)MAGIC, (const uint8_t*)MAGIC + sizeof(MAGIC));
    // First pass: size the directory (offsets are always 4 bytes).
    size_t dir_size = sizeof(MAGIC) + 1; // magic + terminator
    for (auto& e : entries)
        dir_size += e.name.size() + 1 + e.label.size() + 1 + 12 + e.method.size() + 1 + 8;
    uint32_t cursor = (uint32_t)dir_size;
    std::vector<uint8_t> blob;
    for (auto& e : entries) {
        put_cstr(dir, e.name);
        put_cstr(dir, e.label);
        put_u32(dir, e.flags);
        put_u32(dir, e.usize);
        put_u32(dir, e.mtime);
        put_cstr(dir, e.method);
        put_u32(dir, e.csize);
        put_u32(dir, e.override_offset ? e.offset : cursor);
        blob.insert(blob.end(), e.payload.begin(), e.payload.end());
        cursor += e.csize;
    }
    dir.push_back(0); // terminator
    dir.insert(dir.end(), blob.begin(), blob.end());
    return dir;
}

// ---------------------------------------------------------------------------
// Round-trip
// ---------------------------------------------------------------------------

TEST_CASE("esa_build then esa_read_dir round-trips fields and order") {
    std::vector<EsaInput> in = {
        {"FA.EXE", "FA_EXECUTABLE_FILES", 0x211, 0x33EFA5C2, {1, 2, 3, 4}},
        {"JANE'S HOME PAGE.URL", "FA_INTERNET", 0x211, 0, {'h', 'i'}},
        {"EAREMOVE.EXE", "REMOVER_EXECUTABLE_FILE", 0x221, 0, {9}},
    };
    auto esa = esa_build(in);
    auto dir = esa_read_dir(esa.data(), esa.size());
    REQUIRE(dir.size() == 3);
    REQUIRE(dir[0].name == "FA.EXE");
    REQUIRE(dir[0].label == "FA_EXECUTABLE_FILES");
    REQUIRE(dir[0].method == "NULL");
    REQUIRE(dir[0].usize == 4);
    REQUIRE(dir[0].csize == 4);
    REQUIRE(dir[0].mtime == 0x33EFA5C2);
    REQUIRE(dir[1].name == "JANE'S HOME PAGE.URL"); // spaces + apostrophe survive
    REQUIRE(dir[2].flags == 0x221);                 // the INSTALL_SYSFILES flag
    // Contiguity: dir_size == first offset, blobs back-to-back.
    REQUIRE(esa_dir_size(esa.data(), esa.size()) == dir[0].offset);
    REQUIRE(dir[1].offset == dir[0].offset + dir[0].csize);
    REQUIRE((uint64_t)dir.back().offset + dir.back().csize == esa.size());
}

TEST_CASE("esa_repack of a built archive is byte-identical") {
    auto esa = esa_build({{"A.LIB", "FA_LIBS", 0x211, 0, {1, 2, 3}},
                          {"B.TXT", "FA_MISC", 0x211, 0, {4, 5}}});
    REQUIRE(esa_repack(esa.data(), esa.size()) == esa);
}

TEST_CASE("esa_extract returns stored NULL payload verbatim") {
    auto esa = esa_build({{"X", "L", 0x211, 0, {10, 20, 30, 40}}});
    auto dir = esa_read_dir(esa.data(), esa.size());
    auto out = esa_extract(esa.data(), esa.size(), dir[0]);
    REQUIRE(out == std::vector<uint8_t>{10, 20, 30, 40});
}

// ---------------------------------------------------------------------------
// PKWA — the "raw DCL, no EA prefix" claim, using the committed blast KAT
// ---------------------------------------------------------------------------

TEST_CASE("esa_extract blast-decodes a PKWA entry (raw DCL, no EA prefix)") {
    // adler-ai.dcl: litmode 0, dictbits 4 -> "AIAIAIAIAIAIA" (13 bytes).
    auto dcl = fx_test::load_fixture("blast/adler-ai.dcl");
    RawEntry e;
    e.name = "AI.TXT"; e.label = "FA_MISC"; e.method = "PKWA";
    e.usize = 13; e.csize = (uint32_t)dcl.size(); e.payload = dcl;
    auto esa = make_esa({e});
    auto dir = esa_read_dir(esa.data(), esa.size());
    REQUIRE(dir.size() == 1);
    bool unsupported = true;
    auto out = esa_extract(esa.data(), esa.size(), dir[0], true, &unsupported);
    REQUIRE(unsupported == false);
    REQUIRE(std::string(out.begin(), out.end()) == "AIAIAIAIAIAIA");
    // decompress=false hands back the stored (still-compressed) bytes.
    auto raw = esa_extract(esa.data(), esa.size(), dir[0], false);
    REQUIRE(raw == dcl);
}

TEST_CASE("a PKWA stream that under-produces its claimed usize is rejected") {
    auto dcl = fx_test::load_fixture("blast/adler-ai.dcl");
    RawEntry e;
    e.name = "AI"; e.label = "L"; e.method = "PKWA";
    e.usize = 20; // the real stream yields only 13 — a truncated/lying claim
    e.csize = (uint32_t)dcl.size(); e.payload = dcl;
    auto esa = make_esa({e});
    auto dir = esa_read_dir(esa.data(), esa.size());
    // Strict: the directory pins the size, so a stream that decodes to fewer
    // than usize bytes is corruption (unlike ealib_extract, which tolerates it).
    REQUIRE(esa_extract(esa.data(), esa.size(), dir[0]).empty());
}

// ---------------------------------------------------------------------------
// Rejections — a malformed directory yields an empty vector
// ---------------------------------------------------------------------------

TEST_CASE("esa_read_dir rejects malformed archives") {
    auto good = esa_build({{"X", "L", 0x211, 0, {1, 2, 3}}});

    SECTION("bad magic") {
        auto b = good; b[0] = 'X';
        REQUIRE(esa_read_dir(b.data(), b.size()).empty());
    }
    SECTION("truncated below the magic") {
        REQUIRE(esa_read_dir(good.data(), 10).empty());
    }
    SECTION("no terminator before EOF") {
        // Drop the terminator and the blob: the cursor runs off the end.
        auto b = good; b.resize(esa_dir_size(good.data(), good.size()) - 1);
        REQUIRE(esa_read_dir(b.data(), b.size()).empty());
    }
    SECTION("unknown compression method") {
        RawEntry e; e.name = "X"; e.label = "L"; e.method = "LZ77";
        e.usize = 3; e.csize = 3; e.payload = {1, 2, 3};
        auto b = make_esa({e});
        REQUIRE(esa_read_dir(b.data(), b.size()).empty());
    }
    SECTION("NULL entry with csize != usize") {
        RawEntry e; e.name = "X"; e.label = "L"; e.method = "NULL";
        e.usize = 4; e.csize = 3; e.payload = {1, 2, 3};
        auto b = make_esa({e});
        REQUIRE(esa_read_dir(b.data(), b.size()).empty());
    }
    SECTION("offset overlapping the directory") {
        RawEntry e; e.name = "X"; e.label = "L"; e.method = "NULL";
        e.usize = 3; e.csize = 3; e.payload = {1, 2, 3};
        e.override_offset = true; e.offset = 5; // inside the magic/dir
        auto b = make_esa({e});
        REQUIRE(esa_read_dir(b.data(), b.size()).empty());
    }
    SECTION("offset + csize past EOF") {
        RawEntry e; e.name = "X"; e.label = "L"; e.method = "NULL";
        e.usize = 999; e.csize = 999; e.payload = {1, 2, 3};
        auto b = make_esa({e});
        REQUIRE(esa_read_dir(b.data(), b.size()).empty());
    }
}

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

TEST_CASE("a malformed non-8.3 member name survives verbatim") {
    // PKCOMP.IDKDECODLL — an EA builder bug on the real disc; the codec must not
    // choke on it or assume 8.3.
    auto esa = esa_build({{"PKCOMP.IDKDECODLL", "SETUP_SPECIAL_FILES", 0x211, 0, {1}}});
    auto dir = esa_read_dir(esa.data(), esa.size());
    REQUIRE(dir.size() == 1);
    REQUIRE(dir[0].name == "PKCOMP.IDKDECODLL");
    REQUIRE(esa_repack(esa.data(), esa.size()) == esa);
}

TEST_CASE("esa_find is ASCII case-insensitive") {
    auto esa = esa_build({{"FA.EXE", "L", 0x211, 0, {1}}});
    auto dir = esa_read_dir(esa.data(), esa.size());
    REQUIRE(esa_find(dir, "fa.exe") != nullptr);
    REQUIRE(esa_find(dir, "FA.EXE") != nullptr);
    REQUIRE(esa_find(dir, "nope") == nullptr);
}

TEST_CASE("esa_safe_name neutralises path characters but keeps spaces/apostrophes") {
    REQUIRE(esa_safe_name("a/b\\c:d") == "a_b_c_d");
    REQUIRE(esa_safe_name("..") == "_");
    REQUIRE(esa_safe_name("JANE'S HOME PAGE.URL") == "JANE'S HOME PAGE.URL");
    REQUIRE(esa_safe_name(std::string("x\x01y")) == "x_y");
}
