#include <catch2/catch_test_macros.hpp>
#include <fx/ealib.h>
#include <cstring>

using namespace fx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<std::pair<std::string, std::vector<uint8_t>>>
make_files(std::initializer_list<std::pair<const char*, std::vector<uint8_t>>> init) {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> out;
    for (auto& p : init) out.push_back({ p.first, p.second });
    return out;
}

static std::vector<uint8_t> bytes(std::initializer_list<uint8_t> il) {
    return std::vector<uint8_t>(il);
}

// Single-entry LIB with arbitrary flags — ealib_build writes raw entries
// only, so compressed fixtures are assembled by hand.
static std::vector<uint8_t> make_lib_with_entry(const char* name, uint8_t flags,
                                                const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> lib(7 + 18, 0);
    memcpy(lib.data(), "EALIB", 5);
    lib[5] = 1; // count = 1 (LE)
    uint8_t* e = lib.data() + 7;
    memcpy(e, name, std::min(strlen(name), (size_t)12));
    e[13] = flags;
    e[14] = 7 + 18; // offset (LE, fits in one byte)
    lib.insert(lib.end(), payload.begin(), payload.end());
    return lib;
}

// Mark Adler's blast.c example stream: litmode 0, dictbits 4, decompresses
// to "AIAIAIAIAIAIA" (13 bytes)
static const std::vector<uint8_t> dcl_example =
    { 0x00, 0x04, 0x82, 0x24, 0x25, 0x8F, 0x80, 0x7F };

// flags=4 payload: 4-byte LE EA decompressed-size prefix + DCL stream
static std::vector<uint8_t> dcl_payload(uint32_t claimed_size) {
    std::vector<uint8_t> p = {
        (uint8_t)claimed_size, (uint8_t)(claimed_size >> 8),
        (uint8_t)(claimed_size >> 16), (uint8_t)(claimed_size >> 24)
    };
    p.insert(p.end(), dcl_example.begin(), dcl_example.end());
    return p;
}

// ---------------------------------------------------------------------------
// ealib_read_dir â€” parsing
// ---------------------------------------------------------------------------

TEST_CASE("ealib_read_dir rejects bad magic") {
    auto lib = ealib_build(make_files({ { "a.bin", bytes({ 1, 2, 3 }) } }));
    lib[0] = 'X'; // corrupt magic
    REQUIRE(ealib_read_dir(lib.data(), lib.size()).empty());
}

TEST_CASE("ealib_read_dir rejects truncated buffer") {
    auto lib = ealib_build(make_files({ { "a.bin", bytes({ 1 }) } }));
    // Feed only the first 4 bytes â€” smaller than the minimum header
    REQUIRE(ealib_read_dir(lib.data(), 4).empty());
}

TEST_CASE("ealib_read_dir on empty archive") {
    auto lib = ealib_build({});
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.empty());
}

TEST_CASE("ealib_read_dir returns correct entry count") {
    auto lib = ealib_build(make_files({
        { "foo.bin", bytes({ 1, 2 }) },
        { "bar.bin", bytes({ 3, 4, 5 }) },
    }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 2);
}

TEST_CASE("ealib_read_dir populates names correctly") {
    auto lib = ealib_build(make_files({
        { "alpha.bin", bytes({ 0 }) },
        { "beta.txt",  bytes({ 0 }) },
    }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(std::string(entries[0].name) == "alpha.bin");
    REQUIRE(std::string(entries[1].name) == "beta.txt");
}

TEST_CASE("ealib_read_dir name is always null-terminated") {
    // 12-character name exactly fills the 13-byte field â€” byte 12 must be '\0'
    auto lib = ealib_build(make_files({ { "123456789012", bytes({ 0 }) } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries[0].name[12] == '\0');
}

TEST_CASE("ealib_read_dir computes sizes from adjacent offsets") {
    auto lib = ealib_build(make_files({
        { "a.bin", bytes({ 1, 2, 3 }) },       // size 3
        { "b.bin", bytes({ 4, 5 }) },           // size 2
        { "c.bin", bytes({ 6, 7, 8, 9 }) },     // size 4
    }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries[0].size == 3);
    REQUIRE(entries[1].size == 2);
    REQUIRE(entries[2].size == 4);
}

TEST_CASE("ealib_read_dir raw entries have flags == 0") {
    auto lib = ealib_build(make_files({ { "x.bin", bytes({ 0xAB }) } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries[0].flags == 0);
}

// ---------------------------------------------------------------------------
// ealib_extract â€” extraction
// ---------------------------------------------------------------------------

TEST_CASE("ealib_extract returns exact raw bytes") {
    std::vector<uint8_t> payload = { 0xDE, 0xAD, 0xBE, 0xEF };
    auto lib = ealib_build(make_files({ { "data.bin", payload } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 1);

    auto extracted = ealib_extract(lib.data(), lib.size(), entries[0]);
    REQUIRE(extracted == payload);
}

TEST_CASE("ealib_extract returns correct data for each of multiple entries") {
    std::vector<uint8_t> a = { 1, 2 };
    std::vector<uint8_t> b = { 3, 4, 5 };
    std::vector<uint8_t> c = { 6 };
    auto lib = ealib_build(make_files({ { "a.bin", a }, { "b.bin", b }, { "c.bin", c } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());

    REQUIRE(ealib_extract(lib.data(), lib.size(), entries[0]) == a);
    REQUIRE(ealib_extract(lib.data(), lib.size(), entries[1]) == b);
    REQUIRE(ealib_extract(lib.data(), lib.size(), entries[2]) == c);
}

TEST_CASE("ealib_extract decompresses a flags==4 (PKWare DCL) entry") {
    auto lib = make_lib_with_entry("dcl.bin", 4, dcl_payload(13));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].flags == 4);

    auto out = ealib_extract(lib.data(), lib.size(), entries[0]);
    REQUIRE(std::string(out.begin(), out.end()) == "AIAIAIAIAIAIA");
}

TEST_CASE("ealib_extract rejects an implausible decompressed-size claim") {
    // #168: the EA size prefix is attacker-controlled — a crafted archive
    // claiming ~1.3 GiB must be rejected as malformed, not allocated.
    auto lib = make_lib_with_entry("evil.bin", 4, dcl_payload(0x50000009u));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 1);

    REQUIRE(ealib_extract(lib.data(), lib.size(), entries[0]).empty());
}

TEST_CASE("ealib_extract handles a zero decompressed-size claim") {
    // #169: expected == 0 hands blast a null output buffer; the extraction
    // must degrade to an empty payload without UB (verified by the sanitizer
    // presets)
    auto lib = make_lib_with_entry("zero.bin", 4, dcl_payload(0));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 1);

    REQUIRE(ealib_extract(lib.data(), lib.size(), entries[0]).empty());
}

TEST_CASE("ealib_extract accepts a decompressed-size claim at the ceiling") {
    // Exactly 64 MiB — the documented maximum plausible decompressed size.
    // The prefix may overstate the actual output; extraction still succeeds.
    auto lib = make_lib_with_entry("edge.bin", 4, dcl_payload(64u << 20));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 1);

    auto out = ealib_extract(lib.data(), lib.size(), entries[0]);
    REQUIRE(std::string(out.begin(), out.end()) == "AIAIAIAIAIAIA");
}

TEST_CASE("ealib_extract returns empty vector for out-of-bounds entry") {
    auto lib = ealib_build(make_files({ { "x.bin", bytes({ 1 }) } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());

    Entry bad = entries[0];
    bad.offset = (uint32_t)lib.size(); // past EOF
    bad.size   = 1;

    REQUIRE(ealib_extract(lib.data(), lib.size(), bad).empty());
}

// ---------------------------------------------------------------------------
// Round-trip: build â†’ read_dir â†’ extract
// ---------------------------------------------------------------------------

TEST_CASE("ealib round-trip preserves all file contents") {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
        { "one.bin",   { 0x11, 0x22, 0x33 } },
        { "two.bin",   { 0xAA, 0xBB } },
        { "three.bin", { 0xFF } },
    };

    auto lib = ealib_build(files);
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == files.size());

    for (size_t i = 0; i < files.size(); i++) {
        REQUIRE(std::string(entries[i].name) == files[i].first);
        auto extracted = ealib_extract(lib.data(), lib.size(), entries[i]);
        REQUIRE(extracted == files[i].second);
    }
}

// ---------------------------------------------------------------------------
// ealib_patch
// ---------------------------------------------------------------------------

TEST_CASE("ealib_patch replaces target entry content") {
    auto lib = ealib_build(make_files({
        { "keep.bin",    bytes({ 1, 2, 3 }) },
        { "replace.bin", bytes({ 4, 5, 6 }) },
    }));

    std::vector<uint8_t> replacement = { 0xDE, 0xAD };
    auto patched = ealib_patch(lib.data(), lib.size(), "replace.bin", replacement);

    auto entries = ealib_read_dir(patched.data(), patched.size());
    REQUIRE(entries.size() == 2);

    auto got = ealib_extract(patched.data(), patched.size(), entries[1]);
    REQUIRE(got == replacement);
}

TEST_CASE("ealib_patch preserves untouched entries") {
    std::vector<uint8_t> original = { 1, 2, 3 };
    auto lib = ealib_build(make_files({
        { "keep.bin",    original },
        { "replace.bin", bytes({ 9 }) },
    }));

    auto patched = ealib_patch(lib.data(), lib.size(), "replace.bin", bytes({ 0xFF }));
    auto entries = ealib_read_dir(patched.data(), patched.size());

    auto kept = ealib_extract(patched.data(), patched.size(), entries[0]);
    REQUIRE(kept == original);
}

TEST_CASE("ealib_patch returns empty for unrecognised archive") {
    std::vector<uint8_t> garbage = { 0, 1, 2, 3, 4, 5, 6 };
    auto result = ealib_patch(garbage.data(), garbage.size(), "x.bin", bytes({ 1 }));
    REQUIRE(result.empty());
}

// ---------------------------------------------------------------------------
// ealib_find — case-insensitive lookup
// ---------------------------------------------------------------------------

TEST_CASE("ealib_find matches entry names case-insensitively") {
    auto lib = ealib_build(make_files({
        { "A.BIN", bytes({ 1 }) },
        { "b.dat", bytes({ 2 }) },
    }));
    auto entries = ealib_read_dir(lib.data(), lib.size());
    REQUIRE(entries.size() == 2);

    const Entry* e = ealib_find(entries, "a.bin");
    REQUIRE(e != nullptr);
    REQUIRE(strcmp(e->name, "A.BIN") == 0);

    REQUIRE(ealib_find(entries, "B.DAT") == &entries[1]);
    REQUIRE(ealib_find(entries, "b.dat") == &entries[1]);
}

TEST_CASE("ealib_find rejects prefix and missing names") {
    auto lib = ealib_build(make_files({ { "data.bin", bytes({ 1 }) } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());

    REQUIRE(ealib_find(entries, "data.bi") == nullptr);   // prefix, not a match
    REQUIRE(ealib_find(entries, "data.bin2") == nullptr); // longer than entry
    REQUIRE(ealib_find(entries, "missing.x") == nullptr);
}

TEST_CASE("ealib_find on empty entry list") {
    REQUIRE(ealib_find({}, "a.bin") == nullptr);
}

// ---------------------------------------------------------------------------
// ealib_safe_name — platform-identical output filenames
// ---------------------------------------------------------------------------

TEST_CASE("ealib_safe_name passes legitimate 8.3 names through unchanged") {
    REQUIRE(ealib_safe_name("BALTIC.TXT") == "BALTIC.TXT");
    REQUIRE(ealib_safe_name("f16c_0.pic") == "f16c_0.pic");
    REQUIRE(ealib_safe_name("") == "");
}

TEST_CASE("ealib_safe_name replaces each special character with underscore") {
    REQUIRE(ealib_safe_name("&AFTB2.11K") == "_AFTB2.11K");
    REQUIRE(ealib_safe_name("A*B.BIN") == "A_B.BIN");
    REQUIRE(ealib_safe_name("A?B.BIN") == "A_B.BIN");
    REQUIRE(ealib_safe_name("A\"B.BIN") == "A_B.BIN");
    REQUIRE(ealib_safe_name("A<B>.BIN") == "A_B_.BIN");
    REQUIRE(ealib_safe_name("A|B.BIN") == "A_B.BIN");
}

TEST_CASE("ealib_safe_name replaces path characters from crafted archives") {
    REQUIRE(ealib_safe_name("../EVIL.BIN") == ".._EVIL.BIN");
    REQUIRE(ealib_safe_name("..\\EVIL.BIN") == ".._EVIL.BIN");
    REQUIRE(ealib_safe_name("C:EVIL.BIN") == "C_EVIL.BIN");
}

TEST_CASE("ealib_safe_name on all-special input") {
    REQUIRE(ealib_safe_name("&*?\"<>|/\\:") == "__________");
}
