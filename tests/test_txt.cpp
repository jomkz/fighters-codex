#include <catch2/catch_test_macros.hpp>
#include <fx/txt.h>
#include <cstring>
#include <string>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes_of(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

// The documented campaign-description skeleton (TXT.md, BALTIC.TXT shape).
static const char* kCampaign =
    ".section 1\r\n.header\r\nThe Baltics 2009\r\n.body\r\n"
    "Fly missions over Estonia.\r\n.section 2\r\nEND\r\n";

TEST_CASE("txt_read splits lines and extracts directives") {
    auto data = bytes_of(kCampaign);
    auto doc = txt_read(data.data(), data.size());
    REQUIRE(doc.lines.size() == 7);
    REQUIRE(doc.lines[0].directives == std::vector<std::string>{ ".section" });
    REQUIRE(doc.lines[1].directives == std::vector<std::string>{ ".header" });
    REQUIRE(doc.lines[2].directives.empty());
    REQUIRE(doc.lines[0].crlf);
}

TEST_CASE("txt_read handles inline directives") {
    // SHWPILOT.TXT style: ".header PILOT SERVICE RECORD .body" on one line,
    // and an open/close button pair.
    auto data = bytes_of(".header PILOT SERVICE RECORD .body\r\n"
                         "NAME: .button  ..button\r\n");
    auto doc = txt_read(data.data(), data.size());
    REQUIRE(doc.lines[0].directives ==
            std::vector<std::string>{ ".header", ".body" });
    REQUIRE(doc.lines[1].directives ==
            std::vector<std::string>{ ".button", "..button" });
}

TEST_CASE("txt_write is the byte-identical inverse of txt_read") {
    SECTION("campaign description (CRLF throughout)") {
        auto data = bytes_of(kCampaign);
        REQUIRE(txt_write(txt_read(data.data(), data.size())) == data);
    }
    SECTION("no trailing newline") {
        auto data = bytes_of("line one\r\nlast line no eol");
        REQUIRE(txt_write(txt_read(data.data(), data.size())) == data);
    }
    SECTION("mixed LF and CRLF with blank lines and stray CR") {
        auto data = bytes_of("a\nb\r\n\r\n\nc\rd\r\n");
        REQUIRE(txt_write(txt_read(data.data(), data.size())) == data);
    }
    SECTION("empty input") {
        REQUIRE(txt_write(txt_read(nullptr, 0)).empty());
    }
    SECTION("non-ASCII bytes pass through") {
        std::vector<uint8_t> data = { 0xFF, 0x00, 0x80, '\n', 0x01 };
        REQUIRE(txt_write(txt_read(data.data(), data.size())) == data);
    }
}

TEST_CASE("txt_classify distinguishes the three documented uses") {
    auto camp = bytes_of(kCampaign);
    REQUIRE(txt_classify(txt_read(camp.data(), camp.size())) ==
            TxtKind::CampaignDescription);

    auto ui = bytes_of(".center\r\n.picture\r\n.page\r\n");
    REQUIRE(txt_classify(txt_read(ui.data(), ui.size())) ==
            TxtKind::UiTemplate);

    auto plain = bytes_of("CREDITS\r\n\r\nDesign: somebody\r\n");
    REQUIRE(txt_classify(txt_read(plain.data(), plain.size())) ==
            TxtKind::PlainText);
}

TEST_CASE("txt_count matches directives exactly") {
    auto data = bytes_of(".button A ..button\r\n.button B ..button\r\n");
    auto doc = txt_read(data.data(), data.size());
    REQUIRE(txt_count(doc, ".button") == 2);   // "..button" not counted
    REQUIRE(txt_count(doc, "..button") == 2);
    REQUIRE(txt_count(doc, ".section") == 0);
}
