#include <catch2/catch_test_macros.hpp>
#include <fx/mission.h>
#include <fx/ealib.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
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

// #156: full object list + waypoint blocks with typed geometry and preserved
// fields.
TEST_CASE("mission_parse_objects extracts objects, fields, and waypoint blocks") {
    const char* src =
        "textFormat\r\n"
        "map TEST.T2\r\n"
        "obj\r\n"
        "type F22.PT\r\n"
        "pos 100 0 200\r\n"
        "angle 1 2 3\r\n"
        "nationality2 130\r\n"
        "flags $411\r\n"
        "alias -1\r\n"
        ".\r\n"
        "obj\r\n"
        "type TRUCK.NT\r\n"
        "pos 300 0 400\r\n"
        ".\r\n"
        "waypoint2 2\r\n"
        "w_index 0\r\n"
        "w_goal 1\r\n"
        "w_pos2 0 0 500 0 600\r\n"
        "w_index 1\r\n"
        "w_goal 2\r\n"
        ".\r\n";
    auto data = bytes(src);
    auto mo = mission_parse_objects(data.data(), data.size());

    REQUIRE(mo.objects.size() == 2);
    const auto& a = mo.objects[0];
    REQUIRE(a.type_file == "F22.PT");
    REQUIRE(a.pos[0] == 100);
    REQUIRE(a.pos[2] == 200);
    REQUIRE(a.angle[0] == 1);
    REQUIRE(a.angle[2] == 3);
    REQUIRE(a.get("nationality2") != nullptr);
    REQUIRE((*a.get("nationality2"))[0] == "130");
    REQUIRE((*a.get("flags"))[0] == "$411");
    REQUIRE((*a.get("alias"))[0] == "-1");
    REQUIRE(a.get("does_not_exist") == nullptr);
    REQUIRE(mo.objects[1].type_file == "TRUCK.NT");

    REQUIRE(mo.waypoint_blocks.size() == 1);
    const auto& blk = mo.waypoint_blocks[0];
    REQUIRE(blk.count == 2);
    REQUIRE(blk.waypoints.size() == 2);
    REQUIRE(blk.waypoints[0].index == 0);
    REQUIRE((*blk.waypoints[0].get("w_goal"))[0] == "1");
    REQUIRE(blk.waypoints[0].get("w_pos2") != nullptr);
    REQUIRE(blk.waypoints[0].get("w_pos2")->size() == 5);
    REQUIRE((*blk.waypoints[0].get("w_pos2"))[2] == "500");
    REQUIRE(blk.waypoints[1].index == 1);
}

// #156 acceptance: the object list is consistent with the obj counter, and each
// waypoint block's parsed length matches its `waypoint2 N` header, for every
// stock mission. Real-asset mode; runs where FX_FA_ROOT points at an install.
TEST_CASE("mission_parse_objects is consistent across every stock mission") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    auto ends_with = [](const std::string& s, const char* suf) {
        std::string t = suf;
        return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
    };

    int total = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            std::string name = lower(std::string(e.name));
            if (!ends_with(name, ".m") && !ends_with(name, ".mm")) continue;
            auto d = ealib_extract(lib.data(), lib.size(), e, true);
            if (d.empty()) continue;
            auto info = mission_parse_info(d.data(), d.size());
            auto mo = mission_parse_objects(d.data(), d.size());
            INFO(e.name);
            REQUIRE((int)mo.objects.size() == info.obj_count);
            for (const auto& b : mo.waypoint_blocks)
                REQUIRE((int)b.waypoints.size() == b.count);
            total++;
        }
    }
    if (total == 0) SKIP("no mission files in this install");
    WARN("parsed " << total << " stock missions");
}
