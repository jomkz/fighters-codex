#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Brent's Relocatable Format (BRF) -- FA object/type text format.
//
// Used by: .OT .NT .PT .JT .SEE .ECM .GAS
//
// File structure:
//   [brent's_relocatable_format]
//   ; comments
//   <INDENT> <type> <value>   -- data fields (byte/word/dword/ptr/symbol/string)
//   ...
//   :<ptr_name>               -- named pointer target block
//   <INDENT> string "..."     -- strings in the block
//   <INDENT> end              -- terminates the file
//
// Negative dwords are written as:  dword ^X  (meaning -X in two's complement)
// Null pointers are written as:    dword 0   OR   ptr <name-that-resolves-to-empty>
// Hex values:                      dword $12bf3

namespace fx {

struct BrfField {
    std::string type;    // "byte", "word", "dword", "ptr", "symbol", "string"
    std::string value;   // raw value token (e.g. "1", "$12bf3", "^300", "ot_names")

    // What the FILE says about this field. A BRF record is self-describing: it delimits
    // its records with the developers' own section names (`;--- START OF PLANE_TYPE ---`),
    // and each field's width is its own type. So the section and the byte offset are FACTS
    // READ FROM THE FILE, not a schema imposed on it -- which is the whole point, because a
    // schema imposed from outside is how the labels came to be wrong (see fx/ot.h).
    std::string section; // "OBJ_TYPE", "NPC_TYPE", "PLANE_TYPE", "PROJ_TYPE", or "" 
    uint32_t    offset;  // byte offset from the start of `section`
    std::string comment; // the file's own inline comment, if any (e.g. "utilProc")
};

// Byte width of a BRF field type, as the format emits it.
inline uint32_t brf_type_size(const std::string& t) {
    if (t == "byte") return 1;
    if (t == "word") return 2;
    return 4;  // dword, ptr, symbol
}

struct BrfTable {
    std::string              name;
    std::vector<std::string> strings; // quoted string values (without outer "")
};

struct BrfDoc {
    // Raw lines, preserved for perfect round-trip serialization.
    std::vector<std::string> raw_lines;

    // Parsed data fields (in file order, skipping comments/blanks).
    std::vector<BrfField> fields;

    // Named pointer-target tables (at the bottom of the file).
    std::vector<BrfTable> tables;

    // Resolve a ptr value to its table, or nullptr if not found.
    const BrfTable* find_table(const std::string& name) const;
};

// Parse a BRF file. Returns an empty BrfDoc (fields empty) on error.
// Always populates raw_lines even on partial parse.
BrfDoc brf_parse(const uint8_t* data, size_t size);

// Serialize back to bytes. Uses raw_lines for perfect round-trip.
std::vector<uint8_t> brf_serialize(const BrfDoc& doc);

// Resolve a dword ^X value to a signed int64.
// E.g. "^300" -> -300, "$7fffffff" -> 2147483647, "300" -> 300.
int64_t brf_parse_int(const std::string& value);

} // namespace fx
