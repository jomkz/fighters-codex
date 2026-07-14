#include <catch2/catch_test_macros.hpp>
#include <fx/brf.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <string>
#include <vector>

using namespace fx;

static std::vector<uint8_t> bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---------------------------------------------------------------------------
// brf_parse_int
// ---------------------------------------------------------------------------

TEST_CASE("brf_parse_int parses decimal") {
    REQUIRE(brf_parse_int("0")    == 0);
    REQUIRE(brf_parse_int("300")  == 300);
    REQUIRE(brf_parse_int("1000") == 1000);
}

TEST_CASE("brf_parse_int parses negative via caret") {
    REQUIRE(brf_parse_int("^300")  == -300);
    REQUIRE(brf_parse_int("^1")    == -1);
    REQUIRE(brf_parse_int("^4096") == -4096);
}

TEST_CASE("brf_parse_int parses hex via dollar prefix") {
    REQUIRE(brf_parse_int("$ff")       == 255);
    REQUIRE(brf_parse_int("$7fffffff") == 0x7fffffff);
    REQUIRE(brf_parse_int("$0")        == 0);
}

TEST_CASE("brf_parse_int returns 0 for empty string") {
    REQUIRE(brf_parse_int("") == 0);
}

// ---------------------------------------------------------------------------
// The fixture.
//
// The old one was invented: it opened with a `:envptr` table and a `ptr NULL` field, a shape
// NO shipped record has -- and a fixture no real file resembles is a liability (#491 C). This
// one is cut down from a real record, APTB1.OT, and keeps its shape: the section comments the
// file draws around its own records, a `symbol` naming the class proc with the file's own
// `; utilProc` comment, an inline `:hards` block of NUMERIC fields, and the string tables the
// `ptr` fields resolve to. Widths and offsets below are the ones the loader would assemble.
// ---------------------------------------------------------------------------

static const std::string kOtDoc =
    "[brent's_relocatable_format]\r\n"
    "\r\n"
    ";---------------- START OF OBJ_TYPE ----------------\r\n"
    "\r\n"
    "    byte 1\r\n"                       // +0x00  struct_type
    "    word 35\r\n"                      // +0x01  type_size (27 root + 8 of hardpoint)
    "    word 0\r\n"                       // +0x03  obj_ext_size
    "\tptr ot_names\r\n"                   // +0x05
    "    dword $20501\r\n"                 // +0x09
    "    word $100\r\n"                    // +0x0D
    "\tptr shape\r\n"                      // +0x0F
    "    dword ^300\r\n"                   // +0x13
    "\tsymbol _OBJProc\t; utilProc\r\n"    // +0x17
    "\r\n"
    ";---------------- END OF OBJ_TYPE ----------------\r\n"
    "\r\n"
    ":hards\r\n"                           // label at +0x1B -- numeric, and part of the record
    ";-------- hardpoint 0\r\n"
    "    word $8\r\n"                      // +0x1B
    "    word 0\r\n"                       // +0x1D
    "\tptr ot_names\r\n"                   // +0x1F  a station's default store
    ":ot_names\r\n"                        // label at +0x23 -- past the record
    "\tstring \"Apartments\"\r\n"          // 10 chars + NUL = 11
    "\tstring \"APTB1.OT\"\r\n"            // 8 + NUL = 9
    ":shape\r\n"
    "\tstring \"aptb1.SH\"\r\n"            // 8 + NUL = 9
    "\tend\r\n";

// ---------------------------------------------------------------------------
// The image model (LoadBrentDLL @0x41f240)
// ---------------------------------------------------------------------------

TEST_CASE("brf round-trip preserves raw bytes") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(brf_serialize(doc) == data);
}

TEST_CASE("brf_parse assembles fields at their image offsets") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());

    REQUIRE(doc.fields.size() == 9u);
    REQUIRE(doc.fields[0].type == "byte");
    REQUIRE(doc.fields[0].image_offset == 0u);
    REQUIRE(doc.fields[1].image_offset == 1u);   // word type_size
    REQUIRE(doc.fields[3].image_offset == 5u);   // ptr, after two words
    REQUIRE(doc.fields[3].type == "ptr");
    REQUIRE(doc.fields[8].image_offset == 0x17u);
    REQUIRE(doc.fields[8].type  == "symbol");
    REQUIRE(doc.fields[8].value == "_OBJProc");
    // The file names its own field. A name the file gives us is a fact.
    REQUIRE(doc.fields[8].comment == "utilProc");
    // Sections come from the file's own comment delimiters.
    REQUIRE(doc.fields[0].section == "OBJ_TYPE");
    REQUIRE(doc.fields[0].offset  == 0u);
}

// The regression. `:hards` is a numeric block, and every field in it used to be DROPPED --
// which is why `fx pt info` showed no hardpoints for any of the 145 shipped aircraft.
TEST_CASE("brf_parse keeps the numeric fields inside a labelled block") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());

    const BrfBlock* hards = doc.find_block("hards");
    REQUIRE(hards != nullptr);
    REQUIRE(hards->offset == 0x1Bu);
    REQUIRE(hards->fields.size() == 3u);
    REQUIRE(hards->fields[0].type  == "word");
    REQUIRE(hards->fields[0].value == "$8");
    REQUIRE(hards->fields[0].image_offset == 0x1Bu);
    REQUIRE(hards->fields[2].type  == "ptr");     // the station's default store
    REQUIRE(hards->fields[2].value == "ot_names");
    REQUIRE(hards->width == 8u);                  // 2 + 2 + 4
    REQUIRE(hards->strings.empty());
}

// `string` emits its characters plus a NUL. A fixed 4 bytes -- what brf_type_size used to
// return -- puts every field after a string at the wrong image offset.
TEST_CASE("a string emits its characters plus a NUL, not four bytes") {
    REQUIRE(brf_field_width("string", "Apartments") == 11u);
    REQUIRE(brf_field_width("string", "")           == 1u);
    REQUIRE(brf_field_width("byte",   "1")          == 1u);
    REQUIRE(brf_field_width("word",   "1")          == 2u);
    REQUIRE(brf_field_width("dword",  "1")          == 4u);
    REQUIRE(brf_field_width("ptr",    "shape")      == 4u);
    REQUIRE(brf_field_width("symbol", "_OBJProc")   == 4u);

    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    const BrfBlock* names = doc.find_block("ot_names");
    REQUIRE(names != nullptr);
    REQUIRE(names->offset == 0x23u);
    REQUIRE(names->width  == 11u + 9u);
    REQUIRE(names->strings.size() == 2u);
    REQUIRE(names->strings[0] == "Apartments");
    // ot_names (20) then shape (9): the whole image.
    REQUIRE(doc.image_size == 0x23u + 20u + 9u);
}

TEST_CASE("the record the file declares is the image prefix before the string tables") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    // 0x1B of root fields + an 8-byte hardpoint = 0x23 = the declared type_size.
    REQUIRE(brf_declared_size(doc) == 35u);
    REQUIRE(doc.find_block("hards")->offset + doc.find_block("hards")->width == 35u);
}

TEST_CASE("a ptr resolves to its block case-insensitively, as the loader lowercases both") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.find_block("ot_names") != nullptr);
    REQUIRE(doc.find_block("OT_NAMES") != nullptr);
    REQUIRE(doc.find_block("Shape")    != nullptr);
    REQUIRE(doc.find_block("nosuch")   == nullptr);
}

// The loader's byte/word/dword keywords consume operands while the next token is a number.
TEST_CASE("a numeric keyword consumes every operand on the line") {
    auto data = bytes("[brent's_relocatable_format]\r\n\tword 1 2 3\r\n\tend\r\n");
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.fields.size() == 3u);
    REQUIRE(doc.fields[2].value == "3");
    REQUIRE(doc.fields[2].image_offset == 4u);
    REQUIRE(doc.image_size == 6u);
}

TEST_CASE("brf_parse on empty data returns empty doc") {
    uint8_t buf[1] = {0};
    auto doc = brf_parse(buf, 0);
    REQUIRE(doc.fields.empty());
    REQUIRE(doc.blocks.empty());
    REQUIRE(doc.image_size == 0u);
}

TEST_CASE("brf_parse handles inline comments after field values") {
    auto data = bytes("[brent's_relocatable_format]\r\n\tbyte 42 ; this is a comment\r\n");
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.fields.size() == 1u);
    REQUIRE(doc.fields[0].value   == "42");
    REQUIRE(doc.fields[0].comment == "this is a comment");
}

// ---------------------------------------------------------------------------
// The WRITE path (#491 B): if a field can be written, a test must write it.
// ---------------------------------------------------------------------------

TEST_CASE("brf_set_value splices the value and disturbs nothing else on the line") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());

    // The symbol line carries the file's own comment and a tab separator. Writing the field
    // next to it must not touch either -- the GUI editor used to rebuild the whole line and
    // silently drop the comment that NAMES the class proc.
    REQUIRE(brf_set_value(doc, doc.fields[8], "_PLANEProc"));
    auto out = brf_serialize(doc);
    std::string text((const char*)out.data(), out.size());
    REQUIRE(text.find("\tsymbol _PLANEProc\t; utilProc\r\n") != std::string::npos);
    REQUIRE(text.find("_OBJProc") == std::string::npos);

    // Everything else is byte-for-byte what it was.
    std::string before(kOtDoc);
    before.replace(before.find("_OBJProc"), 8, "_PLANEProc");
    REQUIRE(text == before);

    // And the edit is visible to a re-parse.
    auto doc2 = brf_parse(out.data(), out.size());
    REQUIRE(doc2.fields[8].value == "_PLANEProc");
}

TEST_CASE("brf_set_value handles a value that changes length, on a shared line") {
    auto data = bytes("[brent's_relocatable_format]\r\n\tword 1 2 3\r\n");
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(brf_set_value(doc, doc.fields[0], "1000"));  // 1 char -> 4
    REQUIRE(brf_set_value(doc, doc.fields[2], "$ff"));   // the field after it shifted
    auto out = brf_serialize(doc);
    std::string text((const char*)out.data(), out.size());
    REQUIRE(text.find("\tword 1000 2 $ff\r\n") != std::string::npos);
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// BRF.md claimed "byte-identical for all OT/NT/PT in FA_2.LIB" and NO test opened one. It
// round-trips, and always did -- but a round-trip only proves the fields the round-trip
// reads, and this codec was silently dropping every field inside a labelled block. So the
// census asserts what the DECODE produced, against what each record says about itself.
// ---------------------------------------------------------------------------

namespace {

std::vector<uint8_t> read_all(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    std::vector<uint8_t> data((size_t)f.tellg());
    f.seekg(0);
    f.read((char*)data.data(), (std::streamsize)data.size());
    return data;
}

std::string lower_str(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Every name db/symbols/*.csv claims. The retail records name the engine functions they
// dispatch to; a name here that db/ does not know is a function we have not claimed.
std::set<std::string> db_symbol_names() {
    std::set<std::string> names;
    for (const auto& de : std::filesystem::directory_iterator(FX_DB_DIR "/symbols")) {
        if (de.path().extension() != ".csv") continue;
        std::ifstream f(de.path());
        std::string line;
        std::getline(f, line);  // header
        while (std::getline(f, line)) {
            // va,kind,name,...
            size_t a = line.find(',');
            if (a == std::string::npos) continue;
            size_t b = line.find(',', a + 1);
            if (b == std::string::npos) continue;
            size_t c = line.find(',', b + 1);
            names.insert(line.substr(b + 1, c - b - 1));
        }
    }
    return names;
}

}  // namespace

TEST_CASE("BRF decode census: every shipped record, against its own declaration") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    const std::set<std::string> db_names = db_symbol_names();
    REQUIRE_FALSE(db_names.empty());

    // All seven members of the family: the OT chain, plus the three flat records.
    const std::set<std::string> kExts = {".ot", ".nt", ".pt", ".jt", ".see", ".ecm", ".gas"};

    int total = 0, with_hards = 0, hardpoints = 0, envelopes = 0, flat = 0;
    std::set<std::string> procs;

    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower_str(de.path().extension().string()) != ".lib") continue;
        std::vector<uint8_t> lib = read_all(de.path());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            const std::string ext = lower_str(fs::path(e.name).extension().string());
            if (!kExts.count(ext)) continue;
            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            BrfDoc doc = brf_parse(rec.data(), rec.size());
            REQUIRE_FALSE(doc.fields.empty());

            // 1. The round-trip BRF.md claimed and never tested.
            REQUIRE(brf_serialize(doc) == rec);

            // 2. The image is contiguous: every field, root and block, tiles it exactly.
            uint32_t emitted = 0;
            for (const auto& f : doc.fields) {
                REQUIRE(f.image_offset == emitted);
                emitted += brf_field_width(f.type, f.value);
            }
            for (const auto& b : doc.blocks) {
                REQUIRE(b.offset == emitted);
                for (const auto& f : b.fields) {
                    REQUIRE(f.image_offset == emitted);
                    emitted += brf_field_width(f.type, f.value);
                }
            }
            REQUIRE(emitted == doc.image_size);

            // 3. The record is the image prefix the file declares as its own type_size:
            //    the root fields, plus the inline hardpoint array where there is one. This
            //    is what the old parser could not satisfy -- it stopped decoding at the
            //    first label, so its fields fell 216 bytes short of an A-10's declared 660.
            //
            //    SEE/ECM/GAS are flat records that declare no size (offset 1 is a ptr, not a
            //    word), so there is nothing to check against -- they only get the image
            //    assertions above and the relocation check below.
            const uint32_t declared = brf_declared_size(doc);
            if (declared == 0) {
                flat++;
            } else {
                uint32_t root_width = 0;
                for (const auto& f : doc.fields) root_width += brf_field_width(f.type, f.value);
                const BrfBlock* hards = doc.find_block("hards");
                uint32_t record = root_width;
                if (hards) {
                    REQUIRE(hards->offset == root_width);   // contiguous with the root fields
                    record += hards->width;
                    REQUIRE(hards->width % 24 == 0);        // 24 bytes per station
                    with_hards++;
                    hardpoints += (int)(hards->width / 24);
                }
                REQUIRE(declared == record);
                REQUIRE(record <= doc.image_size);
            }

            // 4. Every relocation resolves. An unresolved ptr would not load.
            auto check_ptrs = [&](const std::vector<BrfField>& fields) {
                for (const auto& f : fields)
                    if (f.type == "ptr") {
                        INFO("ptr " << f.value);
                        REQUIRE(doc.find_block(f.value) != nullptr);
                    }
            };
            check_ptrs(doc.fields);
            for (const auto& b : doc.blocks) check_ptrs(b.fields);

            // 5. Every `symbol` is an import: the loader resolves it against the engine's
            //    own symbol table, so the name must be one we have claimed in db/.
            for (const auto& f : doc.fields)
                if (f.type == "symbol") {
                    INFO("symbol " << f.value);
                    REQUIRE(db_names.count(f.value) == 1);
                    procs.insert(f.value);
                }

            if (doc.find_block("env")) envelopes++;
            total++;
        }
    }
    REQUIRE(total > 0);
    WARN("BRF census: " << total << " records (" << flat << " flat SEE/ECM/GAS), " << with_hards
         << " with an inline hardpoint array (" << hardpoints << " stations), " << envelopes
         << " flight envelopes, " << procs.size() << " distinct class procs");
}
