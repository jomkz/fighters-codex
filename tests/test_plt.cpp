#include <catch2/catch_test_macros.hpp>
#include <fx/plt.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;

// Copy at most max_chars characters; the buffer is pre-zeroed so the
// string stays null-terminated (portable stand-in for MSVC strncpy_s).
static void copy_field(std::vector<uint8_t>& buf, size_t off,
                       const char* src, size_t max_chars)
{
    std::memcpy(buf.data() + off, src,
                std::min(std::strlen(src), max_chars));
}

// Build a minimal PLT identity block (0xB0 bytes).
static std::vector<uint8_t> make_plt_identity(
    const char* name     = "LT James Kirk",
    const char* callsign = "ARCHER",
    const char* rank     = "2nd Lieutenant")
{
    std::vector<uint8_t> buf(0xB0, 0);
    buf[0x00] = 0x0F; // version tag
    copy_field(buf, 0x01, name,     62);
    copy_field(buf, 0x40, callsign, 31);
    copy_field(buf, 0xA2, rank,     13);
    return buf;
}

TEST_CASE("plt_parse rejects buffer smaller than 0xB0") {
    std::vector<uint8_t> tiny(0xAF, 0x0F);
    PltInfo info;
    REQUIRE_FALSE(plt_parse(tiny.data(), tiny.size(), &info));
}

TEST_CASE("plt_parse rejects wrong version tag") {
    auto buf = make_plt_identity();
    buf[0x00] = 0x01; // wrong tag
    PltInfo info;
    REQUIRE_FALSE(plt_parse(buf.data(), buf.size(), &info));
}

TEST_CASE("plt_parse reads pilot name") {
    auto buf = make_plt_identity("Top Gun Pilot");
    PltInfo info;
    REQUIRE(plt_parse(buf.data(), buf.size(), &info));
    REQUIRE(info.name == "Top Gun Pilot");
}

TEST_CASE("plt_parse reads callsign") {
    auto buf = make_plt_identity("Unused", "VIPER");
    PltInfo info;
    REQUIRE(plt_parse(buf.data(), buf.size(), &info));
    REQUIRE(info.callsign == "VIPER");
}

TEST_CASE("plt_parse reads rank") {
    auto buf = make_plt_identity("Unused", "UNUSED", "Colonel");
    PltInfo info;
    REQUIRE(plt_parse(buf.data(), buf.size(), &info));
    REQUIRE(info.rank == "Colonel");
}

TEST_CASE("plt_parse returns true and empty campaign for minimal identity block") {
    auto buf = make_plt_identity();
    PltInfo info;
    REQUIRE(plt_parse(buf.data(), buf.size(), &info));
    REQUIRE(info.cam_file.empty());
    REQUIRE(info.aircraft.empty());
}

TEST_CASE("plt_parse sets version_tag to 0x0F") {
    auto buf = make_plt_identity();
    PltInfo info;
    REQUIRE(plt_parse(buf.data(), buf.size(), &info));
    REQUIRE(info.version_tag == 0x0F);
}

// ─── Round-trip / serializer (issue #103) ───────────────────────────────────

// Build a full-size (0x25E0) pilot image whose identity and stats regions are
// well-formed, while every other byte — the four unmapped gap regions and the
// variable-length campaign region — carries a deterministic non-zero pattern
// standing in for content the serializer must pass through verbatim.
static std::vector<uint8_t> make_plt_full() {
    std::vector<uint8_t> buf(0x25E0);
    uint32_t s = 0x01234567u;              // deterministic LCG fill, always non-zero
    for (auto& b : buf) { s = s * 1103515245u + 12345u; b = (uint8_t)((s >> 16) | 1u); }

    std::memset(buf.data(), 0, 0xB0);      // well-formed, null-padded identity block
    buf[0x00] = 0x0F;
    copy_field(buf, 0x01, "Maj Chuck Yeager", 62);
    copy_field(buf, 0x40, "GLAMOROUS",        31);
    copy_field(buf, 0x61, "^ACID.5K",         12);
    copy_field(buf, 0x6E, "NOSE01",           12);
    copy_field(buf, 0x7B, "LEFT03",           12);
    copy_field(buf, 0x88, "RIGHT03",          12);
    copy_field(buf, 0x95, "PILOT02",          12);
    copy_field(buf, 0xA2, "1st Lieutenant",   13);
    buf[0x60] = 0xAB;                       // non-zero gap byte inside identity range
    return buf;
}

TEST_CASE("plt_repack is byte-identical for an identity-only block") {
    auto buf = make_plt_identity();
    auto out = plt_repack(buf.data(), buf.size());
    REQUIRE(out == buf);
}

TEST_CASE("plt_repack passes the four gap regions through verbatim") {
    auto buf = make_plt_full();
    auto out = plt_repack(buf.data(), buf.size());
    REQUIRE(out == buf);
}

TEST_CASE("plt_write reproduces a field that fills its whole width") {
    std::vector<uint8_t> buf(0xB0, 0);
    buf[0x00] = 0x0F;
    std::string full(63, 'A');             // occupies 0x01..0x3F with no terminator
    std::memcpy(buf.data() + 0x01, full.data(), full.size());
    auto out = plt_repack(buf.data(), buf.size());
    REQUIRE(out == buf);
}

TEST_CASE("plt_write preserves stale bytes past a field terminator") {
    // A shorter name written over a longer one can leave non-zero tail bytes;
    // an unedited round-trip must keep them (byte-exact passthrough).
    auto buf = make_plt_identity("Al");
    buf[0x04] = 'X';  buf[0x05] = 'Y';      // stale bytes after "Al\0"
    PltFile f;
    REQUIRE(plt_read(buf.data(), buf.size(), &f));
    REQUIRE(f.info.name == "Al");
    REQUIRE(plt_write(f) == buf);
}

TEST_CASE("editing one identity field via PltFile touches only that field") {
    auto buf = make_plt_full();
    PltFile f;
    REQUIRE(plt_read(buf.data(), buf.size(), &f));
    f.info.callsign = "MAVERICK";
    auto out = plt_write(f);
    REQUIRE(out.size() == buf.size());

    PltInfo re;
    REQUIRE(plt_parse(out.data(), out.size(), &re));
    REQUIRE(re.callsign == "MAVERICK");
    for (size_t i = 0; i < out.size(); i++) {
        if (i >= 0x40 && i < 0x60) continue;   // the callsign field
        INFO("byte " << i);
        REQUIRE(out[i] == buf[i]);
    }
}

TEST_CASE("editing a stats counter via PltFile touches only that field") {
    auto buf = make_plt_full();
    PltFile f;
    REQUIRE(plt_read(buf.data(), buf.size(), &f));
    REQUIRE(f.has_stats);
    f.stats.missions_flown = 42;
    auto out = plt_write(f);

    PltStats re;
    REQUIRE(plt_parse_stats(out.data(), out.size(), &re));
    REQUIRE(re.missions_flown == 42);
    for (size_t i = 0; i < out.size(); i++) {
        if (i >= 0x1F80 && i < 0x1F84) continue;   // missions_flown u32
        INFO("byte " << i);
        REQUIRE(out[i] == buf[i]);
    }
}

TEST_CASE("plt_repack rejects a buffer smaller than the identity block") {
    std::vector<uint8_t> tiny(0xAF, 0);
    tiny[0x00] = 0x0F;
    REQUIRE(plt_repack(tiny.data(), tiny.size()).empty());
}

TEST_CASE("plt_repack rejects a wrong version tag") {
    auto buf = make_plt_full();
    buf[0x00] = 0x01;
    REQUIRE(plt_repack(buf.data(), buf.size()).empty());
}

// Real pilot files live loose in the FA install (created in-game), never in a
// LIB and never committed — so this census only runs under FX_FA_ROOT.
TEST_CASE("plt_repack is byte-identical for every real pilot file") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string x) {
        for (char& c : x) c = (char)std::tolower((unsigned char)c);
        return x;
    };
    int total = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        std::string fn = lower(de.path().filename().string());
        if (fn.size() < 4 || fn.compare(0, 3, "plt") != 0 ||
            fn.substr(fn.size() - 2) != ".p")
            continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> data((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)data.data(), (std::streamsize)data.size());
        PltInfo info;
        if (!plt_parse(data.data(), data.size(), &info)) continue;  // not a pilot file
        INFO(de.path().filename().string());
        REQUIRE(plt_repack(data.data(), data.size()) == data);
        total++;
    }
    if (total == 0)
        SKIP("no PLTnnn.P pilot files in this install (created in-game)");
    WARN("round-tripped " << total << " real pilot file(s)");
}
