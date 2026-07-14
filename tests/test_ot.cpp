// OT schema tables + annotated info printer over BRF (#112). The tables in
// lib/src/ot.cpp are the format knowledge: field ORDER is the contract (a
// silent insert/reorder would mislabel every field `fx ot info` prints), so
// anchor fields are pinned by index alongside the print dispatch itself.
#include <catch2/catch_test_macros.hpp>
#include <fx/brf.h>
#include <fx/ot.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace fx;

namespace {

// Capture what fn() writes to stdout (brf_print_info uses printf). The
// pipe is drained only after fn() returns, so output must stay below the
// pipe buffer — the synthetic docs here print well under 64 KiB.
template <typename Fn>
std::string capture_stdout(Fn&& fn) {
    fflush(stdout);
    int fds[2];
#ifdef _WIN32
    REQUIRE(_pipe(fds, 1 << 20, _O_BINARY) == 0);
    int saved = _dup(_fileno(stdout));
    REQUIRE(saved >= 0);
    REQUIRE(_dup2(fds[1], _fileno(stdout)) == 0);
#else
    REQUIRE(pipe(fds) == 0);
    int saved = dup(fileno(stdout));
    REQUIRE(saved >= 0);
    REQUIRE(dup2(fds[1], fileno(stdout)) >= 0);
#endif
    fn();
    fflush(stdout);
#ifdef _WIN32
    _dup2(saved, _fileno(stdout));
    _close(saved);
    _close(fds[1]);
#else
    dup2(saved, fileno(stdout));
    close(saved);
    close(fds[1]);
#endif
    std::string out;
    char buf[4096];
    for (;;) {
#ifdef _WIN32
        int n = _read(fds[0], buf, sizeof buf);
#else
        ssize_t n = read(fds[0], buf, sizeof buf);
#endif
        if (n <= 0) break;
        out.append(buf, (size_t)n);
    }
#ifdef _WIN32
    _close(fds[0]);
#else
    close(fds[0]);
#endif
    return out;
}

std::vector<uint8_t> bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// A minimal OT-shaped BRF document, DELIMITING ITS OWN RECORD as the retail files do.
//
// The record comes FIRST and the `:label` blocks after it, which is the shape all 534 shipped
// records have -- and the only shape that loads. This fixture used to open with `:ot_names`,
// following BRF.md's claim that "the pointer table comes first"; the engine tokenizer says
// otherwise, and so does every real file. `end` finishes the FILE (it is the first keyword
// LoadBrentDLL tests, and it jumps to the allocate-and-finish path), so a fixture that put a
// block's `end` before the fields was describing a file the engine would load as nothing but
// two strings.
const char* kOtDoc =
    "[brent's_relocatable_format]\r\n"
    "\r\n"
    ";---------------- START OF OBJ_TYPE ----------------\r\n"
    "\tbyte 1\r\n"          // +0x00 struct_type = OT
    "\tword 336\r\n"        // +0x01 type_size
    "\tword 128\r\n"        // +0x03 obj_ext_size
    "\tptr ot_names\r\n"    // +0x05 names
    "\tdword $100\r\n"      // +0x09 type_flags
    "\tword $100\r\n"       // +0x0D obj_class
    ";---------------- END OF OBJ_TYPE ----------------\r\n"
    "\r\n"
    ":ot_names\r\n"
    "\tstring \"SHED\"\r\n"
    "\tstring \"Storage Shed\"\r\n"
    "\tend\r\n";

// A struct_type-7 document that DELIMITS ITS OWN RECORDS, exactly as the retail files do.
// The old version padded to OT_GENERAL_COUNT -- i.e. it assumed the very schema that turned
// out to be wrong. A real .JT says where OBJ_TYPE ends and PROJ_TYPE begins; so does this.
std::string jt_doc() {
    std::string s = "[brent's_relocatable_format]\r\n";
    s += ";---------------- START OF OBJ_TYPE ----------------\r\n";
    s += "\tbyte 7\r\n";    // +0x00 struct_type = JT
    s += "\tword 315\r\n";  // +0x01 type_size
    s += "\tword 52\r\n";   // +0x03 obj_ext_size
    s += ";---------------- END OF OBJ_TYPE ----------------\r\n";
    s += ";---------------- START OF PROJ_TYPE ----------------\r\n";
    s += "\tdword $1\r\n";  // +0x00 of the extension
    s += "\tword 2\r\n";    // +0x04
    s += ";---------------- END OF PROJ_TYPE ----------------\r\n";
    s += "\tend\r\n";
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// Schema tables
// ---------------------------------------------------------------------------

TEST_CASE("brf reads each field's section and offset FROM THE FILE") {
    // The record is self-describing: it names its own sections and each field's width. This
    // is what replaced the positional OpenFA tables -- structure comes from the document.
    auto jt   = bytes(jt_doc());
    auto jdoc = brf_parse(jt.data(), jt.size());
    REQUIRE(jdoc.fields.size() == 5u);

    CHECK(jdoc.fields[0].section == "OBJ_TYPE");
    CHECK(jdoc.fields[0].offset  == 0x00u);   // byte
    CHECK(jdoc.fields[1].offset  == 0x01u);   // word  -> after 1 byte
    CHECK(jdoc.fields[2].offset  == 0x03u);   // word  -> after byte+word

    // The extension is a DIFFERENT record, and its offsets restart at zero.
    CHECK(jdoc.fields[3].section == "PROJ_TYPE");
    CHECK(jdoc.fields[3].offset  == 0x00u);
    CHECK(jdoc.fields[4].offset  == 0x04u);   // after the dword
}

TEST_CASE("brf_field_name only names what we actually recovered") {
    // These come from db/types/fa_types.h -- our own reconstruction of the code that reads
    // these records (#454/#476), keyed by byte offset.
    REQUIRE(brf_field_name("OBJ_TYPE", 0x00) != nullptr);
    CHECK(strcmp(brf_field_name("OBJ_TYPE", 0x00), "struct_type") == 0);
    CHECK(strcmp(brf_field_name("OBJ_TYPE", 0x01), "type_size") == 0);
    CHECK(strcmp(brf_field_name("OBJ_TYPE", 0x03), "obj_ext_size") == 0);
    CHECK(strcmp(brf_field_name("OBJ_TYPE", 0x0F), "shape") == 0);
    CHECK(strcmp(brf_field_name("OBJ_TYPE", 0x7D), "class_proc") == 0);

    // An offset we have NOT recovered gets no name -- not a guessed one. This is the whole
    // point of the rewrite: the OpenFA tables named every field, and named them wrongly.
    CHECK(brf_field_name("OBJ_TYPE", 0x02) == nullptr);   // interior of type_size
    CHECK(brf_field_name("OBJ_TYPE", 0x40) == nullptr);

    // We have recovered NO interior of the class extensions. They alias one another in the
    // object record, and an invented name there would be exactly the old bug.
    CHECK(brf_field_name("PLANE_TYPE", 0x00) == nullptr);
    CHECK(brf_field_name("PROJ_TYPE",  0x00) == nullptr);
    CHECK(brf_field_name("NPC_TYPE",   0x00) == nullptr);
}

TEST_CASE("the flat SEE/ECM/GAS schemas have no null names or notes") {
    // These are ours -- derived from SEE.md / ECM.md / GAS.md against real files.
    struct Table { const OtField* fields; int count; };
    const Table tables[] = {
        {SEE_FIELDS, SEE_COUNT}, {ECM_FIELDS, ECM_COUNT}, {GAS_FIELDS, GAS_COUNT},
    };
    for (const auto& t : tables) {
        for (int i = 0; i < t.count; ++i) {
            INFO("field index " << i);
            REQUIRE(t.fields[i].name != nullptr);
            REQUIRE(t.fields[i].note != nullptr);
        }
    }
}

// ---------------------------------------------------------------------------
// Round-trip
// ---------------------------------------------------------------------------

TEST_CASE("ot-shaped document round-trips byte-identically through brf") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.fields.size() == 6u);
    REQUIRE(doc.blocks.size() == 1u);
    REQUIRE(brf_serialize(doc) == data);

    auto jt   = bytes(jt_doc());
    auto jdoc = brf_parse(jt.data(), jt.size());
    REQUIRE(jdoc.fields.size() == 5u);
    REQUIRE(brf_serialize(jdoc) == jt);
}

// ---------------------------------------------------------------------------
// brf_print_info dispatch
// ---------------------------------------------------------------------------

TEST_CASE("brf_print_info labels an OT document and resolves name pointers") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    auto out  = capture_stdout([&] { brf_print_info(doc, "ot"); });

    CHECK(contains(out, "--- OBJ_TYPE ---"));      // the record the FILE declares
    CHECK(contains(out, "+0x000"));                // offsets, read from the file
    CHECK(contains(out, "struct_type"));           // named from OUR reconstruction
    CHECK(contains(out, "type_size"));
    CHECK(contains(out, "obj_class"));
    CHECK(contains(out, "ot_names -> \"SHED\""));   // ptr resolved to its block
    CHECK(contains(out, "Blocks"));
    CHECK(contains(out, "\"Storage Shed\""));
    // The file declares no extension, so none is invented.
    CHECK_FALSE(contains(out, "PLANE_TYPE"));
    CHECK_FALSE(contains(out, "PROJ_TYPE"));
}

TEST_CASE("brf_print_info dispatches struct_type 7 into the JT extension") {
    auto data = bytes(jt_doc());
    auto doc  = brf_parse(data.data(), data.size());
    auto out  = capture_stdout([&] { brf_print_info(doc, "jt"); });

    // The sections come from the document, not from a schema keyed on struct_type.
    CHECK(contains(out, "--- OBJ_TYPE ---"));
    CHECK(contains(out, "--- PROJ_TYPE ---"));
    CHECK(contains(out, "obj_ext_size"));          // recovered name, OBJ_TYPE +0x03
    CHECK_FALSE(contains(out, "PLANE_TYPE"));
    // We have recovered no PROJ_TYPE interior, so its fields are shown unnamed -- never
    // with an invented label. (The old tables called these "jt_flags"/"warhead_count".)
    CHECK_FALSE(contains(out, "jt_flags"));
    CHECK_FALSE(contains(out, "warhead_count"));
}

// ---------------------------------------------------------------------------
// Error paths
// ---------------------------------------------------------------------------

TEST_CASE("brf_print_info survives empty and schema-defying documents") {
    // Empty document: header only, nothing to label.
    BrfDoc empty;
    auto out = capture_stdout([&] { brf_print_info(empty, "ot"); });
    CHECK(out.find("Pointer Tables") == std::string::npos);

    // First field is not a byte and a ptr dangles: struct_type stays 0,
    // the unresolved pointer prints raw instead of resolving.
    const char* src =
        "[brent's_relocatable_format]\r\n"
        "\tword 42\r\n"
        "\tptr no_such_table\r\n"
        "\tend\r\n";
    auto data = bytes(std::string(src));
    auto doc  = brf_parse(data.data(), data.size());
    REQUIRE(doc.fields.size() == 2u);
    out = capture_stdout([&] { brf_print_info(doc, "ot"); });
    // No section markers in this document, so no record is claimed and nothing is named --
    // the fields are still printed, with their offsets and an unresolved pointer.
    CHECK(contains(out, "(outside any record)"));
    CHECK(contains(out, "ptr no_such_table"));
}
