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

// A minimal OT-shaped BRF document: names table + the first general fields.
const char* kOtDoc =
    "[brent's_relocatable_format]\r\n"
    "\r\n"
    ":ot_names\r\n"
    "\tstring \"SHED\"\r\n"
    "\tstring \"Storage Shed\"\r\n"
    "\tend\r\n"
    "\r\n"
    "\tbyte 1\r\n"          // struct_type = OT
    "\tword 336\r\n"        // type_size
    "\tword 128\r\n"        // instance_size
    "\tptr ot_names\r\n"    // names
    "\tdword $100\r\n"      // ot_flags
    "\tword $100\r\n"       // obj_class = Struct
    "\tend\r\n";

// A struct_type-7 document long enough to spill into the JT extension:
// the full general section as filler words, then two projectile fields.
std::string jt_doc() {
    std::string s = "[brent's_relocatable_format]\r\n";
    s += "\tbyte 7\r\n";  // struct_type = JT
    for (int i = 1; i < OT_GENERAL_COUNT; ++i) s += "\tword 0\r\n";
    s += "\tdword $1\r\n";  // jt_flags
    s += "\tword 2\r\n";    // warhead_count
    s += "\tend\r\n";
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// Schema tables
// ---------------------------------------------------------------------------

TEST_CASE("ot schema tables pin the field order at their anchors") {
    REQUIRE(OT_GENERAL_COUNT > 0);
    CHECK(strcmp(OT_GENERAL_FIELDS[0].name, "struct_type") == 0);
    CHECK(strcmp(OT_GENERAL_FIELDS[5].name, "obj_class") == 0);
    CHECK(strcmp(OT_GENERAL_FIELDS[OT_GENERAL_COUNT - 1].name, "hud_name") == 0);

    REQUIRE(NT_COUNT > 0);
    CHECK(strcmp(NT_FIELDS[0].name, "npc_flags") == 0);
    CHECK(strcmp(NT_FIELDS[NT_COUNT - 1].name, "hards") == 0);

    REQUIRE(PT_COUNT > 0);
    CHECK(strcmp(PT_FIELDS[0].name, "pt_flags") == 0);
    CHECK(strcmp(PT_FIELDS[PT_COUNT - 1].name, "max_takeoff_weight") == 0);

    REQUIRE(JT_COUNT > 0);
    CHECK(strcmp(JT_FIELDS[0].name, "jt_flags") == 0);
    CHECK(strcmp(JT_FIELDS[JT_COUNT - 1].name, "lobe2_max_heading") == 0);

    REQUIRE(SEE_COUNT > 0);
    CHECK(strcmp(SEE_FIELDS[SEE_COUNT - 1].name, "lobe2_prob_detect") == 0);
    REQUIRE(ECM_COUNT > 0);
    CHECK(strcmp(ECM_FIELDS[ECM_COUNT - 1].name, "unk_ecm_3") == 0);
    REQUIRE(GAS_COUNT == 5);
    CHECK(strcmp(GAS_FIELDS[GAS_COUNT - 1].name, "fuel_weight") == 0);
}

TEST_CASE("ot schema tables have no null names or notes") {
    struct Table { const OtField* fields; int count; };
    const Table tables[] = {
        {OT_GENERAL_FIELDS, OT_GENERAL_COUNT}, {NT_FIELDS, NT_COUNT},
        {PT_FIELDS, PT_COUNT},                 {JT_FIELDS, JT_COUNT},
        {SEE_FIELDS, SEE_COUNT},               {ECM_FIELDS, ECM_COUNT},
        {GAS_FIELDS, GAS_COUNT},
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
    REQUIRE(doc.tables.size() == 1u);
    REQUIRE(brf_serialize(doc) == data);

    auto jt   = bytes(jt_doc());
    auto jdoc = brf_parse(jt.data(), jt.size());
    REQUIRE((int)jdoc.fields.size() == OT_GENERAL_COUNT + 2);
    REQUIRE(brf_serialize(jdoc) == jt);
}

// ---------------------------------------------------------------------------
// brf_print_info dispatch
// ---------------------------------------------------------------------------

TEST_CASE("brf_print_info labels an OT document and resolves name pointers") {
    auto data = bytes(kOtDoc);
    auto doc  = brf_parse(data.data(), data.size());
    auto out  = capture_stdout([&] { brf_print_info(doc, "ot"); });

    CHECK(contains(out, "struct_type=1"));
    CHECK(contains(out, "struct_type"));
    CHECK(contains(out, "obj_class"));
    CHECK(contains(out, "ot_names -> \"SHED\""));   // ptr resolved to its table
    CHECK(contains(out, "Pointer Tables"));
    CHECK(contains(out, "\"Storage Shed\""));
    // struct_type 1 must not grow vehicle/projectile extensions
    CHECK_FALSE(contains(out, "NT/Npc Extension"));
    CHECK_FALSE(contains(out, "PT/Plane Extension"));
    CHECK_FALSE(contains(out, "JT/Projectile Extension"));
}

TEST_CASE("brf_print_info dispatches struct_type 7 into the JT extension") {
    auto data = bytes(jt_doc());
    auto doc  = brf_parse(data.data(), data.size());
    auto out  = capture_stdout([&] { brf_print_info(doc, "jt"); });

    CHECK(contains(out, "struct_type=7"));
    CHECK(contains(out, "JT/Projectile Extension"));
    CHECK(contains(out, "jt_flags"));
    CHECK(contains(out, "warhead_count"));
    CHECK_FALSE(contains(out, "NT/Npc Extension"));
}

// ---------------------------------------------------------------------------
// Error paths
// ---------------------------------------------------------------------------

TEST_CASE("brf_print_info survives empty and schema-defying documents") {
    // Empty document: header only, nothing to label.
    BrfDoc empty;
    auto out = capture_stdout([&] { brf_print_info(empty, "ot"); });
    CHECK(contains(out, "struct_type=0"));

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
    CHECK(contains(out, "struct_type=0"));
    CHECK(contains(out, "ptr no_such_table"));
}
