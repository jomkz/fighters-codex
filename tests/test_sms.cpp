// SMS symbol map parser — record layout, error paths, install FA.SMS (#113).
#include <catch2/catch_test_macros.hpp>
#include <fx/sms.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace fx;

namespace {

void w32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++) v.push_back((uint8_t)(x >> (8 * i)));
}

// count, then {str_off, va} records, then the packed string table.
std::vector<uint8_t> tiny_sms() {
    std::vector<uint8_t> f;
    w32(f, 3);
    w32(f, 0); w32(f, 0x401000);  // "Foo"
    w32(f, 4); w32(f, 0x500000);  // "Bar"
    w32(f, 8); w32(f, 0x42);      // "Baz"
    const char tab[] = "Foo\0Bar\0Baz";
    f.insert(f.end(), tab, tab + sizeof(tab));  // sizeof includes the last NUL
    return f;
}

}  // namespace

TEST_CASE("sms_parse reads {str_off, va} records against the string table") {
    auto f = tiny_sms();
    auto syms = sms_parse(f.data(), f.size());
    REQUIRE(syms.size() == 3u);
    // Field order pinned: a str_off/va swap would put 0x401000 in the name slot.
    CHECK(syms[0].va == 0x401000u);
    CHECK(syms[0].name == "Foo");
    CHECK(syms[1].va == 0x500000u);
    CHECK(syms[1].name == "Bar");
    CHECK(syms[2].name == "Baz");
}

TEST_CASE("sms_parse clamps an unterminated tail string") {
    auto f = tiny_sms();
    f.pop_back();  // drop the final NUL
    auto syms = sms_parse(f.data(), f.size());
    REQUIRE(syms.size() == 3u);
    CHECK(syms[2].name == "Baz");  // strnlen-clamped to the table end
}

TEST_CASE("sms_parse rejects undersized, empty, and truncated maps") {
    auto f = tiny_sms();
    CHECK(sms_parse(f.data(), 3).empty());       // shorter than the count
    std::vector<uint8_t> zero;
    w32(zero, 0);
    CHECK(sms_parse(zero.data(), zero.size()).empty());
    CHECK(sms_parse(f.data(), 4 + 2 * 8).empty());  // records cut short

    // A record pointing past the string table is skipped, not fabricated.
    auto bad = tiny_sms();
    bad[4] = (uint8_t)0xFF;  // record 0 str_off -> far past the table
    auto syms = sms_parse(bad.data(), bad.size());
    REQUIRE(syms.size() == 2u);
    CHECK(syms[0].name == "Bar");
}

TEST_CASE("sms_parse reads the install's FA.SMS per the spec's claims") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // FA.SMS ships loose in the install root (case varies across installs).
    std::vector<uint8_t> file;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().filename().string()) != "fa.sms") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        file.resize((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)file.data(), (std::streamsize)file.size());
        break;
    }
    if (file.empty()) SKIP("FA.SMS not present in this install");

    auto syms = sms_parse(file.data(), file.size());
    // The spec's documented facts (docs/fa/formats/SMS.md): 3,829 records
    // spanning the game executable's image.
    REQUIRE(syms.size() == 3829u);
    uint32_t lo = UINT32_MAX, hi = 0;
    for (const auto& s : syms) {
        CHECK_FALSE(s.name.empty());
        lo = std::min(lo, s.va);
        hi = std::max(hi, s.va);
    }
    CHECK(lo == 0x00401000u);
    CHECK(hi == 0x005937E0u);
    // Cross-check against the reconstruction: HUDDrawAlt's VA in
    // docs/fa/hud.md came from these symbols.
    auto it = std::find_if(syms.begin(), syms.end(), [](const SmsSymbol& s) {
        return s.name == "?HUDDrawAlt@@YIXXZ";
    });
    REQUIRE(it != syms.end());
    CHECK(it->va == 0x408420u);
}
