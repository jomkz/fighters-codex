#include <catch2/catch_test_macros.hpp>
#include <fx/mt.h>
#include <cstring>
#include <vector>

using namespace fx;

// Real-shaped sample (BEXTRA01.MT header, MT.md section semantics).
static const char* kMt =
    ".section 1\r\n"
    "--AB01  (bextra01)\r\n"
    "WEAPONS  FREE  (The Baltics)\r\n"
    "Single Player Mission  (ATFGOLD)\r\n"
    ".section 2\r\n"
    ".center .underline .header\r\n"
    "WEAPONS FREE\r\n"
    "..underline .left .body\r\n"
    "TARTU AIRBASE\r\n"
    ".section 3\r\n"
    "Good work.\r\n"
    ".section 4\r\n"
    "You failed.\r\n"
    ".section 5\r\n"
    "Objectives incomplete.\r\n";

static std::vector<uint8_t> bytes_of(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

TEST_CASE("mt_info extracts the section-1 header facts") {
    auto data = bytes_of(kMt);
    auto info = mt_info(txt_read(data.data(), data.size()));
    REQUIRE(info.mission_id == "AB01");
    REQUIRE(info.source_name == "bextra01");
    REQUIRE(info.title == "WEAPONS  FREE  (The Baltics)");
    REQUIRE(info.mission_type == "Single Player Mission  (ATFGOLD)");
    REQUIRE(info.sections == 5);
}

TEST_CASE("mt round-trips byte-identically through the txt engine") {
    auto data = bytes_of(kMt);
    REQUIRE(txt_write(txt_read(data.data(), data.size())) == data);
}

TEST_CASE("mt_info degrades on files without the identifier line") {
    auto data = bytes_of(".section 1\r\nJust a title\r\n.section 2\r\nEND\r\n");
    auto info = mt_info(txt_read(data.data(), data.size()));
    REQUIRE(info.mission_id.empty());
    REQUIRE(info.title == "Just a title");
    REQUIRE(info.sections == 2);
}

TEST_CASE("mt_info on non-MT content is empty but safe") {
    auto data = bytes_of("no directives at all\r\n");
    auto info = mt_info(txt_read(data.data(), data.size()));
    REQUIRE(info.mission_id.empty());
    REQUIRE(info.sections == 0);
}
