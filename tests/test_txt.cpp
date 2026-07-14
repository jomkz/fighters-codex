#include <catch2/catch_test_macros.hpp>
#include <fx/txt.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
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

// ---------------------------------------------------------------------------
// The directive vocabulary (#491).
//
// It is a fact read out of the executable: the text interpreter (0x47E1B0) compares each
// token against a fixed set, and anything else is just text. The parser used to call EVERY
// `.`-token a directive -- so a briefing that writes "get the .ell out" as prose (~K30.MT)
// was reported as using a directive named `.ell`.
// ---------------------------------------------------------------------------

TEST_CASE("txt_is_directive knows exactly the engine's vocabulary") {
    for (const char* d : { ".title", ".header", ".body", ".italic", "..italic", ".bold",
                           "..bold", ".underline", "..underline", ".left", ".right",
                           ".center", ".full", ".indent_off", ".indent_left", ".indent_right",
                           ".page", ".picture", ".section", ".sound", ".music", ".music_off",
                           ".button", "..button", ".dbutton", "..dbutton" })
        REQUIRE(txt_is_directive(d));

    // The engine lowercases the token before comparing.
    REQUIRE(txt_is_directive(".SECTION"));
    REQUIRE(txt_is_directive(".Header"));

    // Not directives -- the engine would render these as text.
    REQUIRE_FALSE(txt_is_directive(".ell"));
    REQUIRE_FALSE(txt_is_directive(".50"));
    REQUIRE_FALSE(txt_is_directive("..."));
    REQUIRE_FALSE(txt_is_directive(".body_"));
    REQUIRE_FALSE(txt_is_directive("header"));
}

TEST_CASE("txt_read does not call a prose token a directive") {
    // ~K30.MT, verbatim.
    auto data = bytes_of("take out the bunkers, and then get the .ell out.\r\n");
    auto doc  = txt_read(data.data(), data.size());
    REQUIRE(doc.lines[0].directives.empty());
    REQUIRE(txt_classify(doc) == TxtKind::PlainText);   // not a directive => not a template
    REQUIRE(txt_write(doc) == data);                    // and the bytes are untouched
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
// ---------------------------------------------------------------------------

TEST_CASE("TXT decode census: every shipped .TXT, against the engine's vocabulary") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int total = 0, ui = 0, campaign = 0, plain = 0;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".txt") continue;
            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            TxtDoc doc = txt_read(rec.data(), rec.size());
            REQUIRE(txt_write(doc) == rec);   // the round-trip TXT.md claimed, untested

            for (const TxtLine& line : doc.lines)
                for (const std::string& d : line.directives) {
                    INFO("directive " << d);
                    REQUIRE(txt_is_directive(d));
                }

            switch (txt_classify(doc)) {
                case TxtKind::UiTemplate:          ui++; break;
                case TxtKind::CampaignDescription: campaign++; break;
                case TxtKind::PlainText:           plain++; break;
            }
            total++;
        }
    }
    REQUIRE(total > 0);
    WARN("TXT census: " << total << " files -- " << ui << " UI templates, " << campaign
         << " campaign descriptions, " << plain << " plain text");
}
