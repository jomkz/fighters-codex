#include <catch2/catch_test_macros.hpp>
#include <fx/bin.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <string>
#include <vector>

using namespace fx;

TEST_CASE("bin_classify recognizes every documented table") {
    // BIN.md file inventory — all six FA_2.LIB tables.
    REQUIRE(bin_classify("INSIGMAP.BIN") == BinKind::Insigmap);
    REQUIRE(bin_classify("MIX2.BIN")     == BinKind::Mix2);
    REQUIRE(bin_classify("MIX2L.BIN")    == BinKind::Mix2L);
    REQUIRE(bin_classify("MIX4.BIN")     == BinKind::Mix4);
    REQUIRE(bin_classify("MIX4L.BIN")    == BinKind::Mix4L);
    REQUIRE(bin_classify("VFONTPAL.BIN") == BinKind::VFontPal);
}

TEST_CASE("bin_classify is case-insensitive and extension-tolerant") {
    REQUIRE(bin_classify("mix2l.bin") == BinKind::Mix2L);
    REQUIRE(bin_classify("VfontPal")  == BinKind::VFontPal);
    REQUIRE(bin_classify("insigmap")  == BinKind::Insigmap);
}

TEST_CASE("bin_classify does not confuse the MIX family") {
    // MIX2 vs MIX2L vs MIX4 vs MIX4L are distinct tables (linear vs gamma);
    // prefix matching would conflate them.
    REQUIRE(bin_classify("MIX2.BIN")  != bin_classify("MIX2L.BIN"));
    REQUIRE(bin_classify("MIX4.BIN")  != bin_classify("MIX4L.BIN"));
    REQUIRE(bin_classify("MIX23.BIN") == BinKind::Unknown);
}

TEST_CASE("bin_expected_size matches the documented inventory") {
    REQUIRE(bin_expected_size(BinKind::Insigmap) == 256);
    REQUIRE(bin_expected_size(BinKind::Mix2)     == 512);
    REQUIRE(bin_expected_size(BinKind::Mix2L)    == 512);
    REQUIRE(bin_expected_size(BinKind::Mix4)     == 1024);
    REQUIRE(bin_expected_size(BinKind::Mix4L)    == 1024);
    REQUIRE(bin_expected_size(BinKind::VFontPal) == 48);
    REQUIRE(bin_expected_size(BinKind::Unknown)  == 0);
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// BIN's codec is read-only by design: "the bytes ARE the content; identity is the only write,
// so a writer would prove nothing -- BIN.md documents each table's GENERATOR FUNCTION instead."
// That rationale is only worth anything if the generators are checked, and none of them were.
// So check them: regenerate each table from what the doc says it is, and compare.
// ---------------------------------------------------------------------------

TEST_CASE("BIN decode census: each shipped table, against its documented generator") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    std::map<std::string, std::vector<uint8_t>> tables;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".bin") continue;
            tables[lower(e.name)] = ealib_extract(lib.data(), lib.size(), e, true);
        }
    }
    REQUIRE(tables.size() == 6);

    for (const auto& [name, data] : tables) {
        INFO(name);
        const BinKind kind = bin_classify(name);
        REQUIRE(kind != BinKind::Unknown);              // every shipped table is one we know
        REQUIRE(data.size() == bin_expected_size(kind)); // ...and is the size we say it is
    }

    // The linear blend tables ARE their generator, exactly.
    const std::vector<uint8_t>& mix2l = tables["mix2l.bin"];
    for (size_t i = 0; i < mix2l.size(); i++) {
        INFO("MIX2L[" << i << "]");
        REQUIRE(mix2l[i] == (uint8_t)(i / 2));
    }
    const std::vector<uint8_t>& mix4l = tables["mix4l.bin"];
    for (size_t i = 0; i < mix4l.size(); i++) {
        INFO("MIX4L[" << i << "]");
        REQUIRE(mix4l[i] == (uint8_t)(i / 4));
    }

    // The gamma variants are not linear -- that is the point of them -- but they are
    // monotonic and hit the endpoints BIN.md states.
    const std::vector<uint8_t>& mix2 = tables["mix2.bin"];
    REQUIRE(mix2[0] == 0);
    REQUIRE(mix2[255] == 128);
    REQUIRE(mix2[511] == 255);
    for (size_t i = 1; i < mix2.size(); i++) REQUIRE(mix2[i] >= mix2[i - 1]);
    REQUIRE(mix2[64] != 64 / 2);   // and genuinely not the linear curve

    const std::vector<uint8_t>& mix4 = tables["mix4.bin"];
    REQUIRE(mix4[0] == 0);
    REQUIRE(mix4[1023] == 255);
    for (size_t i = 1; i < mix4.size(); i++) REQUIRE(mix4[i] >= mix4[i - 1]);

    // INSIGMAP is NOT a constant fill. BIN.md said "entry 0 = 0x00; all remaining 255 entries
    // = 0x3B", and inferred a "no insignia" sentinel from that fill. The fill is real but it
    // is not the whole table: 135 of the 256 entries are 0x3B and 120 are ACTUAL SLOTS. It is
    // a map, and reading it as a constant threw away everything it maps.
    const std::vector<uint8_t>& insig = tables["insigmap.bin"];
    REQUIRE(insig.size() == 256);
    REQUIRE(insig[0] == 0x00);
    size_t sentinel = 0;
    for (uint8_t b : insig) if (b == 0x3B) sentinel++;
    REQUIRE(sentinel == 135);                 // not 255
    REQUIRE(256 - sentinel - 1 == 120);       // real insignia slots, minus entry 0

    // VFONTPAL is 16 VGA entries, and VGA components are 6-bit.
    const std::vector<uint8_t>& vf = tables["vfontpal.bin"];
    REQUIRE(vf.size() == 48);
    for (uint8_t c : vf) REQUIRE(c <= 63);

    WARN("BIN census: 6 tables; MIX2L/MIX4L match their generators exactly; INSIGMAP holds "
         << (256 - sentinel - 1) << " real slots behind " << sentinel << " sentinels");
}
