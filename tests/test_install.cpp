#include <catch2/catch_test_macros.hpp>
#include <fx/install.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "support/fixture.h"

using namespace fx;
namespace fs = std::filesystem;

// The install engine's decision layer is a pure function of scanned metadata, so
// most of this file never touches a disk: a DiscSource is just a struct. The
// three I/O stages (scan, execute, verify) get a synthetic disc written to a
// temp directory — no game bytes anywhere, per tests/fixtures/README.md.

// ---------------------------------------------------------------------------
// Fixtures — a miniature of the retail disc's shape
// ---------------------------------------------------------------------------

static const char SETUP_SSF[] =
    "# master script\n"
    "COMPANY_NAME \"Jane's Combat Simulations\"\n"
    "APP_NAME \"Fighters Anthology\"\n"
    "DEFAULT_PATH \"\\JANES\\Fighters Anthology\"\n"
    // Localised labels, deliberately in the order the retail disc uses them: the
    // minimal script is named first and neither label is machine-readable.
    "INSTALL_SCRIPT \"MINSTALL.SSF\",\":0409:Minimal Install:0C:Mini installation\"\n"
    "INSTALL_SCRIPT \"FINSTALL.SSF\",\":0409:Full Install:0C:Installation complete\"\n";

// The full script: two named files, a whole-label glob, a suffix glob, a
// system-directory file, and the clobber guards.
static const char FINSTALL_SSF[] =
    "CREATE_FOLDERS \"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"FA.EXE\",\"FA_EXECUTABLE_FILES\",\"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"*.*\",\"FA_MISC\",\"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"*.DLL\",\"COMMDRV_DLLS_FILES\",\"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"FA_4B.LIB\",\"FA_LIBS\",\"[INSTALL_PATH]\"\n"
    "INSTALL_SYSFILES \"EAREMOVE.EXE\",\"REMOVER_EXECUTABLE_FILE\"\n"
    "SKIP_ON_REMOVE \"*.MT\"\n"
    "SKIP_ON_REMOVE \"EA.CFG\"\n"
    "REGEXE \"[INSTALL_PATH]\\FA.EXE\"\n";

// The minimal script is the same, less the digital-music LIB.
static const char MINSTALL_SSF[] =
    "CREATE_FOLDERS \"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"FA.EXE\",\"FA_EXECUTABLE_FILES\",\"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"*.*\",\"FA_MISC\",\"[INSTALL_PATH]\"\n"
    "INSTALL_FILES \"*.DLL\",\"COMMDRV_DLLS_FILES\",\"[INSTALL_PATH]\"\n"
    "INSTALL_SYSFILES \"EAREMOVE.EXE\",\"REMOVER_EXECUTABLE_FILE\"\n"
    "SKIP_ON_REMOVE \"*.MT\"\n"
    "SKIP_ON_REMOVE \"EA.CFG\"\n";

static DiscScript script_of(const std::string& name, const char* text) {
    const uint8_t* p = (const uint8_t*)text;
    return {name, ssf_read(p, std::char_traits<char>::length(text))};
}

static EsaEntry entry(const std::string& name, const std::string& label,
                      uint32_t usize, uint32_t flags = 0x211) {
    EsaEntry e;
    e.name = name; e.label = label; e.usize = usize; e.csize = usize;
    e.flags = flags; e.method = "NULL";
    return e;
}

// A disc 1 with the retail directory's shape: the two fingerprinted files at
// their 1.00F sizes, a whole label, a DLL label, a LIB, a sysfile, and the two
// loose CD-resident LIBs the archive does not carry.
static DiscSource disc1() {
    DiscSource d;
    d.root     = "/media/disc1";
    d.disc     = 1;
    d.esa_name = "SETUP.ESA";
    d.esa = {
        entry("FA.EXE", "FA_EXECUTABLE_FILES", 1299968),
        entry("FA.SMS", "FA_EXECUTABLE_FILES", 104452),
        entry("CHAT.TXT", "FA_MISC", 591),
        entry("EXAMPLE.MT", "FA_MISC", 1178),
        entry("FA_4B.LIB", "FA_LIBS", 34670738),
        entry("CDRVDL32.DLL", "COMMDRV_DLLS_FILES", 28672),
        entry("COMMSC32.DLL", "COMMDRV_DLLS_FILES", 18432),
        entry("EAREMOVE.EXE", "REMOVER_EXECUTABLE_FILE", 325632, 0x221),
    };
    d.loose = {{"SETUP.ESA", 4096}, {"SETUP.SSF", 100}, {"FINSTALL.SSF", 100},
               {"MINSTALL.SSF", 100}, {"FA_4C.LIB", 87176343}, {"FA_7.LIB", 160640711}};
    d.scripts = {script_of("SETUP.SSF", SETUP_SSF),
                 script_of("FINSTALL.SSF", FINSTALL_SSF),
                 script_of("MINSTALL.SSF", MINSTALL_SSF)};
    return d;
}

// Disc 2: data only. Lower-case, as an ISO9660 mount may well present it.
static DiscSource disc2() {
    DiscSource d;
    d.root  = "/media/disc2";
    d.disc  = 2;
    d.loose = {{"fa_3.lib", 27769544}, {"fa_10.lib", 179804523}};
    return d;
}

static const InstallItem* item(const InstallPlan& plan, const std::string& dest) {
    for (const auto& it : plan.items)
        if (it.dest == dest) return &it;
    return nullptr;
}

static size_t count(const InstallPlan& plan, InstallStatus status) {
    return (size_t)std::count_if(plan.items.begin(), plan.items.end(),
                                 [&](const InstallItem& it) { return it.status == status; });
}

// ---------------------------------------------------------------------------
// Glob — the DOS matcher the scripts speak
// ---------------------------------------------------------------------------

TEST_CASE("install_match: DOS globs, ASCII case-insensitive", "[install]") {
    // "*.*" is DOS's everything, dot or not.
    REQUIRE(install_match("*.*", "FA.EXE"));
    REQUIRE(install_match("*.*", "JANE'S HOME PAGE.URL"));
    REQUIRE(install_match("*.*", "NODOT"));

    REQUIRE(install_match("*.DLL", "COMMSC32.DLL"));
    REQUIRE(install_match("*.DLL", "commsc32.dll"));
    REQUIRE_FALSE(install_match("*.DLL", "FA.EXE"));
    REQUIRE_FALSE(install_match("*.DLL", "A.DLL.TXT"));

    REQUIRE(install_match("FA.EXE", "fa.exe"));
    REQUIRE_FALSE(install_match("FA.EXE", "FA.SMS"));
    REQUIRE_FALSE(install_match("FA.EXE", "XFA.EXE"));

    REQUIRE(install_match("FA_?.LIB", "FA_1.LIB"));
    REQUIRE_FALSE(install_match("FA_?.LIB", "FA_4B.LIB"));

    // Pathological patterns must not blow up: the matcher is iterative.
    REQUIRE(install_match("*a*a*a*a*a*a*b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"));
    REQUIRE_FALSE(install_match("*a*a*a*a*a*a*b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaac"));
}

// ---------------------------------------------------------------------------
// Probe — which disc, from the contents alone
// ---------------------------------------------------------------------------

TEST_CASE("install_probe_disc: content-based, order-independent", "[install]") {
    REQUIRE(install_probe_disc(disc1()) == 1);
    REQUIRE(install_probe_disc(disc2()) == 2);

    // An archive without the master script is not disc 1.
    DiscSource d = disc1();
    d.scripts.erase(d.scripts.begin());  // SETUP.SSF sorts first
    d.loose.erase(std::remove_if(d.loose.begin(), d.loose.end(),
                                 [](const DiscFile& f) { return f.name == "SETUP.SSF"; }),
                  d.loose.end());
    REQUIRE(install_probe_disc(d) == 0);

    // A directory of LIBs with an executable in it is somebody's install, not
    // disc 2.
    DiscSource two = disc2();
    two.loose.push_back({"FA.EXE", 1299968});
    REQUIRE(install_probe_disc(two) == 0);

    REQUIRE(install_probe_disc(DiscSource{}) == 0);
}

// ---------------------------------------------------------------------------
// Plan — the pure decision layer
// ---------------------------------------------------------------------------

TEST_CASE("install_plan: the full script is chosen by size, not by its label", "[install]") {
    InstallOptions opt;  // full by default
    const InstallPlan plan = install_plan({disc1(), disc2()}, {}, opt);
    REQUIRE(plan.errors.empty());

    REQUIRE(plan.script == "FINSTALL.SSF");
    REQUIRE(plan.company == "Jane's Combat Simulations");
    REQUIRE(plan.app_name == "Fighters Anthology");
    REQUIRE(plan.default_path == "\\JANES\\Fighters Anthology");
    REQUIRE(plan.build == MediaBuild::V100F);

    // The whole-label glob pulled both FA_MISC entries; the suffix glob both DLLs.
    REQUIRE(item(plan, "FA.EXE"));
    REQUIRE(item(plan, "CHAT.TXT"));
    REQUIRE(item(plan, "EXAMPLE.MT"));
    REQUIRE(item(plan, "CDRVDL32.DLL"));
    REQUIRE(item(plan, "COMMSC32.DLL"));
    REQUIRE(item(plan, "FA_4B.LIB"));

    // FA.SMS carries a label no INSTALL_FILES glob in this script selects... it
    // is in FA_EXECUTABLE_FILES, but only "FA.EXE" is named. Nothing is copied
    // that the script did not ask for.
    REQUIRE(item(plan, "FA.SMS") == nullptr);

    // The minimal script drops exactly the digital-music LIB.
    opt.full = false;
    const InstallPlan mini = install_plan({disc1(), disc2()}, {}, opt);
    REQUIRE(mini.script == "MINSTALL.SSF");
    REQUIRE(item(mini, "FA_4B.LIB") == nullptr);
    REQUIRE(mini.items.size() == plan.items.size() - 1);
}

TEST_CASE("install_plan: INSTALL_SYSFILES is recorded, never written", "[install]") {
    const InstallPlan plan = install_plan({disc1()}, {}, InstallOptions{});
    const InstallItem* it = item(plan, "EAREMOVE.EXE");
    REQUIRE(it);
    REQUIRE(it->status == InstallStatus::SkipSysfile);
    REQUIRE_FALSE(it->note.empty());

    // ...and the directive says so rather than vanishing.
    bool seen = false;
    for (const auto& d : plan.directives) {
        if (d.keyword != "INSTALL_SYSFILES") continue;
        seen = true;
        REQUIRE_FALSE(d.honored);
        REQUIRE_FALSE(d.note.empty());
    }
    REQUIRE(seen);
}

TEST_CASE("install_plan: every script statement is accounted for", "[install]") {
    const InstallPlan plan = install_plan({disc1()}, {}, InstallOptions{});
    // The shell/registry directives are reported unhonored, not dropped.
    bool regexe = false;
    for (const auto& d : plan.directives) {
        if (d.keyword == "REGEXE") { regexe = true; REQUIRE_FALSE(d.honored); }
        REQUIRE_FALSE(d.keyword.empty());
    }
    REQUIRE(regexe);

    // SETUP.SSF's statements + the chosen sub-script's, and nothing from the
    // sub-script that was not chosen.
    size_t from_min = 0;
    for (const auto& d : plan.directives)
        if (d.script == "MINSTALL.SSF") from_min++;
    REQUIRE(from_min == 0);
}

TEST_CASE("install_plan: CD-resident LIBs are a rule, not a list", "[install]") {
    InstallOptions opt;
    const InstallPlan plan = install_plan({disc1(), disc2()}, {}, opt);

    // Loose on a disc root, not in the archive => copied.
    for (const char* name : {"FA_4C.LIB", "FA_7.LIB", "FA_3.LIB", "FA_10.LIB"}) {
        const InstallItem* it = item(plan, name);
        REQUIRE(it);
        REQUIRE(it->origin == InstallOrigin::Loose);
    }
    // Disc 2 came off the mount lower-case; the install is upper-case.
    REQUIRE(item(plan, "FA_3.LIB")->source == "fa_3.lib");
    REQUIRE(item(plan, "FA_3.LIB")->disc == 1);  // the second source

    // FA_4B.LIB is in the archive, so the loose rule must not double-plan it.
    REQUIRE(item(plan, "FA_4B.LIB")->origin == InstallOrigin::Archive);

    opt.cd_resident = false;
    const InstallPlan lean = install_plan({disc1(), disc2()}, {}, opt);
    REQUIRE(item(lean, "FA_4C.LIB") == nullptr);
    REQUIRE(item(lean, "FA_4B.LIB"));  // still archive-sourced
}

TEST_CASE("install_plan: the clobber guard protects what the game writes", "[install]") {
    // A re-install over a directory that already holds these.
    const std::vector<std::string> existing = {"CHAT.TXT", "EXAMPLE.MT", "EA.CFG"};

    InstallOptions opt;
    InstallPlan plan = install_plan({disc1()}, existing, opt);
    // Default: nothing already there is touched.
    REQUIRE(item(plan, "CHAT.TXT")->status == InstallStatus::KeepExisting);
    REQUIRE(item(plan, "EXAMPLE.MT")->status == InstallStatus::KeepExisting);
    REQUIRE(item(plan, "FA.EXE")->status == InstallStatus::Copy);

    // --overwrite replaces the game's own files...
    opt.overwrite = true;
    plan = install_plan({disc1()}, existing, opt);
    REQUIRE(item(plan, "CHAT.TXT")->status == InstallStatus::Copy);

    // ...but never one SKIP_ON_REMOVE marks as the user's. EXAMPLE.MT is the
    // sharp case: the archive ships it AND `*.MT` guards it, so a fresh install
    // writes it and a re-install keeps the one the user edited.
    const InstallItem* mt = item(plan, "EXAMPLE.MT");
    REQUIRE(mt->status == InstallStatus::KeepExisting);
    REQUIRE(mt->note.find("SKIP_ON_REMOVE") != std::string::npos);

    // A fresh directory writes it.
    REQUIRE(item(install_plan({disc1()}, {}, opt), "EXAMPLE.MT")->status ==
            InstallStatus::Copy);

    // Only Copy items are counted in the byte total.
    uint64_t want = 0;
    for (const auto& it : plan.items)
        if (it.status == InstallStatus::Copy) want += it.bytes;
    REQUIRE(plan.bytes == want);
}

TEST_CASE("install_plan: a hostile script cannot escape the install directory",
          "[install]") {
    // The destination argument comes off the disc. A script that tries to walk
    // out of the install directory — or name an absolute one — must be confined,
    // not obeyed. Both halves of the path are covered: the directory here, and
    // the file name by esa_safe_name.
    static const char EVIL_SSF[] =
        "CREATE_FOLDERS \"[INSTALL_PATH]\"\n"
        "INSTALL_FILES \"FA.EXE\",\"FA_EXECUTABLE_FILES\",\"[INSTALL_PATH]\\..\\..\\WINDOWS\"\n"
        "INSTALL_FILES \"CHAT.TXT\",\"FA_MISC\",\"C:\\WINDOWS\\SYSTEM\"\n"
        "INSTALL_FILES \"EXAMPLE.MT\",\"FA_MISC\",\"[INSTALL_PATH]\\DATA\"\n";

    DiscSource d = disc1();
    d.scripts = {script_of("SETUP.SSF",
                           "INSTALL_SCRIPT \"FINSTALL.SSF\",\":0409:Full\"\n"),
                 script_of("FINSTALL.SSF", EVIL_SSF)};

    InstallOptions opt;
    opt.cd_resident = false;
    const InstallPlan plan = install_plan({d}, {}, opt);
    REQUIRE(plan.errors.empty());

    for (const auto& it : plan.items) {
        REQUIRE(it.dest.find("..") == std::string::npos);
        REQUIRE(it.dest.find(':') == std::string::npos);
        REQUIRE(it.dest.front() != '/');
    }
    // The traversal components are dropped, not the file.
    REQUIRE(item(plan, "WINDOWS/FA.EXE"));
    REQUIRE(item(plan, "WINDOWS/SYSTEM/CHAT.TXT"));
    // ...and a legitimate subdirectory still works.
    REQUIRE(item(plan, "DATA/EXAMPLE.MT"));
}

TEST_CASE("install_plan: the media is fingerprinted, and unknown media is fatal",
          "[install]") {
    REQUIRE(install_plan({disc1()}, {}, InstallOptions{}).build == MediaBuild::V100F);

    // The patched build, as a pre-patched disc image would carry it.
    DiscSource patched = disc1();
    patched.esa[0].usize = 1319424;  // FA.EXE
    patched.esa[1].usize = 106706;   // FA.SMS
    InstallPlan plan = install_plan({patched}, {}, InstallOptions{});
    REQUIRE(plan.build == MediaBuild::V102F);
    REQUIRE(plan.errors.empty());

    // Anything else is media we cannot describe: refuse rather than guess.
    DiscSource odd = disc1();
    odd.esa[0].usize = 1234567;
    plan = install_plan({odd}, {}, InstallOptions{});
    REQUIRE(plan.build == MediaBuild::Unknown);
    REQUIRE_FALSE(plan.errors.empty());

    InstallOptions opt;
    opt.allow_unknown_media = true;
    plan = install_plan({odd}, {}, opt);
    REQUIRE(plan.build == MediaBuild::Unknown);
    REQUIRE(plan.errors.empty());  // ...unless the caller says it knows better

    REQUIRE(std::string(install_build_name(MediaBuild::V100F)) == "1.00F");
    REQUIRE(std::string(install_build_name(MediaBuild::Unknown)) == "unknown");
}

TEST_CASE("install_plan: missing or broken media errors out", "[install]") {
    // No disc 1 at all.
    InstallPlan plan = install_plan({disc2()}, {}, InstallOptions{});
    REQUIRE_FALSE(plan.errors.empty());
    REQUIRE(plan.items.empty());

    // Disc 1 whose SETUP.SSF names a script that is not on the disc.
    DiscSource d = disc1();
    d.scripts.pop_back();  // SETUP.SSF, FINSTALL.SSF, MINSTALL.SSF -> drop MINSTALL
    plan = install_plan({d}, {}, InstallOptions{});
    REQUIRE_FALSE(plan.errors.empty());
    REQUIRE(plan.script == "FINSTALL.SSF");  // the one that is there still resolves

    // An archive the scripts select nothing from.
    DiscSource empty = disc1();
    empty.esa.clear();
    InstallOptions opt;
    opt.allow_unknown_media = true;
    opt.cd_resident = false;
    plan = install_plan({empty}, {}, opt);
    REQUIRE_FALSE(plan.errors.empty());
}

// ---------------------------------------------------------------------------
// scan -> plan -> execute -> verify, over a synthetic disc on disk
// ---------------------------------------------------------------------------

namespace {

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name)
        : path(fs::temp_directory_path() / ("fx_install_test_" + std::string(name))) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

void write_file(const fs::path& p, const std::vector<uint8_t>& bytes) {
    std::ofstream f(p, std::ios::binary);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
}

void write_text(const fs::path& p, const char* text) {
    std::ofstream f(p, std::ios::binary);
    f << text;
}

std::vector<uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

// A disc 1 on disk: the scripts, a loose CD-resident LIB, and a real ESA
// carrying one stored entry and one PKWA entry — the committed DCL known-answer
// stream, so the compressed path is exercised without a byte of game data.
void make_disc_dir(const fs::path& root) {
    fs::create_directories(root);
    write_text(root / "SETUP.SSF",
               "COMPANY_NAME \"Jane's Combat Simulations\"\n"
               "APP_NAME \"Fighters Anthology\"\n"
               "INSTALL_SCRIPT \"FINSTALL.SSF\",\":0409:Full Install\"\n");
    write_text(root / "FINSTALL.SSF",
               "CREATE_FOLDERS \"[INSTALL_PATH]\"\n"
               "INSTALL_FILES \"*.*\",\"FA_MISC\",\"[INSTALL_PATH]\"\n"
               "SKIP_ON_REMOVE \"*.MT\"\n");
    write_text(root / "FA_4C.LIB", "cd-resident audio\n");

    // esa_build stores; a PKWA entry has to be assembled by hand, exactly as the
    // ESA suite does it.
    const auto dcl = fx_test::load_fixture("blast/adler-ai.dcl");  // -> "AIAIAIAIAIAIA"
    const std::vector<uint8_t> stored = {'s', 't', 'o', 'r', 'e', 'd', '\n'};

    std::vector<uint8_t> esa;
    const char magic[] = "ELECTRONIC_ARTS_ARCHIVE_FILE";
    esa.insert(esa.end(), (const uint8_t*)magic, (const uint8_t*)magic + sizeof(magic));

    struct Rec { std::string name, method; uint32_t usize, csize; };
    const Rec recs[] = {
        {"STORED.TXT", "NULL", (uint32_t)stored.size(), (uint32_t)stored.size()},
        {"AI.MT",      "PKWA", 13,                      (uint32_t)dcl.size()},
    };
    // The directory is fixed-shape, so its size is computable up front.
    size_t dir_size = sizeof(magic) + 1;  // magic + terminator
    for (const Rec& r : recs)
        dir_size += r.name.size() + 1 + std::string("FA_MISC").size() + 1 +
                    4 + 4 + 4 + r.method.size() + 1 + 4 + 4;

    uint32_t offset = (uint32_t)dir_size;
    auto put_u32 = [&](uint32_t x) {
        esa.push_back((uint8_t)x);         esa.push_back((uint8_t)(x >> 8));
        esa.push_back((uint8_t)(x >> 16)); esa.push_back((uint8_t)(x >> 24));
    };
    auto put_cstr = [&](const std::string& s) {
        esa.insert(esa.end(), s.begin(), s.end());
        esa.push_back(0);
    };
    for (const Rec& r : recs) {
        put_cstr(r.name);
        put_cstr("FA_MISC");
        put_u32(0x211);
        put_u32(r.usize);
        put_u32(0);
        put_cstr(r.method);
        put_u32(r.csize);
        put_u32(offset);
        offset += r.csize;
    }
    esa.push_back(0);  // terminator
    REQUIRE(esa.size() == dir_size);
    esa.insert(esa.end(), stored.begin(), stored.end());
    esa.insert(esa.end(), dcl.begin(), dcl.end());
    write_file(root / "SETUP.ESA", esa);
}

} // namespace

TEST_CASE("install: scan, execute and verify a synthetic disc", "[install]") {
    TempDir disc("disc"), dest("dest");
    make_disc_dir(disc.path);

    const DiscSource scanned = install_scan(disc.path.string());
    REQUIRE(scanned.disc == 1);
    REQUIRE(scanned.esa.size() == 2);
    REQUIRE(scanned.scripts.size() == 2);

    InstallOptions opt;
    opt.allow_unknown_media = true;  // a synthetic archive has no FA.EXE to print
    const std::vector<DiscSource> discs = {scanned};
    InstallPlan plan = install_plan(discs, install_list_dir(dest.path.string()), opt);
    REQUIRE(plan.errors.empty());
    REQUIRE(plan.items.size() == 3);  // 2 archive entries + the CD-resident LIB

    std::vector<std::string> errors;
    REQUIRE(install_execute(discs, plan, dest.path.string(), nullptr, nullptr, &errors));
    REQUIRE(errors.empty());

    // The PKWA entry decoded; the stored entry and the loose file copied.
    const auto ai = read_file(dest.path / "AI.MT");
    REQUIRE(std::string(ai.begin(), ai.end()) == "AIAIAIAIAIAIA");
    const auto st = read_file(dest.path / "STORED.TXT");
    REQUIRE(std::string(st.begin(), st.end()) == "stored\n");
    REQUIRE(fs::exists(dest.path / "FA_4C.LIB"));

    // Nothing half-written is left behind.
    for (const auto& de : fs::directory_iterator(dest.path))
        REQUIRE(de.path().extension() != ".part");

    REQUIRE(install_verify(discs, plan, dest.path.string(), &errors));

    // Verify is a byte-compare against the disc, so it catches a corrupted file.
    write_text(dest.path / "STORED.TXT", "tampered\n");
    errors.clear();
    REQUIRE_FALSE(install_verify(discs, plan, dest.path.string(), &errors));
    REQUIRE(errors.size() == 1);

    // A re-plan over the installed directory keeps what is there; the guarded
    // AI.MT survives even an --overwrite.
    opt.overwrite = true;
    plan = install_plan(discs, install_list_dir(dest.path.string()), opt);
    REQUIRE(item(plan, "AI.MT")->status == InstallStatus::KeepExisting);
    REQUIRE(item(plan, "STORED.TXT")->status == InstallStatus::Copy);
    REQUIRE(install_execute(discs, plan, dest.path.string(), nullptr, nullptr, &errors));
    REQUIRE(read_file(dest.path / "STORED.TXT").size() == 7);  // restored from the disc
}

TEST_CASE("install: a plan with errors is never executed", "[install]") {
    TempDir dest("refuse");
    InstallPlan plan;
    plan.errors.push_back("unrecognised media");
    plan.items.push_back({"FA.EXE", InstallStatus::Copy, InstallOrigin::Archive, 0,
                          "FA.EXE", "L", 10, ""});
    std::vector<std::string> errors;
    REQUIRE_FALSE(install_execute({}, plan, dest.path.string(), nullptr, nullptr, &errors));
    REQUIRE(fs::is_empty(dest.path));
}

TEST_CASE("install_scan: a directory that is not a disc", "[install]") {
    TempDir empty("empty");
    const DiscSource d = install_scan(empty.path.string());
    REQUIRE(d.disc == 0);
    REQUIRE(d.loose.empty());

    const DiscSource missing = install_scan((empty.path / "nope").string());
    REQUIRE(missing.disc == 0);
}
