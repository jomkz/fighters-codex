#include <catch2/catch_test_macros.hpp>
#include <fx/pal.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>
#include <cstring>
#include <vector>

using namespace fx;

// A full valid palette: every channel value in the 6-bit range, varied enough
// to catch channel swaps and off-by-one indexing.
static std::vector<uint8_t> make_vga_palette() {
    std::vector<uint8_t> raw(768);
    for (int i = 0; i < 256; i++) {
        raw[i * 3 + 0] = (uint8_t)(i % 64);
        raw[i * 3 + 1] = (uint8_t)((i * 7 + 3) % 64);
        raw[i * 3 + 2] = (uint8_t)(63 - (i % 64));
    }
    return raw;
}

TEST_CASE("pal_load / pal_save round-trip is byte-identical for valid 6-bit input") {
    auto raw = make_vga_palette();
    Palette pal = pal_load(raw.data(), raw.size());
    uint8_t out[768];
    pal_save(pal, out);
    REQUIRE(std::memcmp(out, raw.data(), 768) == 0);
}

TEST_CASE("pal_load scales 6-bit channels as documented") {
    uint8_t raw[9] = {0, 32, 63,   1, 2, 3,   10, 20, 30};
    Palette pal = pal_load(raw, sizeof raw);
    // actual = pal_widen6: (stored << 2) | (stored >> 4) — 63 → full 255 (#369)
    REQUIRE(pal.r[0] == 0);
    REQUIRE(pal.g[0] == 130);
    REQUIRE(pal.b[0] == 255);
    REQUIRE(pal.r[1] == 4);
    REQUIRE(pal.g[1] == 8);
    REQUIRE(pal.b[1] == 12);
}

TEST_CASE("pal_load falls back to greyscale on null or tiny input") {
    Palette null_pal = pal_load(nullptr, 768);
    Palette tiny_pal = pal_load(make_vga_palette().data(), 2);
    for (int i = 0; i < 256; i++) {
        REQUIRE(null_pal.r[i] == (uint8_t)i);
        REQUIRE(null_pal.g[i] == (uint8_t)i);
        REQUIRE(null_pal.b[i] == (uint8_t)i);
        REQUIRE(tiny_pal.r[i] == (uint8_t)i);
    }
}

TEST_CASE("pal_load reads only complete triplets of a partial palette") {
    uint8_t raw[8] = {10, 20, 30,   40, 50, 60,   1, 2}; // 2 triplets + 2 stray bytes
    Palette pal = pal_load(raw, sizeof raw);
    REQUIRE(pal.r[0] == ((10 << 2) | (10 >> 4)));
    REQUIRE(pal.b[1] == ((60 << 2) | (60 >> 4)));  // 243 (#369)
    // entry 2 untouched by the stray bytes: zero-initialised
    REQUIRE(pal.r[2] == 0);
    REQUIRE(pal.g[2] == 0);
    REQUIRE(pal.b[2] == 0);
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// One shipped palette, and the whole codec is a 6-bit <-> 8-bit conversion pair. So the thing
// worth asserting is that the pair is a true inverse ON THE REAL BYTES: pal_load widens with
// bit replication ((v<<2)|(v>>4), #369) and pal_save narrows with >>2. If those two ever
// disagree, every PIC in the game shifts colour -- and nothing else would have noticed.
// ---------------------------------------------------------------------------

TEST_CASE("PAL decode census: the shipped palette survives widen/narrow exactly") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int found = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".pal") continue;
            auto data = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(e.name);
            REQUIRE(data.size() == 768);          // 256 x 3, VGA

            // Every component is 6-bit, as a VGA palette must be.
            for (uint8_t c : data) REQUIRE(c <= 63);

            // The round-trip the codec's two halves imply.
            Palette pal = pal_load(data.data(), data.size());
            uint8_t out[768];
            pal_save(pal, out);
            REQUIRE(std::vector<uint8_t>(out, out + 768) == data);

            // And the widen really is bit replication: 63 -> 255, not 63 -> 252.
            for (int i = 0; i < 256; i++) {
                INFO("entry " << i);
                REQUIRE(pal.r[i] == pal_widen6(data[i * 3 + 0]));
                REQUIRE(pal.g[i] == pal_widen6(data[i * 3 + 1]));
                REQUIRE(pal.b[i] == pal_widen6(data[i * 3 + 2]));
            }
            found++;
        }
    }
    REQUIRE(found > 0);
    WARN("PAL census: " << found << " shipped palette(s), widen/narrow exact on every entry");
}
