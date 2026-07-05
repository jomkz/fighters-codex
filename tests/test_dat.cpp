#include <catch2/catch_test_macros.hpp>
#include <fx/dat.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace fx;

// Synthetic 3552-byte config with distinctive bytes everywhere, so any
// misplaced field boundary breaks the round-trip.
static std::vector<uint8_t> make_dat() {
    std::vector<uint8_t> b(DAT_FILE_SIZE);
    for (size_t i = 0; i < b.size(); i++) b[i] = (uint8_t)(i * 13 + 5);
    return b;
}

TEST_CASE("dat_read requires the exact v3 file size") {
    auto b = make_dat();
    CnInfo c;
    REQUIRE(dat_read(b.data(), b.size(), c));
    REQUIRE_FALSE(dat_read(b.data(), b.size() - 4, c));  // v2 size: not handled
    REQUIRE_FALSE(dat_read(b.data(), 0, c));
}

TEST_CASE("dat_read maps documented field offsets") {
    auto b = make_dat();
    // File offset = CN_INFO offset + 4.
    b[0x04] = 3; b[0x05] = b[0x06] = b[0x07] = 0;    // version = 3
    memcpy(&b[0x08], "GHOSTRIDER", 11);              // callsign [0x4]
    b[0x58] = 4; b[0x59] = b[0x5A] = b[0x5B] = 0;    // transport [0x54] = TCP/IP
    b[0x64] = 10; b[0x65] = b[0x66] = b[0x67] = 0;   // baud index [0x60]
    memcpy(&b[0x08E8], "c0a80101", 8);               // IP hex [0x8e4]
    CnInfo c;
    REQUIRE(dat_read(b.data(), b.size(), c));
    REQUIRE(c.version == 3);
    REQUIRE(memcmp(c.callsign, "GHOSTRIDER", 11) == 0);
    REQUIRE(c.transport == 4);
    REQUIRE(c.baud_index == 10);
    REQUIRE(memcmp(c.ip_hex, "c0a80101", 8) == 0);
}

TEST_CASE("dat_write is the byte-identical inverse of dat_read") {
    auto b = make_dat();
    CnInfo c;
    REQUIRE(dat_read(b.data(), b.size(), c));
    REQUIRE(dat_write(c) == b);
}

TEST_CASE("dat transport and baud tables match the spec") {
    REQUIRE(std::string(dat_transport_name(2)) == "modem");
    REQUIRE(std::string(dat_transport_name(3)) == "serial");
    REQUIRE(std::string(dat_transport_name(4)) == "TCP/IP");
    REQUIRE(dat_baud_rate(7) == 9600);
    REQUIRE(dat_baud_rate(10) == 57600);
    REQUIRE(dat_baud_rate(13) == 115200);
    REQUIRE(dat_baud_rate(99) == 0);
}

TEST_CASE("dat round-trips the real NET.DAT from the FA install") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    std::filesystem::path p = std::filesystem::path(root) / "NET.DAT";
    if (!std::filesystem::is_regular_file(p))
        SKIP("NET.DAT not present in this install");

    std::ifstream f(p, std::ios::binary | std::ios::ate);
    std::vector<uint8_t> data((size_t)f.tellg());
    f.seekg(0);
    f.read((char*)data.data(), (std::streamsize)data.size());

    CnInfo c;
    REQUIRE(dat_read(data.data(), data.size(), c));
    REQUIRE(c.version == 3);
    REQUIRE(dat_write(c) == data);
}
