#include <catch2/catch_test_macros.hpp>
#include <ft/plt.h>
#include <cstring>
#include <vector>

using namespace ft;

// Build a minimal PLT identity block (0xB0 bytes).
static std::vector<uint8_t> make_plt_identity(
    const char* name     = "LT James Kirk",
    const char* callsign = "ARCHER",
    const char* rank     = "2nd Lieutenant")
{
    std::vector<uint8_t> buf(0xB0, 0);
    buf[0x00] = 0x0F; // version tag
    strncpy_s((char*)buf.data() + 0x01, 63, name,     62);
    strncpy_s((char*)buf.data() + 0x40, 32, callsign, 31);
    strncpy_s((char*)buf.data() + 0xA2, 14, rank,     13);
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
