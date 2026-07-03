#include <catch2/catch_test_macros.hpp>
#include <fx/inf.h>
#include <cstring>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

static const char* kSample =
    ".body .right\r\n"
    "Jane's All The World's Aircraft\r\n"
    "\r\n"
    ".title .center\r\n"
    "A-10 THUNDERBOLT II\r\n"
    ".body .left\r\n"
    "\r\n"
    "TITLE:\r\n"
    "FAIRCHILD REPUBLIC THUNDERBOLT II\r\n"
    "\r\n"
    "LENGTH (m): 16.26\r\n"
    "WINGSPAN (m) 17.53\r\n";

TEST_CASE("inf_parse empty data returns invalid InfFile") {
    auto inf = inf_parse(nullptr, 0);
    REQUIRE_FALSE(inf.valid);
    REQUIRE(inf.sections.empty());
}

TEST_CASE("inf_parse sections and directives") {
    auto data = bytes(kSample);
    auto inf  = inf_parse(data.data(), data.size());
    REQUIRE(inf.valid);
    REQUIRE(inf.sections.size() == 3u);
    REQUIRE(inf.sections[0].directive == ".body .right");
    REQUIRE(inf.sections[0].text      == "Jane's All The World's Aircraft");
    REQUIRE(inf.sections[1].directive == ".title .center");
    REQUIRE(inf.sections[1].text      == "A-10 THUNDERBOLT II");
    REQUIRE(inf.sections[2].directive == ".body .left");
}

TEST_CASE("inf_parse extracts stats in both separator formats") {
    auto data = bytes(kSample);
    auto inf  = inf_parse(data.data(), data.size());
    REQUIRE(inf.stats.size() == 2u);
    REQUIRE(inf.stats.at("LENGTH (m)")   == "16.26");
    REQUIRE(inf.stats.at("WINGSPAN (m)") == "17.53");
    // Extraction is a view: the lines stay in the section text.
    REQUIRE(inf.sections[2].text.find("LENGTH (m): 16.26") != std::string::npos);
}

TEST_CASE("inf_parse / inf_serialize round-trip is byte-identical") {
    auto data = bytes(kSample);
    auto out  = inf_serialize(inf_parse(data.data(), data.size()));
    REQUIRE(out == data);
}

TEST_CASE("inf round-trip preserves LF-only endings and unterminated tail") {
    const char* src = ".body .left\nplain\ntext\nno final newline";
    auto data = bytes(src);
    auto out  = inf_serialize(inf_parse(data.data(), data.size()));
    REQUIRE(out == data);
}

TEST_CASE("inf round-trip preserves leading text before any directive") {
    const char* src = "stray header\r\n.body .left\r\nbody\r\n";
    auto data = bytes(src);
    auto inf  = inf_parse(data.data(), data.size());
    REQUIRE(inf.sections.size() == 2u);
    REQUIRE(inf.sections[0].directive.empty());
    REQUIRE(inf_serialize(inf) == data);
}

TEST_CASE("inf round-trip preserves a DOS EOF trailer byte") {
    const char* src = ".body .left\r\ntext\r\n\x1A";
    auto data = bytes(src);
    auto out  = inf_serialize(inf_parse(data.data(), data.size()));
    REQUIRE(out == data);
}

TEST_CASE("inf_rebuild_section recomposes CRLF and keeps the blank-line tail") {
    auto data = bytes(kSample);
    auto inf  = inf_parse(data.data(), data.size());

    // Section 0 originally ends with a blank line before the next directive.
    auto& s = inf.sections[0];
    s.text  = "Edited header";
    inf_rebuild_section(s);
    REQUIRE(s.raw == ".body .right\r\nEdited header\r\n\r\n");

    // Section 1 has no blank-line tail; the rebuild must not add one.
    auto& t = inf.sections[1];
    t.text  = "A-10 WARTHOG";
    inf_rebuild_section(t);
    REQUIRE(t.raw == ".title .center\r\nA-10 WARTHOG\r\n");
}

TEST_CASE("editor-style section edit round-trips, untouched sections byte-preserved") {
    auto data = bytes(kSample);
    auto inf  = inf_parse(data.data(), data.size());

    std::string sec2_before = inf.sections[2].raw;
    inf.sections[1].text = "A-10 WARTHOG";
    inf_rebuild_section(inf.sections[1]);

    auto out = inf_serialize(inf);
    auto re  = inf_parse(out.data(), out.size());
    REQUIRE(re.sections.size() == 3u);
    REQUIRE(re.sections[1].text == "A-10 WARTHOG");
    REQUIRE(re.sections[2].raw  == sec2_before);
    // Stable across a second pass
    REQUIRE(inf_serialize(re) == out);
}

TEST_CASE("editor-style new section rebuild defaults to a blank-line tail") {
    InfSection s;
    s.directive = ".body .left";
    s.text      = "NEW SECTION";
    inf_rebuild_section(s);
    REQUIRE(s.raw == ".body .left\r\nNEW SECTION\r\n\r\n");
}
