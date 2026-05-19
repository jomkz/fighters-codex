#include <catch2/catch_test_macros.hpp>
#include <ft/ealib.h>
#include <cstring>

using namespace ft;

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

// ---------------------------------------------------------------------------
// ealib_read_dir — parsing
// ---------------------------------------------------------------------------

TEST_CASE("ealib_read_dir rejects bad magic") {
    auto lib = ealib_build(make_files({ { "a.bin", bytes({ 1, 2, 3 }) } }));
    lib[0] = 'X'; // corrupt magic
    REQUIRE(ealib_read_dir(lib.data(), lib.size()).empty());
}

TEST_CASE("ealib_read_dir rejects truncated buffer") {
    auto lib = ealib_build(make_files({ { "a.bin", bytes({ 1 }) } }));
    // Feed only the first 4 bytes — smaller than the minimum header
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
    // 12-character name exactly fills the 13-byte field — byte 12 must be '\0'
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
// ealib_extract — extraction
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

TEST_CASE("ealib_extract returns empty vector for out-of-bounds entry") {
    auto lib = ealib_build(make_files({ { "x.bin", bytes({ 1 }) } }));
    auto entries = ealib_read_dir(lib.data(), lib.size());

    Entry bad = entries[0];
    bad.offset = (uint32_t)lib.size(); // past EOF
    bad.size   = 1;

    REQUIRE(ealib_extract(lib.data(), lib.size(), bad).empty());
}

// ---------------------------------------------------------------------------
// Round-trip: build → read_dir → extract
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
