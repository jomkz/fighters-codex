#pragma once
#include "fx/brf.h"
#include <string>
#include <vector>

// Schema tables for FA object/type BRF files.
// These provide human-readable field names for ft ot/pt/jt/nt/see/ecm/gas info.
//
// Field order and names derived from OpenFA crates/asset/ot, nt, pt, jt, see, ecm.
// Not all versions are fully documented; fields at version boundaries are marked.

namespace fx {

// Named field descriptor for BRF info display.
struct OtField {
    const char* name;  // human-readable name (empty string = unnamed/reserved)
    const char* note;  // optional annotation (units, enum values, etc.)
};

// ---------- OT (ObjectType) general section ----------
// Struct type byte determines which extension sections follow.
// struct_type: 1=OT, 3=NT, 5=PT, 7=JT
extern const OtField OT_GENERAL_FIELDS[];  // fields 0..N-1 for the OT section
extern const int     OT_GENERAL_COUNT;

// ---------- NT (NpcType) extension section ----------
// Follows OT_GENERAL when struct_type >= 3.
extern const OtField NT_FIELDS[];
extern const int     NT_COUNT;

// ---------- PT (PlaneType) extension section ----------
// Follows NT when struct_type >= 5.
extern const OtField PT_FIELDS[];
extern const int     PT_COUNT;

// ---------- JT (ProjectileType) extension section ----------
// Follows OT_GENERAL when struct_type == 7.
extern const OtField JT_FIELDS[];
extern const int     JT_COUNT;

// ---------- SEE (Seeker/Sensor) standalone file ----------
extern const OtField SEE_FIELDS[];
extern const int     SEE_COUNT;

// ---------- ECM (Electronic Countermeasures) standalone file ----------
extern const OtField ECM_FIELDS[];
extern const int     ECM_COUNT;

// ---------- GAS (External Fuel Tank) standalone file ----------
extern const OtField GAS_FIELDS[];
extern const int     GAS_COUNT;

// Print annotated field dump for a parsed BRF document.
// format: "ot", "nt", "pt", "jt", "see", "ecm", "gas" (controls which schema to use)
void brf_print_info(const BrfDoc& doc, const char* format);

} // namespace fx
