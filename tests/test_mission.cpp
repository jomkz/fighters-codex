#include <catch2/catch_test_macros.hpp>
#include <fx/mission.h>
#include <cstring>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

static const char* kMinimalMission =
    "textFormat\r\n"
    "map EGYPT.T2\r\n"
    "clouds 3\r\n"
    "wind 270 15\r\n"
    "time 14 30\r\n";

TEST_CASE("mission_parse_info reads map_file") {
    auto data = bytes(kMinimalMission);
    auto info = mission_parse_info(data.data(), data.size());
    REQUIRE(info.map_file == "EGYPT.T2");
}

TEST_CASE("mission_parse_info reads clouds") {
    auto data = bytes(kMinimalMission);
    auto info = mission_parse_info(data.data(), data.size());
    REQUIRE(info.clouds == 3);
}

TEST_CASE("mission_parse_info reads wind") {
    auto data = bytes(kMinimalMission);
    auto info = mission_parse_info(data.data(), data.size());
    REQUIRE(info.wind_dir   == 270);
    REQUIRE(info.wind_speed == 15);
}

TEST_CASE("mission_parse_info reads time") {
    auto data = bytes(kMinimalMission);
    auto info = mission_parse_info(data.data(), data.size());
    REQUIRE(info.time_h == 14);
    REQUIRE(info.time_m == 30);
}

TEST_CASE("mission_roundtrip preserves bytes") {
    auto data = bytes(kMinimalMission);
    auto out  = mission_roundtrip(data.data(), data.size());
    REQUIRE(out == data);
}

TEST_CASE("mission_parse_info on empty data does not crash") {
    uint8_t buf[1] = {0};
    auto info = mission_parse_info(buf, 0);
    REQUIRE(info.map_file.empty());
}

TEST_CASE("mission_parse_info counts obj blocks") {
    const char* src =
        "textFormat\r\n"
        "map TEST.T2\r\n"
        "obj\r\n"
        "type TANK.NT\r\n"
        "pos 100 200 300\r\n"
        ".\r\n"
        "obj\r\n"
        "type MIG21.PT\r\n"
        "pos 500 600 0\r\n"
        ".\r\n";
    auto data = bytes(src);
    auto info = mission_parse_info(data.data(), data.size());
    REQUIRE(info.obj_count == 2);
}
