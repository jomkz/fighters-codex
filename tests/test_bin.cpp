#include <catch2/catch_test_macros.hpp>
#include <fx/bin.h>

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
