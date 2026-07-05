#include <catch2/catch_test_macros.hpp>
#include <fx/ssf.h>
#include <cstring>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes_of(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

// SETUP.SSF-shaped sample (SSF.md).
static const char* kSsf =
    "# master installer script\r\n"
    "COMPANY_NAME \"Jane's Combat Simulations\"\r\n"
    "APP_NAME \"Fighters Anthology\"\r\n"
    "DEFAULT_PATH \"\\JANES\\Fighters Anthology\"\r\n"
    "\r\n"
    "INSTALL_SCRIPT \"MINSTALL.SSF\", \":0409:Minimal Install - Midi Music:\"\r\n"
    "INSTALL_FILES \"FA_1.LIB\", \"FA_LIBS\", \"[INSTALL_PATH]\"\r\n"
    "DIRECTX \"label\", 5, 0\r\n";

TEST_CASE("ssf_read extracts keywords and arguments") {
    auto data = bytes_of(kSsf);
    auto doc = ssf_read(data.data(), data.size());
    REQUIRE(doc.statements.size() == 6);  // comment + blank line skipped
    REQUIRE(doc.statements[0].keyword == "COMPANY_NAME");
    REQUIRE(doc.statements[0].args ==
            std::vector<std::string>{ "Jane's Combat Simulations" });
    REQUIRE(doc.statements[4].keyword == "INSTALL_FILES");
    REQUIRE(doc.statements[4].args ==
            std::vector<std::string>{ "FA_1.LIB", "FA_LIBS", "[INSTALL_PATH]" });
    REQUIRE(doc.statements[5].keyword == "DIRECTX");
    REQUIRE(doc.statements[5].args ==
            std::vector<std::string>{ "label", "5", "0" });
}

TEST_CASE("ssf statements remember their source line") {
    auto data = bytes_of(kSsf);
    auto doc = ssf_read(data.data(), data.size());
    REQUIRE(doc.statements[0].line == 1);  // after the comment line
    REQUIRE(doc.statements[3].line == 5);  // after the blank line
}

TEST_CASE("ssf_write is the byte-identical inverse of ssf_read") {
    auto data = bytes_of(kSsf);
    REQUIRE(ssf_write(ssf_read(data.data(), data.size())) == data);

    auto odd = bytes_of("# only a comment, no eol");
    REQUIRE(ssf_write(ssf_read(odd.data(), odd.size())) == odd);
}

TEST_CASE("ssf_read ignores non-keyword lines") {
    auto data = bytes_of("lowercase not a keyword\r\n# comment\r\n\r\n");
    auto doc = ssf_read(data.data(), data.size());
    REQUIRE(doc.statements.empty());
    REQUIRE(ssf_write(doc) == data);
}
