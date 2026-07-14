#include <catch2/catch_test_macros.hpp>
#include <fx/seq.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <set>
#include <string>
#include <cstring>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes(const char* s) {
    return std::vector<uint8_t>(s, s + strlen(s));
}

TEST_CASE("seq_parse empty data returns empty SeqFile") {
    auto seq = seq_parse(nullptr, 0);
    REQUIRE(seq.lines.empty());
    REQUIRE(seq.events.empty());
}

TEST_CASE("seq_parse / seq_serialize round-trip") {
    const char* src =
        "; opening comment\r\n"
        "\t1000 fadein\r\n"
        "\t+500 palette DATA.PAL\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());
    auto out  = seq_serialize(seq);
    REQUIRE(out == data);
}

TEST_CASE("seq_parse identifies event lines vs comment lines") {
    const char* src =
        "; comment\r\n"
        "\t200 sound GUN.11K\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());
    REQUIRE(seq.lines.size() == 2u);
    REQUIRE(seq.is_event[0] == false);
    REQUIRE(seq.is_event[1] == true);
}

TEST_CASE("seq_parse parses absolute tick count") {
    const char* src = "\t1000 fadein\r\n";
    auto seq = seq_parse(bytes(src).data(), strlen(src));
    REQUIRE(seq.events.size() == 1u);
    REQUIRE(seq.events[0].ticks    == 1000);
    REQUIRE(seq.events[0].relative == false);
    REQUIRE(seq.events[0].command  == "fadein");
}

TEST_CASE("seq_parse parses relative tick prefix") {
    const char* src = "\t+500 palette DATA.PAL\r\n";
    auto seq = seq_parse(bytes(src).data(), strlen(src));
    REQUIRE(seq.events.size() == 1u);
    REQUIRE(seq.events[0].relative == true);
    REQUIRE(seq.events[0].ticks    == 500);
    REQUIRE(seq.events[0].command  == "palette");
    REQUIRE(seq.events[0].args.size() == 1u);
    REQUIRE(seq.events[0].args[0]     == "DATA.PAL");
}

TEST_CASE("seq_serialize round-trip preserves trailer byte") {
    const char* src = "\t100 wait\r\n\x1A";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());
    auto out  = seq_serialize(seq);
    REQUIRE(out == data);
}

TEST_CASE("seq_parse preserves blank lines in round-trip") {
    const char* src = "\t0 fadein\r\n\r\n\t100 fadeout\r\n";
    auto data = bytes(src);
    auto out  = seq_serialize(seq_parse(data.data(), data.size()));
    REQUIRE(out == data);
}

// The fxs SEQ editor mutates the three parallel vectors directly and
// re-serializes; these tests pin that contract (issue #92).

TEST_CASE("editor-style event insert round-trips and leaves other lines intact") {
    const char* src =
        "; comment\r\n"
        "\t1000\tfadein\r\n"
        "\t+500\tpalette\t\"DATA\"\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());

    // Insert after the fadein (lines[] index 1), the editor way.
    const char* body = "+1\tsound";
    seq.lines.insert(seq.lines.begin() + 2, std::string("\t") + body);
    seq.is_event.insert(seq.is_event.begin() + 2, true);
    auto line = bytes("\t+1\tsound\r\n");
    seq.events.insert(seq.events.begin() + 2,
                      seq_parse(line.data(), line.size()).events[0]);

    auto out = seq_serialize(seq);
    auto re  = seq_parse(out.data(), out.size());
    REQUIRE(re.lines.size() == 4u);
    REQUIRE(re.events[2].relative == true);
    REQUIRE(re.events[2].ticks    == 1);
    REQUIRE(re.events[2].command  == "sound");
    // Untouched lines byte-preserved
    REQUIRE(re.lines[0] == "; comment");
    REQUIRE(re.lines[1] == "\t1000\tfadein");
    REQUIRE(re.lines[3] == "\t+500\tpalette\t\"DATA\"");
    // Serialization is stable across a second pass
    REQUIRE(seq_serialize(re) == out);
}

TEST_CASE("editor-style event delete round-trips") {
    const char* src =
        "\t0\tfadein\r\n"
        "\t100\tsound\tGUN\r\n"
        "\t200\tfadeout\r\n";
    auto data = bytes(src);
    auto seq  = seq_parse(data.data(), data.size());

    seq.lines.erase(seq.lines.begin() + 1);
    seq.is_event.erase(seq.is_event.begin() + 1);
    seq.events.erase(seq.events.begin() + 1);

    auto out = seq_serialize(seq);
    auto re  = seq_parse(out.data(), out.size());
    REQUIRE(re.lines.size() == 2u);
    REQUIRE(re.events[0].command == "fadein");
    REQUIRE(re.events[1].command == "fadeout");
    REQUIRE(seq_serialize(re) == out);
}

TEST_CASE("editor-composed tab-separated line parses to full metadata") {
    // The editor rebuilds edited rows as <time>\t[sync\t]<command>\t<args>
    const char* src = "\t+40\tsync\tsound\t\"BOOM\" 22\r\n";
    auto seq = seq_parse(bytes(src).data(), strlen(src));
    REQUIRE(seq.events.size() == 1u);
    const auto& ev = seq.events[0];
    REQUIRE(ev.relative == true);
    REQUIRE(ev.ticks    == 40);
    REQUIRE(ev.sync     == true);
    REQUIRE(ev.command  == "sound");
    REQUIRE(ev.args.size() == 2u);
    REQUIRE(ev.args[0]     == "\"BOOM\"");
    REQUIRE(ev.args[1]     == "22");
}

// fuzz_seq (#117): the serializer normalizes every line terminator to CRLF,
// so byte-identity holds only for already-CRLF input — but serialize must be
// a fixed point of parse (idempotent) for ANY input. A bare CR and an
// LF-only line both collapse to CRLF; a second round trip then changes
// nothing.
TEST_CASE("seq serialize normalizes line endings and is idempotent") {
    // bare CR mid-file, an LF-only terminator, and a trailing DOS-EOF byte
    const char* src = "; c\r\n\t100 wait\ra\n\t+5 x\r\n\x1A";
    auto data = bytes(src);
    auto once = seq_serialize(seq_parse(data.data(), data.size()));
    auto twice = seq_serialize(seq_parse(once.data(), once.size()));
    REQUIRE(once == twice);            // serialize is a fixed point of parse
    // and the normalized form carries no bare CR / LF-only terminators
    for (size_t i = 0; i + 1 < once.size(); ++i) {
        if (once[i] == '\r') CHECK(once[i + 1] == '\n');
        if (once[i] == '\n') CHECK((i > 0 && once[i - 1] == '\r'));
    }
}

// ---------------------------------------------------------------------------
// The command vocabulary is not a guess -- it is the SYMBOL TABLE (#491).
//
// _SeqContinue (0x445700) builds "_SEQ" + the command token and calls SMAddress on it, then
// calls what comes back. That is why "fadeout" appears nowhere in FA.EXE: the string that
// exists is `_SEQfadeout`, in FA.SMS. So a SEQ command is an IMPORT, exactly like a BRF
// `symbol`, and the vocabulary is whatever `_SEQ*` symbols the engine exports.
// ---------------------------------------------------------------------------

namespace {

std::set<std::string> db_seq_commands() {
    std::set<std::string> cmds;
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
            std::string name = line.substr(b + 1, c - b - 1);
            if (name.rfind("_SEQ", 0) == 0) cmds.insert(name.substr(4));
        }
    }
    return cmds;
}

}  // namespace

TEST_CASE("the SEQ commands are the engine's _SEQ* symbols") {
    std::set<std::string> cmds = db_seq_commands();
    // The eight the shipped sequences use...
    for (const char* c : { "bitmap", "palette", "font", "video", "sound",
                           "fadein", "fadeout", "wait" })
        REQUIRE(cmds.count(c) == 1);
    // ...and the five the engine exports that no shipped sequence calls. SEQ.md listed
    // neither these nor the fact that `sync` is a MODIFIER, not a command.
    for (const char* c : { "call", "music", "run", "sndoff", "text" })
        REQUIRE(cmds.count(c) == 1);
    REQUIRE(cmds.count("sync") == 0);
}

// The regression: three shipped sequences indent their last event with SPACES.
TEST_CASE("a space-indented event is an event (UDEAD/UWON/ULOST.SEQ)") {
    // UDEAD.SEQ, verbatim.
    auto data = bytes("\t0\tbitmap \"UDEAD\" 0 0 0 256\r\n"
                         "\t0 \tfadein  .5\r\n"
                         "\t0\tsound \"^UDEAD.11K\"\r\n"
                         "      +23 sync\tfadeout\t.5\r\n");
    auto seq = seq_parse(data.data(), data.size());

    REQUIRE(seq.is_event.size() == 4u);
    REQUIRE(seq.is_event[3]);                      // was false: dropped as a comment
    REQUIRE(seq.events[3].command  == "fadeout");
    REQUIRE(seq.events[3].sync);
    REQUIRE(seq.events[3].relative);
    REQUIRE(seq.events[3].ticks    == 23);
    REQUIRE(seq.events[3].indent   == "      ");   // six spaces, and they survive
    REQUIRE(seq_serialize(seq) == data);           // byte-identical either way
}

TEST_CASE("a // line is a comment, as the engine's SeqSkipComments has it") {
    auto data = bytes("// a comment\r\n; also a comment\r\n\t0\tfadein .5\r\n");
    auto seq = seq_parse(data.data(), data.size());
    REQUIRE_FALSE(seq.is_event[0]);
    REQUIRE_FALSE(seq.is_event[1]);
    REQUIRE(seq.is_event[2]);
    REQUIRE(seq_serialize(seq) == data);
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
// ---------------------------------------------------------------------------

TEST_CASE("SEQ decode census: every shipped sequence, against the symbol table") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    const std::set<std::string> cmds = db_seq_commands();
    REQUIRE_FALSE(cmds.empty());

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int files = 0, events = 0, space_indented = 0, sync = 0, relative = 0;
    std::map<std::string, int> used;

    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".seq") continue;
            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            SeqFile seq = seq_parse(rec.data(), rec.size());

            // 1. The round-trip SEQ.md claimed and never tested against a real file.
            REQUIRE(seq_serialize(seq) == rec);

            for (size_t i = 0; i < seq.is_event.size(); i++) {
                if (!seq.is_event[i]) continue;
                const SeqEvent& ev = seq.events[i];
                INFO("event: " << ev.raw);

                // 2. Every command resolves as an import: the engine calls
                //    SMAddress("_SEQ" + command) and jumps to whatever it gets.
                REQUIRE(cmds.count(ev.command) == 1);

                // 3. The indent is spaces or a tab -- and it is never empty, since an
                //    unindented non-comment line would be something else entirely.
                REQUIRE_FALSE(ev.indent.empty());
                REQUIRE(ev.indent.find_first_not_of(" \t") == std::string::npos);
                if (ev.indent.find(' ') != std::string::npos) space_indented++;

                used[ev.command]++;
                if (ev.sync) sync++;
                if (ev.relative) relative++;
                events++;
            }
            files++;
        }
    }
    REQUIRE(files > 0);
    REQUIRE(events > 0);
    // The three that would have been dropped as comments.
    REQUIRE(space_indented == 3);

    std::string hist;
    for (auto& [c, n] : used) hist += " " + c + "=" + std::to_string(n);
    WARN("SEQ census: " << files << " sequences, " << events << " events (" << space_indented
         << " space-indented, " << sync << " sync, " << relative << " relative);" << hist);
}
