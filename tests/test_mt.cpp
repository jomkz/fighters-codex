#include <catch2/catch_test_macros.hpp>
#include <fx/mt.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <string>
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

// The identifier line the OTHER 263 files write: no dashes at all. The only fixture here
// used to be the `--AB01` form -- the shape 100 of the 363 files happen to use -- and the
// codec required it, so it lost the ID on the other 263 and shifted every field up by one.
static const char* kMtNoDashes =
    ".section 1\r\n"
    "AB01  (~b01)\r\n"
    "LIVE WIRE  (The Baltics)\r\n"
    "Campaign Mission  (ATFGOLD)\r\n"
    ".section 2\r\n"
    "LIVE  WIRE\r\n";

TEST_CASE("mt_info reads an identifier line with no dashes (263 of the 363 files)") {
    auto data = bytes_of(kMtNoDashes);
    auto info = mt_info(txt_read(data.data(), data.size()));
    REQUIRE(info.mission_id   == "AB01");
    REQUIRE(info.source_name  == "~b01");
    REQUIRE(info.title        == "LIVE WIRE  (The Baltics)");
    REQUIRE(info.mission_type == "Campaign Mission  (ATFGOLD)");
}

TEST_CASE("mt_info reads an identifier line with a single dash (~FANOTH.MT)") {
    auto data = bytes_of(".section 1\r\n-RB12  (Panama)\r\n.section 2\r\nX\r\n");
    auto info = mt_info(txt_read(data.data(), data.size()));
    REQUIRE(info.mission_id  == "RB12");
    REQUIRE(info.source_name == "Panama");   // an annotation, not a filename
    REQUIRE(info.title.empty());             // the file carries none; do not invent one
}

// QUICK.MT: a section-1 line with no parenthesised annotation is a TITLE, not an identifier.
TEST_CASE("mt_info does not mistake a plain title for an identifier") {
    auto data = bytes_of(".section 1\r\nQUICK MISSION\r\n"
                         "This is the last mission you created.\r\n.section 2\r\nX\r\n");
    auto info = mt_info(txt_read(data.data(), data.size()));
    REQUIRE(info.mission_id.empty());
    REQUIRE(info.title == "QUICK MISSION");
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// MT.md claimed "all 363 .MT files in FA_2.LIB round-trip byte-identically" and NO test
// opened one. They do round-trip -- txt_write replays the file's own lines, so they always
// would have, bug or no bug. That is exactly why the round-trip proved nothing: the codec
// was dropping the mission ID on 263 of them at the same time.
// ---------------------------------------------------------------------------

TEST_CASE("MT decode census: every shipped briefing, against the shape it declares") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int total = 0, with_id = 0;
    std::map<size_t, int> section_counts;

    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".mt") continue;
            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            TxtDoc doc = txt_read(rec.data(), rec.size());

            // 1. The round-trip MT.md claimed and never tested.
            REQUIRE(txt_write(doc) == rec);

            // 2. Every directive the decoder reports is one the ENGINE knows. A briefing
            //    writes "get the .ell out" as prose (~K30.MT); that is not a directive, and
            //    a tokenizer that calls every `.`-token one says it is.
            for (const TxtLine& line : doc.lines)
                for (const std::string& d : line.directives) {
                    INFO("directive " << d);
                    REQUIRE(txt_is_directive(d));
                }

            // 3. Every briefing is sectioned, and the sections are the documented 4 or 5.
            MtInfo info = mt_info(doc);
            REQUIRE(info.sections >= 1);
            section_counts[info.sections]++;

            // 4. Where the file carries an identifier line, we read it -- whether or not the
            //    designers decorated it with dashes.
            if (!info.mission_id.empty()) {
                with_id++;
                REQUIRE(info.mission_id.find(' ') == std::string::npos);
                REQUIRE(info.mission_id[0] != '-');   // the ID never keeps the decoration
            }
            total++;
        }
    }
    REQUIRE(total > 0);
    // The corpus: 345 of the 363 carry an identifier line; the rest (QUICK, ~F*NOTH) have a
    // shorter section 1 and are not made to invent one.
    REQUIRE(with_id > total * 9 / 10);

    std::string dist;
    for (auto& [n, c] : section_counts)
        dist += " " + std::to_string(n) + "x" + std::to_string(c);
    WARN("MT census: " << total << " briefings, " << with_id << " with an identifier line;"
         << " section counts:" << dist);
}
