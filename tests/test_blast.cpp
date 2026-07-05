#include <catch2/catch_test_macros.hpp>
#include <fx/blast.h>
#include <fx/ealib.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "support/fixture.h"

using namespace fx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// The committed known-answer stream (tests/fixtures/blast/adler-ai.dcl):
// litmode 0, dictbits 4, decompresses to "AIAIAIAIAIAIA" (13 bytes).
static const std::vector<uint8_t>& kat_stream() {
    static const std::vector<uint8_t> s =
        fx_test::load_fixture("blast/adler-ai.dcl");
    return s;
}

// EA wrapper: 4-byte LE decompressed-size prefix + raw DCL stream.
static std::vector<uint8_t> ea_wrap(uint32_t claim,
                                    const std::vector<uint8_t>& stream) {
    std::vector<uint8_t> p = {
        (uint8_t)claim, (uint8_t)(claim >> 8),
        (uint8_t)(claim >> 16), (uint8_t)(claim >> 24)
    };
    p.insert(p.end(), stream.begin(), stream.end());
    return p;
}

static std::string as_string(const uint8_t* p, int n) {
    return std::string((const char*)p, (size_t)(n < 0 ? 0 : n));
}

// ---------------------------------------------------------------------------
// Known-answer round trip
// ---------------------------------------------------------------------------

TEST_CASE("blast_decompress reproduces the reference stream exactly") {
    uint8_t out[64] = {};
    int n = blast_decompress(kat_stream().data(), kat_stream().size(), out, sizeof(out));
    REQUIRE(n == 13);
    REQUIRE(as_string(out, n) == "AIAIAIAIAIAIA");
}

TEST_CASE("blast_decompress_ea honors the EA size prefix") {
    uint8_t out[128] = {};
    auto ea = ea_wrap(13, kat_stream());
    REQUIRE(blast_decompress_ea(ea.data(), ea.size(), out, sizeof(out)) == 13);
    REQUIRE(as_string(out, 13) == "AIAIAIAIAIAIA");

    // An overstated claim is capped by what the bitstream actually produces.
    auto over = ea_wrap(100, kat_stream());
    REQUIRE(blast_decompress_ea(over.data(), over.size(), out, sizeof(out)) == 13);
}

// ---------------------------------------------------------------------------
// Header error paths
// ---------------------------------------------------------------------------

TEST_CASE("blast_decompress rejects invalid headers") {
    uint8_t out[16] = {};
    auto s = kat_stream();

    SECTION("input shorter than the 2-byte header") {
        REQUIRE(blast_decompress(s.data(), 0, out, sizeof(out)) == -1);
        REQUIRE(blast_decompress(s.data(), 1, out, sizeof(out)) == -1);
    }
    SECTION("litmode other than 0 (coded literals unused by FA)") {
        auto c = s; c[0] = 1;
        REQUIRE(blast_decompress(c.data(), c.size(), out, sizeof(out)) == -1);
        c[0] = 0xFF;
        REQUIRE(blast_decompress(c.data(), c.size(), out, sizeof(out)) == -1);
    }
    SECTION("dictbits outside 4..6") {
        auto c = s; c[1] = 3;
        REQUIRE(blast_decompress(c.data(), c.size(), out, sizeof(out)) == -1);
        c[1] = 7;
        REQUIRE(blast_decompress(c.data(), c.size(), out, sizeof(out)) == -1);
    }
    SECTION("EA wrapper shorter than its 6-byte minimum") {
        auto ea = ea_wrap(13, s);
        REQUIRE(blast_decompress_ea(ea.data(), 5, out, sizeof(out)) == -1);
    }
}

// ---------------------------------------------------------------------------
// Truncation and corruption — must degrade, never crash
// ---------------------------------------------------------------------------

TEST_CASE("blast_decompress degrades gracefully on truncated streams") {
    // The port's documented contract: a valid header with a cut bitstream
    // yields the bytes decoded so far (possibly zero), not an error.
    uint8_t out[64] = {};
    auto s = kat_stream();
    for (size_t len = 2; len < s.size(); len++) {
        int n = blast_decompress(s.data(), len, out, sizeof(out));
        REQUIRE(n >= 0);
        REQUIRE(n <= 13);
    }
}

TEST_CASE("blast_decompress survives a single-byte corruption sweep") {
    uint8_t out[64] = {};
    for (size_t pos = 0; pos < kat_stream().size(); pos++) {
        for (uint8_t val : { (uint8_t)0x00, (uint8_t)0xFF, (uint8_t)0x55 }) {
            auto c = kat_stream();
            c[pos] = val;
            int n = blast_decompress(c.data(), c.size(), out, sizeof(out));
            REQUIRE(n >= -1);
            REQUIRE(n <= (int)sizeof(out));
        }
    }
}

// ---------------------------------------------------------------------------
// Output capacity (size mismatch between claim, capacity, and stream)
// ---------------------------------------------------------------------------

TEST_CASE("blast_decompress clamps output to the caller's capacity") {
    uint8_t out[64] = {};
    auto s = kat_stream();
    REQUIRE(blast_decompress(s.data(), s.size(), out, 5) == 5);
    REQUIRE(as_string(out, 5) == "AIAIA");
    REQUIRE(blast_decompress(s.data(), s.size(), out, 0) == 0);
}

TEST_CASE("blast_decompress_ea lets an understated claim win over capacity") {
    // The EA prefix is the game's contract for the output size: a claim
    // smaller than what the stream would produce caps the output.
    uint8_t out[64] = {};
    auto ea = ea_wrap(5, kat_stream());
    REQUIRE(blast_decompress_ea(ea.data(), ea.size(), out, sizeof(out)) == 5);
    auto zero = ea_wrap(0, kat_stream());
    REQUIRE(blast_decompress_ea(zero.data(), zero.size(), out, sizeof(out)) == 0);
}

// ---------------------------------------------------------------------------
// Real flags==4 entry (FX_FA_ROOT mode — skipped without a licensed install)
// ---------------------------------------------------------------------------

TEST_CASE("blast decompresses a real flags==4 entry from the FA install") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");

    namespace fsys = std::filesystem;
    fsys::path lib_path;
    for (auto& d : fsys::directory_iterator(root)) {
        std::string n = d.path().filename().string();
        for (auto& ch : n) ch = (char)toupper((unsigned char)ch);
        if (n == "FA_2.LIB") { lib_path = d.path(); break; }
    }
    if (lib_path.empty())
        SKIP("FA_2.LIB not found under FX_FA_ROOT");

    std::ifstream f(lib_path, std::ios::binary | std::ios::ate);
    REQUIRE(f.good());
    std::vector<uint8_t> lib((size_t)f.tellg());
    f.seekg(0);
    f.read((char*)lib.data(), (std::streamsize)lib.size());

    auto entries = ealib_read_dir(lib.data(), lib.size());
    const Entry* e = ealib_find(entries, "BALTIC.TXT");
    REQUIRE(e != nullptr);
    REQUIRE(e->flags == 4);

    auto data = ealib_extract(lib.data(), lib.size(), *e, true);
    // Facts recorded from the committed extraction manifest
    // (tests/integration/fa-extract.sha256: FA_2.LIB/BALTIC.TXT, 147 bytes,
    // sha256 a881649d…); the FNV-1a is derived from the same bytes.
    REQUIRE(data.size() == 147);
    REQUIRE(std::string(data.begin(), data.begin() + 10) == ".section 1");
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint8_t b : data) h = (h ^ b) * 0x100000001b3ull;
    REQUIRE(h == 0x6145943ac415cf91ull);
}
