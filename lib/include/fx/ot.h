#pragma once
#include "fx/brf.h"
#include <cstdint>
#include <string>
#include <vector>

// Field naming for FA object/type BRF files (.OT .NT .PT .JT .SEE .ECM .GAS).
//
// CLEAN-ROOM. Every name here comes from one of exactly two sources, and nowhere else:
//
//   1. THE FILE ITSELF. A BRF record is self-describing: it delimits its records with the
//      developers' own section names (`;--- START OF PLANE_TYPE ---`), each field's width is
//      its own type, and one field carries its own name inline (`symbol _PLANEProc ;
//      utilProc`). Section, byte offset and that comment are FACTS READ FROM THE FILE.
//
//   2. OUR OWN RECONSTRUCTION of the executable -- db/types/fa_types.h's OBJ_TYPE, recovered
//      in #454/#476 by reading the code that consumes these records (GetCurObj, OBJAdd,
//      ShapeSetup, SetupOT). Those names are keyed by BYTE OFFSET, because the byte offset is
//      what the code proves.
//
// WHAT IT NO LONGER DOES, AND WHY. This file used to carry positional name tables whose own
// header said: "Field order and names derived from OpenFA crates/asset/ot, nt, pt, jt, see,
// ecm". OpenFA is GPL; this repo is MIT, and CLAUDE.md's license boundary is explicit --
// document facts with attribution, never transcribe code across the boundary.
//
// It was also WRONG, which is how it was caught. OpenFA's field order is not FA's, so
// `fx pt info F16C.PT` labelled a `dmg_debris_pos` x/y/z triple onto fields whose real widths
// are dword/dword/word. A position triple cannot have mismatched widths.
//
// So the tables are gone. A field we have not recovered is now printed with its section, its
// offset, its type and its value, and NO NAME -- which is honest, where an invented name was
// not.

namespace fx {

// SEE / ECM / GAS are flat standalone records with no section markers, and their schemas were
// derived from THIS PROJECT'S OWN specs (SEE.md, ECM.md, GAS.md) against real files -- not from
// any other project. They stay as positional tables.
struct OtField {
    const char* name;  // human-readable name ("" = unnamed/reserved)
    const char* note;  // optional annotation (units, enum values, ...)
};

extern const OtField SEE_FIELDS[];
extern const int     SEE_COUNT;
extern const OtField ECM_FIELDS[];
extern const int     ECM_COUNT;
extern const OtField GAS_FIELDS[];
extern const int     GAS_COUNT;

// The name we have recovered for the field at `offset` within `section`, or nullptr.
// `section` is the record name the FILE declares ("OBJ_TYPE", "PLANE_TYPE", ...).
const char* brf_field_name(const std::string& section, uint32_t offset);

// An optional annotation (units, enum values) for that same field, or nullptr.
const char* brf_field_note(const std::string& section, uint32_t offset);

// Print an annotated field dump for a parsed BRF document. Sections, offsets and widths come
// from the document; names come from brf_field_name().
void brf_print_info(const BrfDoc& doc, const char* format);

}  // namespace fx
