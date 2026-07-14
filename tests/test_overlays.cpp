// The overlay import census (#491).
//
// FA's data files are PE overlays: MZ + Phar Lap "PL". Twelve formats use the container, and
// the ones that behave rather than merely describe IMPORT FROM THE GAME EXECUTABLE. A .DLG
// pulls in `_DrawListBox`, a .MC pulls in `@Alive@4` and `_MSGSendChatter@24`, a .BI pulls in
// the 55 `_CTEval_*`/`_CTDo_*` primitives of the AI language. So an overlay's import table
// says what it is MADE OF, in the engine's own names -- and every one of those names should be
// a symbol we have claimed.
//
// HOW THE NAMES ARE RESOLVED, AND WHY NOT BY NAME.
//
// The obvious check -- "is this string in db/symbols?" -- is unsound twice over:
//
//   * db/ holds symbols for SEVEN binaries, and IP.EXE's VA range (0x401000-0x4BD800) overlaps
//     FA.EXE's completely. Matching a name against all of them lets an FA.EXE overlay
//     "resolve" against an IP.EXE symbol. That is the shape of the bug that shipped a wrong
//     signature in #477: an address alone does not name a function, and neither does a name
//     alone once you have more than one program.
//   * FA.SMS names ONE address six ways. 0x50CE80 is _cg, _cj, _cn, _co, _cp and _curThing --
//     the class-typed views of the current-entity mirror. db keys symbols by VA and requires
//     them globally unique, so five of those six names appear in no row at all, and a
//     name-match would call them unclaimed.
//
// So resolve the way the ENGINE does: through the symbol map. FA.SMS is the executable's own
// name -> VA table (it is what SMAddress reads), so an import name goes through it to an
// address, and THAT is what db/ must claim. Address-based, alias-proof, and it cannot cross a
// binary boundary.

#include <catch2/catch_test_macros.hpp>

#include <fx/dlg.h>
#include <fx/ealib.h>
#include <fx/pe.h>
#include <fx/sms.h>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace fx;

namespace {

namespace fs = std::filesystem;

std::vector<uint8_t> read_all(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return {};
    std::vector<uint8_t> d((size_t)f.tellg());
    f.seekg(0);
    f.read((char*)d.data(), (std::streamsize)d.size());
    return d;
}

std::string lower_str(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Every VA db/symbols claims for FA.EXE -- and ONLY FA.EXE. The other six binaries in db/
// have overlapping address spaces, and an overlay imports from the game executable.
std::set<uint32_t> fa_exe_claimed_vas() {
    // subsystems.csv says which slug belongs to which binary; symbols/<slug>.csv holds its rows.
    std::set<std::string> fa_slugs;
    {
        std::ifstream f(FX_DB_DIR "/subsystems.csv");
        std::string line;
        std::getline(f, line);  // header
        while (std::getline(f, line)) {
            size_t a = line.find(',');
            if (a == std::string::npos) continue;
            const std::string slug = line.substr(0, a);
            // binary is the 3rd column
            size_t b = line.find(',', a + 1);
            if (b == std::string::npos) continue;
            size_t c = line.find(',', b + 1);
            if (c == std::string::npos) continue;
            if (line.substr(b + 1, c - b - 1) == "FA.EXE") fa_slugs.insert(slug);
        }
    }
    REQUIRE_FALSE(fa_slugs.empty());

    std::set<uint32_t> vas;
    for (const std::string& slug : fa_slugs) {
        std::ifstream f(std::string(FX_DB_DIR "/symbols/") + slug + ".csv");
        if (!f) continue;
        std::string line;
        std::getline(f, line);  // header
        while (std::getline(f, line)) {
            if (line.rfind("0x", 0) != 0) continue;
            vas.insert((uint32_t)std::strtoul(line.c_str(), nullptr, 16));
        }
    }
    return vas;
}

// FA.SMS: the executable's own name -> VA map, the table SMAddress reads.
std::map<std::string, uint32_t> sms_map(const char* root) {
    std::map<std::string, uint32_t> m;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower_str(de.path().filename().string()) != "fa.sms") continue;
        auto data = read_all(de.path());
        for (const SmsSymbol& s : sms_parse(data.data(), data.size()))
            m.emplace(s.name, s.va);   // first wins; aliases share a VA anyway
    }
    return m;
}

}  // namespace

TEST_CASE("overlay import census: every shipped overlay, resolved through the symbol map") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");

    const std::map<std::string, uint32_t> sms = sms_map(root);
    if (sms.empty())
        SKIP("no FA.SMS in this install");
    const std::set<uint32_t> claimed = fa_exe_claimed_vas();
    REQUIRE_FALSE(claimed.empty());

    // The behavioural overlays: they call into the engine.
    const std::set<std::string> kImporting = { ".dlg", ".bi", ".cam", ".mc", ".lay" };
    // The data overlays: CODE is a table, and they import NOTHING. That is a fact about the
    // format family, so assert it rather than assume it.
    const std::set<std::string> kDataOnly = { ".mnu", ".pts", ".hgr", ".hud", ".mus" };

    std::map<std::string, int> files, imports_per_ext;
    std::map<std::string, std::set<std::string>> vocab;
    int unresolved = 0, unclaimed = 0;

    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower_str(de.path().extension().string()) != ".lib") continue;
        std::vector<uint8_t> lib = read_all(de.path());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            const std::string ext = lower_str(fs::path(e.name).extension().string());
            const bool importing = kImporting.count(ext) != 0;
            if (!importing && !kDataOnly.count(ext)) continue;

            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            auto imports = pe_imports(rec.data(), rec.size());

            if (!importing) {
                // A data overlay has no import directory at all.
                REQUIRE(imports.empty());
                files[ext]++;
                continue;
            }

            REQUIRE_FALSE(imports.empty());
            for (const PeImport& im : imports) {
                INFO("import " << im.module << " : " << im.name);
                REQUIRE(im.module == "main.dll");   // the game executable, always
                REQUIRE_FALSE(im.name.empty());     // never by ordinal

                // 1. The name resolves through the engine's own symbol map.
                auto it = sms.find(im.name);
                if (it == sms.end()) { unresolved++; continue; }

                // 2. And the address it resolves to is one db/ claims for FA.EXE.
                if (!claimed.count(it->second)) { unclaimed++; continue; }

                vocab[ext].insert(im.name);
                imports_per_ext[ext]++;
            }
            files[ext]++;
        }
    }

    REQUIRE_FALSE(files.empty());
    REQUIRE(unresolved == 0);   // an import FA.SMS cannot resolve would not load
    REQUIRE(unclaimed == 0);    // ...and one db/ has not claimed is a hole in the reconstruction

    std::string summary;
    for (const auto& [ext, n] : files) {
        summary += " " + ext + "=" + std::to_string(n);
        if (vocab.count(ext)) summary += "(" + std::to_string(vocab[ext].size()) + " imports)";
    }
    WARN("overlay census:" << summary);
}

// The six-way alias, which is why the census resolves by ADDRESS and not by name.
TEST_CASE("FA.SMS names one address six ways, and db claims it once") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    const std::map<std::string, uint32_t> sms = sms_map(root);
    if (sms.empty()) SKIP("no FA.SMS in this install");

    // The class-typed views of the current-entity mirror: OBJ, NPC, PLANE, PROJ, GV, untyped.
    // They match the hierarchy the .OT/.NT/.PT/.JT type records declare (#454).
    const uint32_t kCg = 0x0050CE80;
    for (const char* alias : { "_cg", "_cj", "_cn", "_co", "_cp", "_curThing" }) {
        INFO(alias);
        auto it = sms.find(alias);
        REQUIRE(it != sms.end());
        REQUIRE(it->second == kCg);
    }
    // db keys symbols by VA and requires them globally unique, so the block is claimed once.
    REQUIRE(fa_exe_claimed_vas().count(kCg) == 1);
}
