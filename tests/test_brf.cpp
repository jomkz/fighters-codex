#include <catch2/catch_test_macros.hpp>
#include <ft/brf.h>
#include <cstring>
#include <vector>

using namespace ft;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

// ---------------------------------------------------------------------------
// brf_parse_int
// ---------------------------------------------------------------------------

TEST_CASE("brf_parse_int parses decimal") {
    REQUIRE(brf_parse_int("0")    == 0);
    REQUIRE(brf_parse_int("300")  == 300);
    REQUIRE(brf_parse_int("1000") == 1000);
}

TEST_CASE("brf_parse_int parses negative via caret") {
    REQUIRE(brf_parse_int("^300")  == -300);
    REQUIRE(brf_parse_int("^1")    == -1);
    REQUIRE(brf_parse_int("^4096") == -4096);
}

TEST_CASE("brf_parse_int parses hex via dollar prefix") {
    REQUIRE(brf_parse_int("$ff")       == 255);
    REQUIRE(brf_parse_int("$7fffffff") == 0x7fffffff);
    REQUIRE(brf_parse_int("$0")        == 0);
}

TEST_CASE("brf_parse_int returns 0 for empty string") {
    REQUIRE(brf_parse_int("") == 0);
}

// ---------------------------------------------------------------------------
// brf_parse / brf_serialize round-trip
// ---------------------------------------------------------------------------

static const char* kMinimalBrf =
    "[brent's_relocatable_format]\r\n"
    "\r\n"
    ":envptr\r\n"
    "\tend\r\n"
    "\r\n"
    "\tbyte 5\r\n"
    "\tword 100\r\n"
    "\tdword 1000\r\n"
    "\tptr NULL\r\n"
    "\tend\r\n";

TEST_CASE("brf round-trip preserves raw bytes") {
    auto data = bytes(kMinimalBrf);
    auto doc  = brf_parse(data.data(), data.size());
    auto out  = brf_serialize(doc);
    REQUIRE(out == data);
}

TEST_CASE("brf_parse extracts fields correctly") {
    auto data = bytes(kMinimalBrf);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.fields.size() == 4u);
    REQUIRE(doc.fields[0].type  == "byte");
    REQUIRE(doc.fields[0].value == "5");
    REQUIRE(doc.fields[1].type  == "word");
    REQUIRE(doc.fields[1].value == "100");
    REQUIRE(doc.fields[2].type  == "dword");
    REQUIRE(doc.fields[2].value == "1000");
    REQUIRE(doc.fields[3].type  == "ptr");
    REQUIRE(doc.fields[3].value == "NULL");
}

TEST_CASE("brf_parse extracts pointer table") {
    auto data = bytes(kMinimalBrf);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.tables.size() == 1u);
    REQUIRE(doc.tables[0].name == "envptr");
}

TEST_CASE("brf_parse on empty data returns empty doc") {
    uint8_t buf[1] = {0};
    auto doc = brf_parse(buf, 0);
    REQUIRE(doc.fields.empty());
    REQUIRE(doc.tables.empty());
}

TEST_CASE("brf_parse handles inline comments after field values") {
    const char* src =
        "[brent's_relocatable_format]\r\n"
        "\tbyte 42 ; this is a comment\r\n"
        "\tend\r\n";
    auto data = bytes(src);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.fields.size() == 1u);
    REQUIRE(doc.fields[0].value == "42");
}

TEST_CASE("brf find_table returns correct table") {
    auto data = bytes(kMinimalBrf);
    auto doc  = brf_parse(data.data(), data.size());
    const BrfTable* t = doc.find_table("envptr");
    REQUIRE(t != nullptr);
    REQUIRE(t->name == "envptr");
}

TEST_CASE("brf find_table returns nullptr for unknown name") {
    auto data = bytes(kMinimalBrf);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.find_table("nosuchptr") == nullptr);
}
