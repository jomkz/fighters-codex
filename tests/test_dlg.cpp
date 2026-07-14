#include <catch2/catch_test_macros.hpp>
#include <fx/dlg.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>

using namespace fx;

// Minimal MZ + "PL" image with one CODE section (the DLG/CAM container
// family; layout mirrors tests/test_pe.cpp).
static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v; b[off + 1] = (uint8_t)(v >> 8);
}
static void put32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)v;             b[off + 1] = (uint8_t)(v >> 8);
    b[off + 2] = (uint8_t)(v >> 16); b[off + 3] = (uint8_t)(v >> 24);
}

static std::vector<uint8_t> make_dlg(const std::vector<uint8_t>& code) {
    const uint32_t pe_off = 0x40;
    const uint32_t raw_ptr = pe_off + 24 + 40;
    std::vector<uint8_t> b(raw_ptr, 0);
    b[0] = 'M'; b[1] = 'Z';
    put32(b, 0x3C, pe_off);
    b[pe_off] = 'P'; b[pe_off + 1] = 'L';
    put16(b, pe_off + 6, 1);
    put16(b, pe_off + 20, 0);
    const uint32_t sec = pe_off + 24;
    memcpy(&b[sec], "CODE", 4);
    put32(b, sec + 8, (uint32_t)code.size());
    put32(b, sec + 12, 0x1000);
    put32(b, sec + 16, (uint32_t)code.size());
    put32(b, sec + 20, raw_ptr);
    b.insert(b.end(), code.begin(), code.end());
    return b;
}

TEST_CASE("dlg_info validates the container and finds the dispatch table") {
    std::vector<uint8_t> code = { 0x10, 0x12, 0x00, 0x00 };  // header thunk VA
    auto dlg = make_dlg(code);
    auto info = dlg_info(dlg.data(), dlg.size());
    REQUIRE(info.valid);
    REQUIRE(info.code.vma == 0x1000);
    REQUIRE(info.code.size == code.size());
}

TEST_CASE("dlg_strings extracts the embedded control labels") {
    std::vector<uint8_t> code;
    for (const char* s : { "Play Single Mission", "Start New Campaign" }) {
        code.insert(code.end(), s, s + strlen(s));
        code.push_back(0);
    }
    auto dlg = make_dlg(code);
    auto strings = dlg_strings(dlg.data(), dlg.size());
    REQUIRE(std::count(strings.begin(), strings.end(),
                       std::string("Play Single Mission")) == 1);
    REQUIRE(std::count(strings.begin(), strings.end(),
                       std::string("Start New Campaign")) == 1);
}

TEST_CASE("dlg_info rejects a non-DLL payload") {
    std::vector<uint8_t> junk = { 'D', 'L', 'G', 0, 1, 2, 3, 4 };
    REQUIRE_FALSE(dlg_info(junk.data(), junk.size()).valid);
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// DLG.md claimed "all 92 shipped dialogs validate and surface their label strings" and no
// test opened one. It also carried a hand-written table of 8 imports as the control types.
// The import table is DECODABLE -- and it holds 34 distinct symbols, not 8.
// ---------------------------------------------------------------------------

namespace {

std::set<std::string> db_names() {
    std::set<std::string> names;
    for (const auto& de : std::filesystem::directory_iterator(FX_DB_DIR "/symbols")) {
        if (de.path().extension() != ".csv") continue;
        std::ifstream f(de.path());
        std::string line;
        std::getline(f, line);  // header
        while (std::getline(f, line)) {
            size_t a = line.find(',');
            if (a == std::string::npos) continue;
            size_t b = line.find(',', a + 1);
            if (b == std::string::npos) continue;
            size_t c = line.find(',', b + 1);
            std::string n = line.substr(b + 1, c - b - 1);
            if (!n.empty()) names.insert(n);
        }
    }
    return names;
}

}  // namespace

TEST_CASE("DLG decode census: every shipped dialog, and what it imports") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    const std::set<std::string> db = db_names();
    REQUIRE_FALSE(db.empty());

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int files = 0, with_strings = 0;
    std::map<std::string, int> used;

    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".dlg") continue;
            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            // 1. The container claim DLG.md made and never tested.
            DlgInfo info = dlg_info(rec.data(), rec.size());
            REQUIRE(info.valid);
            REQUIRE(info.code.data != nullptr);
            REQUIRE(info.code.size > 0);

            if (!dlg_strings(rec.data(), rec.size()).empty()) with_strings++;

            // 2. Every dialog imports from the game executable, and every name it imports
            //    is one we have claimed. An overlay naming a symbol db/ does not know is a
            //    hole in the reconstruction -- that is how _okString came to be claimed.
            auto imports = dlg_imports(rec.data(), rec.size());
            REQUIRE_FALSE(imports.empty());
            for (const PeImport& im : imports) {
                INFO("import " << im.module << " : " << im.name);
                REQUIRE(im.module == "main.dll");   // the game executable, always
                REQUIRE_FALSE(im.name.empty());     // never by ordinal
                REQUIRE(db.count(im.name) == 1);
                used[im.name]++;
            }
            files++;
        }
    }
    REQUIRE(files > 0);
    REQUIRE(with_strings == files);   // the "surface their label strings" claim

    std::string top;
    for (auto& [n, c] : used)
        if (c >= 20) top += " " + n + "=" + std::to_string(c);
    WARN("DLG census: " << files << " dialogs, " << used.size()
         << " distinct imports from main.dll;" << top);
}
