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

// The fxs SEQ editor mutates the three parallel vectors directly and
// re-serializes; these tests pin that contract (issue #92).

TEST_CASE("editor-style event insert round-trips and leaves other lines intact") {
    const char* src =
        "; comment\r\n"
        "\t1000\tfadein\r\n"
        "\t+500\tpalette\t\"DATA\"\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());

    // Insert after the fadein (lines[] index 1), the editor way.
    const char* body = "+1\tsound";
    seq.lines.insert(seq.lines.begin() + 2, std::string("\t") + body);
    seq.is_event.insert(seq.is_event.begin() + 2, true);
    auto line = bytes("\t+1\tsound\r\n");
    seq.events.insert(seq.events.begin() + 2,
                      seq_parse(line.data(), line.size()).events[0]);

    auto out = seq_serialize(seq);
    auto re  = seq_parse(out.data(), out.size());
    REQUIRE(re.lines.size() == 4u);
    REQUIRE(re.events[2].relative == true);
    REQUIRE(re.events[2].ticks    == 1);
    REQUIRE(re.events[2].command  == "sound");
    // Untouched lines byte-preserved
    REQUIRE(re.lines[0] == "; comment");
    REQUIRE(re.lines[1] == "\t1000\tfadein");
    REQUIRE(re.lines[3] == "\t+500\tpalette\t\"DATA\"");
    // Serialization is stable across a second pass
    REQUIRE(seq_serialize(re) == out);
}

TEST_CASE("editor-style event delete round-trips") {
    const char* src =
        "\t0\tfadein\r\n"
        "\t100\tsound\tGUN\r\n"
        "\t200\tfadeout\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());

    seq.lines.erase(seq.lines.begin() + 1);
    seq.is_event.erase(seq.is_event.begin() + 1);
    seq.events.erase(seq.events.begin() + 1);

    auto out = seq_serialize(seq);
    auto re  = seq_parse(out.data(), out.size());
    REQUIRE(re.lines.size() == 2u);
    REQUIRE(re.events[0].command == "fadein");
    REQUIRE(re.events[1].command == "fadeout");
    REQUIRE(seq_serialize(re) == out);
}

TEST_CASE("editor-composed tab-separated line parses to full metadata") {
    // The editor rebuilds edited rows as <time>\t[sync\t]<command>\t<args>
    const char* src = "\t+40\tsync\tsound\t\"BOOM\" 22\r\n";
    auto seq = seq_parse(bytes(src).data(), strlen(src));
    REQUIRE(seq.events.size() == 1u);
    const auto& ev = seq.events[0];
    REQUIRE(ev.relative == true);
    REQUIRE(ev.ticks    == 40);
    REQUIRE(ev.sync     == true);
    REQUIRE(ev.command  == "sound");
    REQUIRE(ev.args.size() == 2u);
    REQUIRE(ev.args[0]     == "\"BOOM\"");
    REQUIRE(ev.args[1]     == "22");
}
