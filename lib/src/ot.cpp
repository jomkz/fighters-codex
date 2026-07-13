// Field naming for FA object/type BRF records. See fx/ot.h for the provenance rules.
//
// The OT / NT / PT / JT name tables that used to live here were transcribed from OpenFA
// ("Derived from OpenFA crates/asset/ot/src/lib.rs"). OpenFA is GPL and this repo is MIT, so
// they were a license-boundary violation -- and they were wrong, because OpenFA's field order
// is not FA's. They are gone.
//
// What replaces them is smaller and true: the BRF record is SELF-DESCRIBING (it declares its
// own sections and each field's width), and the names below are the ones WE recovered from
// the executable that consumes these records -- db/types/fa_types.h's OBJ_TYPE, from #454 and
// #476. They are keyed by BYTE OFFSET, because a byte offset is what the code proves. A field
// we have not recovered gets no name at all.

#include "fx/ot.h"
#include "fx/brf.h"

#include <cstdio>
#include <cstring>
#include <cstdint>

namespace fx {

namespace {

struct OffsetName {
    uint32_t    offset;
    const char* name;
    const char* note;
};

// OBJ_TYPE -- the shared type-record header every .OT/.NT/.PT/.JT begins with.
// Recovered from the executable (db/types/fa_types.h; #454, #476), NOT from any other project.
// Corroborated by the retail data itself: all 534 shipped records agree, and a static object
// (.OT) really does declare obj_ext_size = 0.
const OffsetName OBJ_TYPE_NAMES[] = {
    { 0x00, "struct_type",  "1=OT (static) 3=NT (npc/gv) 5=PT (aircraft) 7=JT (projectile)" },
    { 0x01, "type_size",    "u16 total bytes of this type record" },
    { 0x03, "obj_ext_size", "u16 bytes of class extension on the OBJECT record (object = 0xDE + this)" },
    { 0x05, "names",        "ptr -> short, long, filename" },
    { 0x09, "type_flags",   "u32 bitfield; & 0x400 = auto-remove on death" },
    { 0x0D, "obj_class",    "u16 class bitfield; high byte & 0xC0 gates the _b shape slot" },
    { 0x0F, "shape",        "ptr base shape" },
    { 0x13, "shadow_shape", "ptr the shadow shape (<name>_s.SH); the files name it shadowShape" },
    { 0x17, "shape_a",      "ptr destroyed set {A,B} -- world pass" },
    { 0x1B, "shape_b",      "ptr destroyed set {A,B} -- graphics pass" },
    { 0x25, "shape_c",      "ptr destroyed set {C,D} -- aircraft only" },
    { 0x29, "shape_d",      "ptr destroyed set {C,D} -- aircraft only" },
    { 0x33, "damage_set",   "u32 == 2 selects the {_C,_D} set" },
    { 0x7D, "class_proc",   "symbol -- the class's proc selector (the file names it: utilProc)" },
};

const OffsetName* lookup(const std::string& section, uint32_t offset) {
    // Only OBJ_TYPE is recovered. NPC_TYPE / PLANE_TYPE / PROJ_TYPE interiors are not, and an
    // invented name is worse than none -- that is exactly how the OpenFA tables went wrong.
    if (section != "OBJ_TYPE") return nullptr;
    for (const auto& e : OBJ_TYPE_NAMES)
        if (e.offset == offset) return &e;
    return nullptr;
}

}  // namespace

const char* brf_field_name(const std::string& section, uint32_t offset) {
    const OffsetName* e = lookup(section, offset);
    return e ? e->name : nullptr;
}

const char* brf_field_note(const std::string& section, uint32_t offset) {
    const OffsetName* e = lookup(section, offset);
    return e ? e->note : nullptr;
}


// ---------------------------------------------------------------------------
// SEE (Seeker/Sensor) -- standalone BRF, struct_type=10
// Derived from SEE.md and F15R.SEE example.
// ---------------------------------------------------------------------------
const OtField SEE_FIELDS[] = {
    { "struct_type",        "u8 always 10 (seeker)" },
    { "names",              "ptr -> short, long, filename strings" },
    { "capability_flags",   "u16 seeker identifier / capability flags" },
    { "seeker_subtype",     "u8 seeker sub-type" },
    { "seeker_type",        "u8 0=visual 1=laser 2=IR 3=radar" },
    { "dual_mode",          "u8 $1=search/track lobe split enabled" },
    { "track_rate",         "u8 acquisition time / track rate" },
    { "reserved_1",         "u8" },
    { "reserved_2",         "u8" },
    { "reserved_3",         "u8" },
    { "reserved_4",         "u8" },
    // Primary lobe (search / initial acquisition)
    { "lobe1_az",           "u16 azimuth half-angle (182 units/deg); $7FFF=omnidirectional" },
    { "lobe1_el",           "u16 elevation half-angle" },
    { "lobe1_min_range",    "i32 feet (^prefix = negative two's complement)" },
    { "lobe1_max_range",    "i32 feet; divide by 6076 for nautical miles" },
    { "lobe1_min_heading",  "i32 $80000000=no limit (any heading passes)" },
    { "lobe1_max_heading",  "i32 $7fffffff=no limit" },
    // Secondary lobe (track / lock-on)
    { "lobe2_az",           "u16 azimuth half-angle; narrower than lobe1 for track mode" },
    { "lobe2_el",           "u16 elevation half-angle" },
    { "lobe2_min_range",    "i32 feet" },
    { "lobe2_max_range",    "i32 feet" },
    { "lobe2_min_heading",  "i32 $80000000=no limit" },
    { "lobe2_max_heading",  "i32 $7fffffff=no limit" },
    { "lobe1_prob_detect",  "u8 probability of detection % (100=always)" },
    { "lobe2_prob_detect",  "u8 probability of detection %" },
};
const int SEE_COUNT = (int)(sizeof(SEE_FIELDS) / sizeof(SEE_FIELDS[0]));

// ---------------------------------------------------------------------------
// ECM (Electronic Countermeasures) -- standalone BRF, struct_type=9
// Derived from ECM.md and F15.ECM example.
// ---------------------------------------------------------------------------
const OtField ECM_FIELDS[] = {
    { "struct_type",        "u8 always 9 (ECM)" },
    { "names",              "ptr -> short, long, filename strings" },
    { "quantity",           "u16 0=built-in suite; N=pod carrying capacity" },
    { "category",           "u8 0=built-in 1=external pod" },
    { "ecm_power",          "u16 bitmask: $10=radar jammer $100=IR jammer; $0=passive only" },
    { "chaff_eff",          "u8 chaff effectiveness (0=no chaff dispensers)" },
    { "radar_scale",        "u8 radar jammer effect scale (passed to MakeObjRotationMatrix)" },
    { "radar_az",           "u8 radar jammer azimuth half-angle (valueÃ—256 = fixed-point deg)" },
    { "radar_el",           "u8 radar jammer elevation half-angle (valueÃ—256)" },
    { "flare_eff",          "u8 flare effectiveness (0=no flare dispensers)" },
    { "ir_scale",           "u8 IR jammer effect scale" },
    { "ir_az",              "u8 IR jammer azimuth half-angle (valueÃ—256)" },
    { "ir_el",              "u8 IR jammer elevation half-angle (valueÃ—256)" },
    { "radar_pk_red",       "u8 radar Pk reduction % (Pk_final = (100-byte)Ã—Pk_base/100)" },
    { "radar_strength",     "u16 overall radar ECM strength (100=full 0=none)" },
    { "unk_ecm_1",          "u8" },
    { "unk_ecm_2",          "u8" },
    { "ir_pk_red",          "u8 IR Pk reduction % applied against IR-guided weapons" },
    { "ir_strength",        "u16 overall IR ECM strength" },
    { "unk_ecm_3",          "u8" },
};
const int ECM_COUNT = (int)(sizeof(ECM_FIELDS) / sizeof(ECM_FIELDS[0]));

// ---------------------------------------------------------------------------
// GAS (External Fuel Tank) -- standalone BRF, struct_type=8
// Derived from GAS.md; only 4 files exist (F150/F250/F350/F500.GAS).
// ---------------------------------------------------------------------------
const OtField GAS_FIELDS[] = {
    { "struct_type",        "u8 always 8 (fuel tank)" },
    { "names",              "ptr -> short, long, filename strings" },
    { "empty_weight",       "u16 lbs â€” tank structural weight when empty" },
    { "flags",              "u8 always $1 (fuel-tank category flag)" },
    { "fuel_weight",        "u32 lbs â€” usable fuel weight when full (JP-8: 6.6 lb/US gal)" },
};
const int GAS_COUNT = (int)(sizeof(GAS_FIELDS) / sizeof(GAS_FIELDS[0]));

// ---------------------------------------------------------------------------
// Info printer
// ---------------------------------------------------------------------------
//
// Sections, offsets and widths come from the DOCUMENT (the file declares them). Names come
// from brf_field_name(), i.e. from our own reconstruction. Nothing is inferred from position.

static void print_value(const BrfField& f, const BrfDoc& doc) {
    if (f.type == "ptr") {
        const BrfTable* tbl = doc.find_table(f.value);
        if (tbl && !tbl->strings.empty())
            printf("%s -> \"%s\"", f.value.c_str(), tbl->strings[0].c_str());
        else
            printf("ptr %s", f.value.c_str());
    } else if (f.type == "symbol") {
        printf("%s", f.value.c_str());
    } else {
        int64_t ival = brf_parse_int(f.value);
        printf("%-12s (%lld)", f.value.c_str(), (long long)ival);
    }
}

// The self-describing path: .OT / .NT / .PT / .JT, which delimit their own records.
static void print_sectioned(const BrfDoc& doc) {
    std::string cur = "\x01";  // impossible, forces a header on the first field
    for (size_t i = 0; i < doc.fields.size(); i++) {
        const BrfField& f = doc.fields[i];
        if (f.section != cur) {
            cur = f.section;
            printf("\n--- %s ---\n", cur.empty() ? "(outside any record)" : cur.c_str());
        }
        const char* name = brf_field_name(f.section, f.offset);
        const char* note = brf_field_note(f.section, f.offset);

        // A name the FILE gives us beats silence: it names utilProc itself.
        if (!name && !f.comment.empty()) name = f.comment.c_str();

        printf("  +0x%03X  %-6s %-16s ", f.offset, f.type.c_str(), name ? name : "");
        print_value(f, doc);
        if (note && note[0]) printf("  ; %s", note);
        printf("\n");
    }
}

// SEE / ECM / GAS are flat standalone records with no section markers, and their schemas were
// derived from THIS project's own specs (SEE.md, ECM.md, GAS.md) -- so they stay positional.
static void print_flat(const BrfDoc& doc, const OtField* schema, int count) {
    for (size_t i = 0; i < doc.fields.size(); i++) {
        const BrfField& f = doc.fields[i];
        const char* name = ((int)i < count && schema[i].name[0]) ? schema[i].name : "";
        const char* note = ((int)i < count) ? schema[i].note : "";
        printf("  [%3d] %-24s %s ", (int)i, name, name[0] ? "=" : " ");
        print_value(f, doc);
        if (note && note[0]) printf("  ; %s", note);
        printf("\n");
    }
}

void brf_print_info(const BrfDoc& doc, const char* format) {
    if (strcmp(format, "see") == 0)      print_flat(doc, SEE_FIELDS, SEE_COUNT);
    else if (strcmp(format, "ecm") == 0) print_flat(doc, ECM_FIELDS, ECM_COUNT);
    else if (strcmp(format, "gas") == 0) print_flat(doc, GAS_FIELDS, GAS_COUNT);
    else                                 print_sectioned(doc);

    if (!doc.tables.empty()) {
        printf("\n--- Pointer Tables ---\n");
        for (auto& t : doc.tables) {
            printf("  :%s\n", t.name.c_str());
            for (auto& s : t.strings)
                printf("    \"%s\"\n", s.c_str());
        }
    }
}

}  // namespace fx
