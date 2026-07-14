#include <catch2/catch_test_macros.hpp>
#include <fx/ai.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace fx;

// Compile AI source, requiring success, and return the BI bytes.
static std::vector<uint8_t> compile_ok(const std::string& src) {
    std::vector<AiCompileError> errs;
    auto bi = ai_compile(src, errs);
    if (!errs.empty()) INFO("compile error line " << errs[0].line << ": " << errs[0].message);
    REQUIRE_FALSE(bi.empty());
    return bi;
}

// The core #102 guarantee: ai_compile(ai_decompile(bi)) == bi, byte for byte.
static void check_roundtrip(const std::string& src) {
    auto bi = compile_ok(src);

    std::string dec = ai_decompile(bi.data(), bi.size());
    REQUIRE_FALSE(dec.empty());
    INFO("decompiled source:\n" << dec);

    std::vector<AiCompileError> errs;
    auto bi2 = ai_compile(dec, errs);
    if (!errs.empty()) INFO("recompile error line " << errs[0].line << ": " << errs[0].message);
    REQUIRE(bi2 == bi);
}

TEST_CASE("ai_decompile round-trips control flow (goto / inline-if / exit / restart)") {
    check_roundtrip(
        "if do_hit goto hit\n"
        "if do_evade goto evade\n"
        "exit\n"
        "hit:\n"
        "  move h 0 any 2000 10\n"
        "  restart\n"
        "evade:\n"
        "  %a = 5 - random 20\n"
        "  if alt < 5000 && %a < 0 %a = 0\n"
        "  goto hit\n");
}

TEST_CASE("ai_decompile round-trips .if/.else blocks, switch, and instructions") {
    check_roundtrip(
        "%a = alt + 100\n"
        ".if tgt && tgtAhead\n"
        "  maneuver \"GND ATTACK;BODENANGRIFF;ATTAQUE AU SOL\"\n"
        "  invert\n"
        ".else\n"
        "  switch random 3 aa bb cc\n"
        ".endif\n"
        "aa:\n"
        "  btoh\n"
        "bb:\n"
        "  immelman corner\n"
        "cc:\n"
        "  exit\n");
}

TEST_CASE("ai_decompile round-trips operators, prefix ops, and nested .if") {
    check_roundtrip(
        "%b = abs hdiff + neg pdiff\n"
        "%c = speed * 2 / 3 % 4\n"
        "if percent 50 goto done\n"
        ".if not tgtFacing\n"
        "  .if chance 2\n"
        "    %d = distToTgt\n"
        "  .endif\n"
        ".endif\n"
        "done:\n"
        "  exit\n");
}

TEST_CASE("ai_decompile preserves raw bytes in maneuver strings") {
    // High-byte characters (here 0xDC = 'Ü' in the game's codepage) must survive
    // the round-trip verbatim — they are copied into PUSH_ADDR unchanged.
    std::string src = "maneuver \"OVERSHOOT;\xDC" "BERSCHUSS;OVERSHOOT\"\nexit\n";
    auto bi = compile_ok(src);
    std::string dec = ai_decompile(bi.data(), bi.size());
    REQUIRE(dec.find("\xDC" "BERSCHUSS") != std::string::npos);
    check_roundtrip(src);
}

TEST_CASE("ai_decompile rejects input with no CODE section") {
    std::vector<uint8_t> junk(64, 0);
    REQUIRE(ai_decompile(junk.data(), junk.size()).empty());
    REQUIRE(ai_decompile(nullptr, 0).empty());
}

// Regression (#186 fuzz, UBSan): a source that compiles to no bytecode reached
// memcpy(dst, nullptr, 0) — UB. The compile must succeed without it.
TEST_CASE("ai_compile handles a source that yields no bytecode") {
    std::vector<AiCompileError> errs;
    auto bi = ai_compile("", errs);  // empty bytecode path — must not be UB
    REQUIRE_FALSE(bi.empty());        // still emits the PE container + END
}

// Regression (#186 fuzz, UBSan): an oversized integer literal overflowed int in
// the tokenizer (v = v*10 + d). It must saturate instead.
TEST_CASE("ai_compile clamps an oversized integer literal without overflow") {
    std::vector<AiCompileError> errs;
    std::string src(400, '9');       // lexes to one enormous T_NUMBER
    auto bi = ai_compile(src, errs);  // v*10 must saturate, not overflow int
    (void)bi;
    SUCCEED();
}

// Acceptance criterion for #102: every stock flight AI is a fixed point of the
// fx toolchain — compile → decompile → recompile reproduces the bytecode
// exactly. Real-asset mode; runs where FX_FA_ROOT points at an FA install.
TEST_CASE("ai_decompile: all 9 stock AIs round-trip byte-identically") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    // The 9 .AI scripts ship in FA_2.LIB (case varies across installs).
    std::vector<uint8_t> lib;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().filename().string()) != "fa_2.lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        lib.resize((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        break;
    }
    if (lib.empty()) SKIP("fa_2.lib not present in this install");

    auto entries = ealib_read_dir(lib.data(), lib.size());
    int total = 0;
    for (const auto& e : entries) {
        std::string name = lower(e.name);
        if (name.size() < 3 || name.substr(name.size() - 3) != ".ai") continue;
        auto data = ealib_extract(lib.data(), lib.size(), e, true);
        REQUIRE_FALSE(data.empty());
        INFO(name);

        std::string source((const char*)data.data(), data.size());
        std::vector<AiCompileError> errs;
        auto bi = ai_compile(source, errs);
        REQUIRE_FALSE(bi.empty());

        std::string dec = ai_decompile(bi.data(), bi.size());
        REQUIRE_FALSE(dec.empty());

        std::vector<AiCompileError> errs2;
        auto bi2 = ai_compile(dec, errs2);
        REQUIRE(bi2 == bi);
        ++total;
    }
    CHECK(total == 9);
    WARN("AI decompile round-trip census: " << total << " stock scripts");
}

// ---------------------------------------------------------------------------
// The action vocabulary is the ENGINE's (#491).
//
// An AI instruction compiles to `_CTDo_<name>`, and the engine exports 26 of them. The
// compiler's table used to hold 15 -- the ones the nine stock scripts use, plus `turn` -- so
// nine real actions could not be compiled at all. The table was a record of the corpus, not
// of the language. It is the language now, and this pins it there.
// ---------------------------------------------------------------------------

namespace {

// The actions the ENGINE exports, read out of db/symbols/. This is the same set FA.SMS names
// and the same set the .BI overlays import.
std::set<std::string> db_actions() {
    std::set<std::string> a;
    for (const auto& de : std::filesystem::directory_iterator(FX_DB_DIR "/symbols")) {
        if (de.path().extension() != ".csv") continue;
        std::ifstream f(de.path());
        std::string line;
        std::getline(f, line);
        while (std::getline(f, line)) {
            size_t b = line.find(',');
            if (b == std::string::npos) continue;
            size_t c = line.find(',', b + 1);
            if (c == std::string::npos) continue;
            size_t d = line.find(',', c + 1);
            std::string name = line.substr(c + 1, d - c - 1);
            if (name.rfind("_CTDo_", 0) == 0) a.insert(name.substr(6));
        }
    }
    return a;
}

// Compile one statement and report whether the compiler accepted it.
bool compiles(const std::string& stmt) {
    std::vector<AiCompileError> errs;
    auto bi = ai_compile(stmt + "\nexit\n", errs);
    return errs.empty() && !bi.empty();
}

}  // namespace

TEST_CASE("the compiler accepts exactly the actions the engine exports") {
    const std::set<std::string> engine = db_actions();
    REQUIRE(engine.size() == 26);

    // Every action the engine exports compiles -- including the nine no stock script uses.
    for (const std::string& a : engine) {
        INFO("action " << a);
        // Arity comes from the source line, so a call with no arguments is well-formed for
        // any of them; what is being asserted is that the NAME is accepted.
        REQUIRE(compiles(a));
    }

    // The nine that could not be compiled at all before.
    for (const char* a : { "play", "print", "printnum", "rudder", "splits",
                           "uhomepos", "wm_control", "wm_formation", "wm_vspacing" }) {
        INFO(a);
        REQUIRE(engine.count(a) == 1);
        REQUIRE(compiles(a));
    }

    // And an identifier the engine does NOT export is still an error, not a silent call into
    // nothing. (A generic parser that accepted any identifier would emit `_CTDo_frobnicate`,
    // which resolves to no symbol and would do nothing at all in-game.)
    REQUIRE_FALSE(compiles("frobnicate 1 2"));
}

TEST_CASE("an action the stock scripts never use compiles and round-trips") {
    // `splits` and `printnum` are real engine actions (_CTDo_splits, _CTDo_printnum) that no
    // shipped .BI imports. Before, `unknown statement: 'splits'`.
    const std::string src =
        "splits 100 200\n"
        "printnum alt\n"
        "wm_vspacing 50\n"
        "exit\n";
    std::vector<AiCompileError> errs;
    auto bi = ai_compile(src, errs);
    if (!errs.empty()) INFO(errs[0].message);
    REQUIRE(errs.empty());
    REQUIRE_FALSE(bi.empty());

    // And it survives the BI -> AI -> BI fixed point, like any other script.
    std::string dec = ai_decompile(bi.data(), bi.size());
    REQUIRE_FALSE(dec.empty());
    std::vector<AiCompileError> errs2;
    auto bi2 = ai_compile(dec, errs2);
    REQUIRE(errs2.empty());
    REQUIRE(bi2 == bi);
}
