#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Brent's Relocatable Format (BRF) -- FA object/type text format.
//
// Used by: .OT .NT .PT .JT .SEE .ECM .GAS
//
// BRF IS A TEXT DLL. The engine loads it through `LoadDLL` (0x41eb60) -- the same entry
// point it uses for a real Win32 DLL. `IsBrentDLL` (0x41e8f0) sniffs the magic line, and
// `LoadBrentDLL` (0x41f240) ASSEMBLES the file into one contiguous image:
//
//   * `byte` / `word` / `dword` emit 1 / 2 / 4 bytes at a moving cursor, and each keyword
//     consumes numbers FOR AS LONG AS THE NEXT TOKEN IS A NUMBER (`word 1 2 3` = 6 bytes).
//   * `string "text"` emits the characters plus a NUL -- so it is len+1 bytes, NOT 4.
//   * `symbol NAME` emits 4 bytes: `SMAddress(NAME)`, the address of one of the ENGINE's
//     own symbols. That is an import, resolved against the executable's symbol table.
//   * `ptr NAME` emits 4 bytes and records a relocation; `:NAME` declares a label at the
//     cursor and emits nothing. At EOF the loader allocates exactly `cursor - base` bytes
//     and back-patches every `ptr` with the address of its label. Both sides are
//     lowercased (`strlwr`), so labels resolve case-insensitively.
//   * Anything else: `ErrorExit("Unknown command '%s' in LoadBrentDLL")`.
//
// So a `:label` block is NOT "a table of strings at the bottom of the file" -- it is a
// labelled offset into the same image, and it may hold NUMERIC fields. The plane records
// use that: `:hards` is the inline hardpoint array (24 bytes per station, holding a `ptr`
// to each station's default store) and `:env` is the flight envelope. A parser that treats
// every `:label` as a string table silently DROPS both -- which is what this one did until
// #491, and no round-trip test could see it, because serialization replays raw_lines.
//
// Negative numbers are written  ^X  (meaning -X); hex is  $12bf3.

namespace fx {

// Bytes this field emits into the assembled image. `string` is variable-width (its
// characters plus a NUL), so the value is needed -- a fixed 4 is wrong for it.
inline uint32_t brf_field_width(const std::string& type, const std::string& value) {
    if (type == "byte")   return 1;
    if (type == "word")   return 2;
    if (type == "string") return (uint32_t)value.size() + 1;
    return 4;  // dword, ptr, symbol -- all 32-bit
}

struct BrfField {
    std::string type;    // "byte", "word", "dword", "ptr", "symbol", "string"
    std::string value;   // raw value token (e.g. "1", "$12bf3", "^300", "ot_names")

    // Where this field lands in the image the loader assembles. This is the engine's own
    // address for the field, and the only offset that is true across the whole file.
    uint32_t    image_offset = 0;

    // What the FILE says about this field. A BRF record is self-describing: it delimits
    // its records with the developers' own section names (`;--- START OF PLANE_TYPE ---`),
    // and each field's width is its own type. So the section and the byte offset are FACTS
    // READ FROM THE FILE, not a schema imposed on it -- which is the whole point, because a
    // schema imposed from outside is how the labels came to be wrong (see fx/ot.h).
    std::string section; // "OBJ_TYPE", "NPC_TYPE", "PLANE_TYPE", "PROJ_TYPE", or ""
    uint32_t    offset = 0;  // byte offset from the start of `section`
    std::string block;   // enclosing ":label", or "" for the root record
    std::string comment; // the file's own inline comment, if any (e.g. "utilProc")

    // Exactly where the value token sits in raw_lines, so an edit can be spliced back in
    // without disturbing the line's spacing or its comment.
    uint32_t    line = 0;       // index into BrfDoc::raw_lines
    uint32_t    value_pos = 0;  // column of the value token within that line
    uint32_t    value_len = 0;  // its length in characters
};

// A `:label` block: a named offset into the image, plus whatever the file emits there.
struct BrfBlock {
    std::string              name;
    uint32_t                 offset = 0;  // image offset the label points at
    uint32_t                 width  = 0;  // bytes emitted between this label and the next
    std::vector<std::string> strings;     // `string` values (without the outer quotes)
    std::vector<BrfField>    fields;      // every field emitted inside the block
};

struct BrfDoc {
    // Raw lines, preserved for perfect round-trip serialization.
    std::vector<std::string> raw_lines;

    // The ROOT record's fields: everything emitted before the first `:label`. These are the
    // type record proper -- `type_size` (the word at image offset 1) counts them.
    std::vector<BrfField> fields;

    // Every `:label` block, in file order.
    std::vector<BrfBlock> blocks;

    // Total bytes the loader would allocate for this file.
    uint32_t image_size = 0;

    // Resolve a `ptr` value to its block, or nullptr. Case-insensitive: the loader
    // lowercases both the label and the ptr target.
    const BrfBlock* find_block(const std::string& name) const;
};

// Parse a BRF file. Returns an empty BrfDoc (fields empty) on error.
// Always populates raw_lines even on partial parse.
BrfDoc brf_parse(const uint8_t* data, size_t size);

// Serialize back to bytes. Uses raw_lines for perfect round-trip.
std::vector<uint8_t> brf_serialize(const BrfDoc& doc);

// Write `value` into the field, splicing it into raw_lines in place -- the line's
// indentation, its separator and its trailing comment all survive. Returns false if the
// field does not belong to this doc. Offsets after a `string` edit are not recomputed;
// re-parse if you need them.
bool brf_set_value(BrfDoc& doc, const BrfField& field, const std::string& value);

// The record size the file declares for itself: the `word` at image offset 1. 0 if the
// file has no such field (SEE/ECM/GAS do not declare one).
uint32_t brf_declared_size(const BrfDoc& doc);

// Resolve a dword ^X value to a signed int64.
// E.g. "^300" -> -300, "$7fffffff" -> 2147483647, "300" -> 300.
int64_t brf_parse_int(const std::string& value);

} // namespace fx
