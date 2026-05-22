#include <catch2/catch_test_macros.hpp>
#include <fx/seq.h>
#include <cstring>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

TEST_CASE("seq_parse empty data returns empty SeqFile") {
    auto seq = seq_parse(nullptr, 0);
    REQUIRE(seq.lines.empty());
    REQUIRE(seq.events.empty());
}

TEST_CASE("seq_parse / seq_serialize round-trip") {
    const char* src =
        "; opening comment\r\n"
        "\t1000 fadein\r\n"
        "\t+500 palette DATA.PAL\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());
    auto out  = seq_serialize(seq);
    REQUIRE(out == data);
}

TEST_CASE("seq_parse identifies event lines vs comment lines") {
    const char* src =
        "; comment\r\n"
        "\t200 sound GUN.11K\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());
    REQUIRE(seq.lines.size() == 2u);
    REQUIRE(seq.is_event[0] == false);
    REQUIRE(seq.is_event[1] == true);
}

TEST_CASE("seq_parse parses absolute tick count") {
    const char* src = "\t1000 fadein\r\n";
    auto seq = seq_parse(bytes(src).data(), strlen(src));
    REQUIRE(seq.events.size() == 1u);
    REQUIRE(seq.events[0].ticks    == 1000);
    REQUIRE(seq.events[0].relative == false);
    REQUIRE(seq.events[0].command  == "fadein");
}

TEST_CASE("seq_parse parses relative tick prefix") {
    const char* src = "\t+500 palette DATA.PAL\r\n";
    auto seq = seq_parse(bytes(src).data(), strlen(src));
    REQUIRE(seq.events.size() == 1u);
    REQUIRE(seq.events[0].relative == true);
    REQUIRE(seq.events[0].ticks    == 500);
    REQUIRE(seq.events[0].command  == "palette");
    REQUIRE(seq.events[0].args.size() == 1u);
    REQUIRE(seq.events[0].args[0]     == "DATA.PAL");
}

TEST_CASE("seq_serialize round-trip preserves trailer byte") {
    const char* src = "\t100 wait\r\n\x1A";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());
    auto out  = seq_serialize(seq);
    REQUIRE(out == data);
}

TEST_CASE("seq_parse preserves blank lines in round-trip") {
    const char* src = "\t0 fadein\r\n\r\n\t100 fadeout\r\n";
    auto data = bytes(src);
    auto out  = seq_serialize(seq_parse(data.data(), data.size()));
    REQUIRE(out == data);
}
